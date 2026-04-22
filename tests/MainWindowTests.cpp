#include <QtTest>

#include <QAction>
#include <QLineEdit>
#include <QListView>
#include <QScrollBar>
#include <QStyleOptionViewItem>

#include "ui/mainwindow/LogItemDelegate.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/models/LogListModel.h"

class MainWindowTests : public QObject {
    Q_OBJECT

private slots:
    void appendLogKeepsBottomWhenCursorAtLastLine();
    void appendLogPreservesViewportWhenCursorNotAtLastLine();
    void appendLogPreservesViewportWhenFilterActive();
    void globalHotkeySettingsActionEmitsSignal();
    void logDelegateUsesViewportWidthForSingleLineHeight();
    void logDelegateKeepsReportedSingleLineAtWideWidth();
    void logViewRelayoutsItemHeightAfterResize();
};

namespace {

void appendManyLogs(MainWindow& window, int count, const QString& prefix)
{
    for (int index = 0; index < count; ++index) {
        window.appendLog(QStringLiteral("%1 %2").arg(prefix).arg(index));
    }
    QCoreApplication::processEvents();
}

void scrollToMiddle(QListView* logView)
{
    QVERIFY(logView != nullptr);
    QAbstractItemModel* model = logView->model();
    QVERIFY(model != nullptr);
    const int middleRow = qMax(0, model->rowCount() / 2);
    logView->scrollTo(model->index(middleRow, 0), QAbstractItemView::PositionAtTop);
    QCoreApplication::processEvents();
}

} // namespace

void MainWindowTests::appendLogKeepsBottomWhenCursorAtLastLine()
{
    MainWindow window;
    window.resize(1000, 640);
    window.show();
    QCoreApplication::processEvents();

    appendManyLogs(window, 200, QStringLiteral("tail"));

    auto* logView = window.findChild<QListView*>(QStringLiteral("logView"));
    QVERIFY(logView != nullptr);

    logView->verticalScrollBar()->setValue(logView->verticalScrollBar()->maximum());
    QCoreApplication::processEvents();

    window.appendLog(QStringLiteral("tail follow"));
    QCoreApplication::processEvents();

    QTRY_COMPARE(logView->verticalScrollBar()->value(), logView->verticalScrollBar()->maximum());
}

void MainWindowTests::appendLogPreservesViewportWhenCursorNotAtLastLine()
{
    MainWindow window;
    window.resize(1000, 640);
    window.show();
    QCoreApplication::processEvents();

    appendManyLogs(window, 200, QStringLiteral("keep"));

    auto* logView = window.findChild<QListView*>(QStringLiteral("logView"));
    QVERIFY(logView != nullptr);

    QScrollBar* scrollBar = logView->verticalScrollBar();
    scrollToMiddle(logView);
    const int preservedValue = scrollBar->value();

    window.appendLog(QStringLiteral("keep viewport"));
    QCoreApplication::processEvents();

    QTRY_COMPARE(scrollBar->value(), preservedValue);
}

void MainWindowTests::appendLogPreservesViewportWhenFilterActive()
{
    MainWindow window;
    window.resize(1000, 640);
    window.show();
    QCoreApplication::processEvents();

    appendManyLogs(window, 200, QStringLiteral("match"));

    auto* logView = window.findChild<QListView*>(QStringLiteral("logView"));
    auto* filterEdit = window.findChild<QLineEdit*>(QStringLiteral("logFilterEdit"));
    QVERIFY(logView != nullptr);
    QVERIFY(filterEdit != nullptr);

    filterEdit->setText(QStringLiteral("match"));
    QCoreApplication::processEvents();

    QScrollBar* scrollBar = logView->verticalScrollBar();
    scrollToMiddle(logView);
    const int preservedValue = scrollBar->value();

    window.appendLog(QStringLiteral("match filtered"));
    QCoreApplication::processEvents();

    QTRY_COMPARE(scrollBar->value(), preservedValue);
}

void MainWindowTests::globalHotkeySettingsActionEmitsSignal()
{
    MainWindow window;
    QSignalSpy spy(&window, SIGNAL(globalHotkeySettingsRequested()));
    QVERIFY(spy.isValid());

    auto* action = window.findChild<QAction*>(QStringLiteral("globalHotkeySettingsAction"));
    QVERIFY(action != nullptr);

    action->trigger();

    QCOMPARE(spy.count(), 1);
}

void MainWindowTests::logDelegateUsesViewportWidthForSingleLineHeight()
{
    const QString text = QStringLiteral("alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu nu xi omicron");

    LogListModel model;
    QListView view;
    view.resize(900, 200);
    view.show();
    QCoreApplication::processEvents();
    model.setWrapConfiguration(view.font(), view.viewport()->width() - 2 * LogItemDelegate::HorizontalPadding);
    model.appendLine(text);

    LogItemDelegate delegate;
    QStyleOptionViewItem option;
    option.initFrom(&view);
    option.fontMetrics = view.fontMetrics();

    const QSize wideHint = delegate.sizeHint(option, model.index(0, 0));

    model.setWrapConfiguration(view.font(), 80 - 2 * LogItemDelegate::HorizontalPadding);
    const QSize narrowHint = delegate.sizeHint(option, model.index(0, 0));

    QVERIFY(wideHint.height() < narrowHint.height());
}

void MainWindowTests::logDelegateKeepsReportedSingleLineAtWideWidth()
{
    const QString text = QStringLiteral("[16:27:54] 2026/04/09 16:27:54.067987 [Info] infra/conf/serial: Reading config: &{Name:C:\\Users\\atom.DELL\\AppData\\Local\\v2rayq\\v2rayq\\config.generated.json Format:json}");

    LogListModel model;
    QListView view;
    view.resize(996, 200);
    view.show();
    QCoreApplication::processEvents();
    model.setWrapConfiguration(view.font(), view.viewport()->width() - 2 * LogItemDelegate::HorizontalPadding);
    model.appendLine(text);

    LogItemDelegate delegate;
    QStyleOptionViewItem option;
    option.initFrom(&view);
    option.fontMetrics = view.fontMetrics();

    const QSize hint = delegate.sizeHint(option, model.index(0, 0));
    const int singleLineHeight = option.fontMetrics.height() + 2;
    QCOMPARE(hint.height(), singleLineHeight);
}

void MainWindowTests::logViewRelayoutsItemHeightAfterResize()
{
    const QString text = QStringLiteral("[16:27:54] 2026/04/09 16:27:54.067987 [Info] infra/conf/serial: Reading config: &{Name:C:\\Users\\atom.DELL\\AppData\\Local\\v2rayq\\v2rayq\\config.generated.json Format:json}");

    LogListModel model;
    QListView view;
    view.resize(220, 200);
    view.show();
    QCoreApplication::processEvents();
    model.setWrapConfiguration(view.font(), view.viewport()->width() - 2 * LogItemDelegate::HorizontalPadding);
    model.appendLine(text);

    LogItemDelegate delegate;
    QStyleOptionViewItem option;
    option.initFrom(&view);
    option.fontMetrics = view.fontMetrics();

    const QSize narrowHint = delegate.sizeHint(option, model.index(0, 0));

    view.resize(996, 200);
    QCoreApplication::processEvents();
    model.setWrapConfiguration(view.font(), view.viewport()->width() - 2 * LogItemDelegate::HorizontalPadding);

    const QSize wideHint = delegate.sizeHint(option, model.index(0, 0));
    QVERIFY(wideHint.height() < narrowHint.height());
}

QTEST_MAIN(MainWindowTests)

#include "MainWindowTests.moc"
