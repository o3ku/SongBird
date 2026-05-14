#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QFontMetrics>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QMenu>
#include <QScrollBar>
#include <QSplitter>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QTextOption>
#include <QToolBar>
#include <QToolButton>

#include "domain/models/Config.h"
#include "subscription/ShareUrlBuilder.h"
#include "ui/mainwindow/LogItemDelegate.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/models/LogListModel.h"
#include "ui/qr/QrCodeRenderer.h"

class MainWindowTests : public QObject {
    Q_OBJECT

private slots:
    void appendLogKeepsBottomWhenCursorAtLastLine();
    void appendLogPreservesViewportWhenCursorNotAtLastLine();
    void appendLogPreservesViewportWhenFilterActive();
    void logStickToBottomButtonMatchesFilterHeight();
    void globalHotkeySettingsActionEmitsSignal();
    void toolbarReplacesReloadAndProxyModeComboWithToggleButtons();
    void toolbarRemovesServerButton();
    void toolbarUsesFullWidthLayoutAndCompactVerticalMargins();
    void sharePanelShowsSelectedServerShareLink();
    void shareLinkTextEditDoesNotForceTallMinimumHeight();
    void qrCodeRendererRemovesQuietZone();
    void coreToggleButtonUsesCheckedStateAndDisablesDuringTransition();
    void coreToggleButtonShowsNoServerTooltipWhenNoServersExist();
    void coreToggleButtonShowsMissingCoreTooltipWhenRequiredCoreIsUnavailable();
    void proxyToggleButtonUsesCheckedStateAndEmitsSignals();
    void restoreAndCaptureUiStatePreservesRuntimeToggles();
    void logDelegateUsesViewportWidthForSingleLineHeight();
    void logDelegateKeepsReportedSingleLineAtWideWidth();
    void logViewRelayoutsItemHeightAfterResize();
    void enterOnServerTableSetsCurrentWithoutMovingSelection();
    void speedTestRefreshKeepsCurrentSelectionOnTriggeredRow();
    void speedTestResultUpdateKeepsSelectionOnSameServerWhenSorted();
    void serverContextMenuKeepsExistingMultiSelection();
    void serverContextMenuSelectsClickedUnselectedRow();
    void copySubscriptionUrlActionCopiesCurrentSubscriptionUrl();
    void statusLabelsUseThemePropertiesInsteadOfInlineStyleSheets();
    void compactUiZonesDoNotExceedServerTableFont();
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

VmessItem createServer(const QString& id, const QString& remarks, int sort)
{
    VmessItem item;
    item.indexId = id;
    item.remarks = remarks;
    item.address = QStringLiteral("127.0.0.%1").arg(sort);
    item.port = 10000 + sort;
    item.configType = ConfigType::VMess;
    item.sort = sort;
    return item;
}

Config createServerSelectionConfig()
{
    Config config;
    config.servers = {
        createServer(QStringLiteral("server-1"), QStringLiteral("First"), 1),
        createServer(QStringLiteral("server-2"), QStringLiteral("Second"), 2),
        createServer(QStringLiteral("server-3"), QStringLiteral("Third"), 3)};
    config.currentIndexId = QStringLiteral("server-1");
    return config;
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

void MainWindowTests::logStickToBottomButtonMatchesFilterHeight()
{
    MainWindow window;
    window.resize(1000, 640);
    window.show();
    QCoreApplication::processEvents();

    auto* filterEdit = window.findChild<QLineEdit*>(QStringLiteral("logFilterEdit"));
    auto* stickButton = window.findChild<QToolButton*>(QStringLiteral("logStickToBottomButton"));
    QVERIFY(filterEdit != nullptr);
    QVERIFY(stickButton != nullptr);

    QCOMPARE(stickButton->height(), filterEdit->height());
    QCOMPARE(stickButton->width(), stickButton->height());
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

void MainWindowTests::toolbarReplacesReloadAndProxyModeComboWithToggleButtons()
{
    MainWindow window;

    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("reloadButton")) == nullptr);
    QVERIFY(window.findChild<QComboBox*>(QStringLiteral("systemProxyModeCombo")) == nullptr);
    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("coreToggleButton")) != nullptr);
    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton")) != nullptr);
    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("stopCoreButton")) == nullptr);
    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("proxyOffButton")) == nullptr);
}

void MainWindowTests::toolbarRemovesServerButton()
{
    MainWindow window;

    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("serverButton")) == nullptr);
}

void MainWindowTests::toolbarUsesFullWidthLayoutAndCompactVerticalMargins()
{
    MainWindow window;

    auto* toolBar = window.findChild<QToolBar*>(QStringLiteral("mainToolBar"));
    QVERIFY(toolBar != nullptr);
    QCOMPARE(toolBar->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);

    const QMargins margins = toolBar->contentsMargins();
    QCOMPARE(margins.left(), 0);
    QCOMPARE(margins.top(), 2);
    QCOMPARE(margins.right(), 0);
    QCOMPARE(margins.bottom(), 2);
}

void MainWindowTests::sharePanelShowsSelectedServerShareLink()
{
    MainWindow window;
    window.resize(1000, 640);
    Config config = createServerSelectionConfig();
    config.mainQrPreviewVisible = true;
    window.restoreUiState(config);
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* shareButton = window.findChild<QToolButton*>(QStringLiteral("qrCodeButton"));
    auto* shareAction = window.findChild<QAction*>(QStringLiteral("toggleQrPanelAction"));
    auto* qrPanel = window.findChild<QWidget*>(QStringLiteral("qrPanel"));
    auto* shareContentPanel = window.findChild<QWidget*>(QStringLiteral("shareContentPanel"));
    auto* qrPlaceholder = window.findChild<QLabel*>(QStringLiteral("qrPlaceholder"));
    auto* shareLinkLabel = window.findChild<QPlainTextEdit*>(QStringLiteral("shareLinkLabel"));
    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(shareButton != nullptr);
    QVERIFY(shareAction != nullptr);
    QVERIFY(qrPanel != nullptr);
    QVERIFY(shareContentPanel != nullptr);
    QVERIFY(qrPlaceholder != nullptr);
    QVERIFY(shareLinkLabel != nullptr);
    QVERIFY(serverView != nullptr);
    shareAction->trigger();
    QCoreApplication::processEvents();
    auto* shareContentLayout = qobject_cast<QBoxLayout*>(shareContentPanel->layout());
    QVERIFY(shareContentLayout != nullptr);
    QCOMPARE(shareButton->text(), QStringLiteral("Share"));
    QCOMPARE(shareAction->text(), QStringLiteral("Share"));
    QCOMPARE(shareLinkLabel->toPlainText(), ShareUrlBuilder::build(config.servers.constFirst()));
    QCOMPARE(shareContentPanel->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(shareContentLayout->count(), 2);
    QCOMPARE(shareContentLayout->contentsMargins().top(), 0);
    QCOMPARE(shareContentLayout->contentsMargins().bottom(), 0);
    QCOMPARE(shareContentLayout->stretch(0), 1);
    QCOMPARE(shareContentLayout->stretch(1), 0);
    QCOMPARE(qrPlaceholder->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(qrPlaceholder->margin(), 10);
    QVERIFY(qrPlaceholder->height() <= qrPlaceholder->width());
    QTRY_VERIFY(!qrPlaceholder->pixmap().isNull());
    const QPixmap qrPixmap = qrPlaceholder->pixmap();
    const qreal qrPixmapDpr = qrPixmap.devicePixelRatio() <= 0.0 ? qreal(1.0) : qrPixmap.devicePixelRatio();
    const int qrPixmapLogicalWidth = qRound(qrPixmap.width() / qrPixmapDpr);
    const int qrPixmapLogicalHeight = qRound(qrPixmap.height() / qrPixmapDpr);
    QVERIFY(qrPixmapLogicalWidth <= qrPlaceholder->width() - (qrPlaceholder->margin() * 2));
    QVERIFY(qrPixmapLogicalHeight <= qrPlaceholder->height() - (qrPlaceholder->margin() * 2));
    QCOMPARE(shareLinkLabel->sizePolicy().verticalPolicy(), QSizePolicy::Preferred);
    QCOMPARE(shareLinkLabel->wordWrapMode(), QTextOption::WrapAnywhere);
    QCOMPARE(shareLinkLabel->property("shareLinkBottomPadding").toInt(), 10);
}

void MainWindowTests::shareLinkTextEditDoesNotForceTallMinimumHeight()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.mainQrPreviewVisible = true;
    window.resize(1000, 640);
    window.restoreUiState(config);
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* shareLinkLabel = window.findChild<QPlainTextEdit*>(QStringLiteral("shareLinkLabel"));
    QVERIFY(shareLinkLabel != nullptr);

    shareLinkLabel->setPlainText(
        QStringLiteral("vless://83e96db9-34ec-4d85-ac13-e37e44006aa8@45.196.236.198:38906?"
                       "encryption=none&flow=xtls-rprx-vision&security=reality&sni=www.icloud.com&"
                       "fp=safari&pbk=CidZrpFUjWxsMWvKyrT6sf4uSzLwTnnbcqZp1b6x7FI&sid=4fe22a&spx=%2F&"
                       "type=tcp&headerType=none#HK-05"));
    QCoreApplication::processEvents();

    QVERIFY(shareLinkLabel->sizeHint().height() > shareLinkLabel->minimumSizeHint().height());
    QVERIFY(shareLinkLabel->minimumSizeHint().height()
            <= shareLinkLabel->fontMetrics().lineSpacing() + 10);
}

void MainWindowTests::qrCodeRendererRemovesQuietZone()
{
    const QPixmap pixmap = QrCodeRenderer::render(QStringLiteral("vmess://share-test"), 256);
    QVERIFY(!pixmap.isNull());

    const QImage image = pixmap.toImage();
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::black));
    QCOMPARE(image.pixelColor(image.width() - 1, 0), QColor(Qt::black));
    QCOMPARE(image.pixelColor(0, image.height() - 1), QColor(Qt::black));
}

void MainWindowTests::coreToggleButtonUsesCheckedStateAndDisablesDuringTransition()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);

    auto* coreButton = window.findChild<QToolButton*>(QStringLiteral("coreToggleButton"));
    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(coreButton != nullptr);
    QVERIFY(proxyButton != nullptr);
    QCOMPARE(coreButton->text(), QStringLiteral("START"));
    QVERIFY(!coreButton->isChecked());

    QSignalSpy startSpy(&window, SIGNAL(startCoreRequested()));
    QSignalSpy stopSpy(&window, SIGNAL(stopCoreRequested()));
    QVERIFY(startSpy.isValid());
    QVERIFY(stopSpy.isValid());

    QVERIFY(window.findChild<QAction*>(QStringLiteral("coreToggleAction")) != nullptr);

    coreButton->click();
    QCoreApplication::processEvents();

    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(stopSpy.count(), 0);
    QVERIFY(coreButton->isEnabled());

    window.setCoreRunning(true, true);
    QCoreApplication::processEvents();
    QCOMPARE(coreButton->text(), QStringLiteral("START"));
    QVERIFY(!coreButton->isChecked());
    QVERIFY(!coreButton->isEnabled());
    QVERIFY(!proxyButton->isEnabled());

    window.setCoreRunning(true, false);
    QCoreApplication::processEvents();
    QVERIFY(coreButton->isEnabled());
    QVERIFY(coreButton->isChecked());
    QVERIFY(proxyButton->isEnabled());

    coreButton->click();
    QCoreApplication::processEvents();

    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(stopSpy.count(), 1);
}

void MainWindowTests::coreToggleButtonShowsNoServerTooltipWhenNoServersExist()
{
    MainWindow window;
    window.setConfig(Config());

    auto* coreButton = window.findChild<QToolButton*>(QStringLiteral("coreToggleButton"));
    QVERIFY(coreButton != nullptr);
    QVERIFY(!coreButton->isEnabled());
    QCOMPARE(coreButton->toolTip(), QStringLiteral("No available server. Add or import a server first."));
}

void MainWindowTests::coreToggleButtonShowsMissingCoreTooltipWhenRequiredCoreIsUnavailable()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.coreTypeItems = {
        CoreTypeItem{static_cast<int>(ConfigType::VMess), static_cast<int>(CoreType::Xray)}};
    window.setConfig(config);
    window.setExistingCoreTypes({CoreType::SingBox});

    auto* coreButton = window.findChild<QToolButton*>(QStringLiteral("coreToggleButton"));
    QVERIFY(coreButton != nullptr);
    QVERIFY(coreButton->isEnabled());
    QVERIFY(coreButton->toolTip().contains(QStringLiteral("No compatible Xray core is installed")));
    QVERIFY(coreButton->toolTip().contains(QStringLiteral("First")));
}

void MainWindowTests::proxyToggleButtonUsesCheckedStateAndEmitsSignals()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.setCoreRunning(true, false);
    QCoreApplication::processEvents();

    QSignalSpy enableSpy(&window, SIGNAL(enableSystemProxyRequested()));
    QSignalSpy disableSpy(&window, SIGNAL(disableSystemProxyRequested()));
    QVERIFY(enableSpy.isValid());
    QVERIFY(disableSpy.isValid());

    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(proxyButton != nullptr);
    QCOMPARE(proxyButton->text(), QStringLiteral("PROXY"));
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(proxyButton->isEnabled());

    proxyButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(enableSpy.count(), 1);

    window.setProxyEnabled(true);
    QCoreApplication::processEvents();
    QCOMPARE(proxyButton->text(), QStringLiteral("PROXY"));
    QVERIFY(proxyButton->isChecked());

    proxyButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(disableSpy.count(), 1);
}

void MainWindowTests::restoreAndCaptureUiStatePreservesRuntimeToggles()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.mainCoreRunning = true;
    config.mainProxyEnabled = true;

    window.setConfig(config);
    window.restoreUiState(config);
    QCoreApplication::processEvents();

    auto* coreButton = window.findChild<QToolButton*>(QStringLiteral("coreToggleButton"));
    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(coreButton != nullptr);
    QVERIFY(proxyButton != nullptr);
    QVERIFY(coreButton->isChecked());
    QVERIFY(proxyButton->isChecked());

    Config captured;
    window.captureUiState(captured);

    QVERIFY(captured.mainCoreRunning);
    QVERIFY(captured.mainProxyEnabled);
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

void MainWindowTests::enterOnServerTableSetsCurrentWithoutMovingSelection()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);

    serverView->setFocus();
    serverView->selectRow(1);
    serverView->setCurrentIndex(serverView->model()->index(1, 2));
    QCoreApplication::processEvents();

    QSignalSpy spy(&window, SIGNAL(setDefaultServerRequested(QString)));
    QVERIFY(spy.isValid());
    connect(&window, &MainWindow::setDefaultServerRequested, &window, [&window, &config](const QString& indexId) {
        config.currentIndexId = indexId;
        window.setConfig(config);
    });

    QTest::keyClick(serverView, Qt::Key_Return);
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("server-2"));
    QCOMPARE(serverView->currentIndex().row(), 1);
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));

    spy.clear();
    QTest::keyClick(serverView, Qt::Key_Enter);
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("server-2"));
    QCOMPARE(serverView->currentIndex().row(), 1);
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));
}

void MainWindowTests::speedTestRefreshKeepsCurrentSelectionOnTriggeredRow()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);

    serverView->setFocus();
    serverView->selectRow(1);
    serverView->setCurrentIndex(serverView->model()->index(1, 2));
    QCoreApplication::processEvents();

    QSignalSpy spy(&window, SIGNAL(testServersRequested(QStringList)));
    QVERIFY(spy.isValid());

    auto* action = window.findChild<QAction*>(QStringLiteral("testAction"));
    QVERIFY(action != nullptr);
    action->trigger();
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    const QStringList requestedIds = spy.takeFirst().at(0).toStringList();
    QCOMPARE(requestedIds, QStringList{QStringLiteral("server-2")});

    config.servers[1].testResult = QStringLiteral("...");
    window.setConfig(config);
    QCoreApplication::processEvents();

    QCOMPARE(serverView->currentIndex().row(), 1);
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));
}

void MainWindowTests::speedTestResultUpdateKeepsSelectionOnSameServerWhenSorted()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.servers[0].testResult = QStringLiteral("20 ms");
    config.servers[1].testResult = QStringLiteral("30 ms");
    config.servers[2].testResult = QStringLiteral("10 ms");
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);

    serverView->sortByColumn(7, Qt::AscendingOrder);
    QCoreApplication::processEvents();

    int selectedRow = -1;
    for (int row = 0; row < serverView->model()->rowCount(); ++row) {
        const QString display = serverView->model()->index(row, 2).data(Qt::DisplayRole).toString();
        if (display == QStringLiteral("Second")) {
            selectedRow = row;
            break;
        }
    }
    QVERIFY(selectedRow >= 0);

    serverView->setFocus();
    serverView->selectRow(selectedRow);
    serverView->setCurrentIndex(serverView->model()->index(selectedRow, 2));
    QCoreApplication::processEvents();

    window.updateServerTestResult(QStringLiteral("server-2"), QStringLiteral("5 ms"));
    QCoreApplication::processEvents();

    const QModelIndex currentIndex = serverView->currentIndex();
    QVERIFY(currentIndex.isValid());
    QCOMPARE(currentIndex.data(Qt::DisplayRole).toString(), QStringLiteral("Second"));
    QVERIFY(serverView->selectionModel()->isRowSelected(currentIndex.row(), QModelIndex()));
}

void MainWindowTests::serverContextMenuKeepsExistingMultiSelection()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);

    const QModelIndex firstIndex = serverView->model()->index(0, 0);
    const QModelIndex secondIndex = serverView->model()->index(1, 0);
    QVERIFY(firstIndex.isValid());
    QVERIFY(secondIndex.isValid());

    serverView->selectionModel()->select(
        QItemSelection(firstIndex, secondIndex),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QCoreApplication::processEvents();

    QVERIFY(serverView->selectionModel()->isRowSelected(0, QModelIndex()));
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));

    const QPoint menuPoint = serverView->visualRect(secondIndex).center();

    QTimer::singleShot(0, []() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (auto* menu = qobject_cast<QMenu*>(widget)) {
                menu->close();
            }
        }
    });

    QMetaObject::invokeMethod(
        &window,
        "showServerContextMenu",
        Qt::DirectConnection,
        Q_ARG(QPoint, menuPoint));
    QCoreApplication::processEvents();

    QVERIFY(serverView->selectionModel()->isRowSelected(0, QModelIndex()));
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));
    QCOMPARE(serverView->selectionModel()->selectedRows().size(), 2);
}

void MainWindowTests::serverContextMenuSelectsClickedUnselectedRow()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);

    const QModelIndex firstIndex = serverView->model()->index(0, 0);
    const QModelIndex secondIndex = serverView->model()->index(1, 0);
    QVERIFY(firstIndex.isValid());
    QVERIFY(secondIndex.isValid());

    serverView->selectionModel()->select(
        QItemSelection(firstIndex, firstIndex),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QCoreApplication::processEvents();

    QVERIFY(serverView->selectionModel()->isRowSelected(0, QModelIndex()));
    QVERIFY(!serverView->selectionModel()->isRowSelected(1, QModelIndex()));

    const QPoint menuPoint = serverView->visualRect(secondIndex).center();

    QTimer::singleShot(0, []() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (auto* menu = qobject_cast<QMenu*>(widget)) {
                menu->close();
            }
        }
    });

    QMetaObject::invokeMethod(
        &window,
        "showServerContextMenu",
        Qt::DirectConnection,
        Q_ARG(QPoint, menuPoint));
    QCoreApplication::processEvents();

    QVERIFY(!serverView->selectionModel()->isRowSelected(0, QModelIndex()));
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));
    QCOMPARE(serverView->selectionModel()->selectedRows().size(), 1);
}

void MainWindowTests::copySubscriptionUrlActionCopiesCurrentSubscriptionUrl()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.subscriptions = {SubItem{
        QStringLiteral("sub-1"),
        QStringLiteral("Test Sub"),
        QStringLiteral("https://example.com/subscription"),
        true,
        QStringLiteral("test-agent")}};
    config.mainSelectedSubId = QStringLiteral("sub-1");
    window.restoreUiState(config);
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    QVERIFY(window.selectSubscriptionTab(QStringLiteral("sub-1")));

    auto* action = window.findChild<QAction*>(QStringLiteral("copySubscriptionUrlAction"));
    QVERIFY(action != nullptr);
    QVERIFY(QApplication::clipboard() != nullptr);

    QApplication::clipboard()->clear();
    action->trigger();
    QCoreApplication::processEvents();

    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("https://example.com/subscription"));
}

void MainWindowTests::statusLabelsUseThemePropertiesInsteadOfInlineStyleSheets()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.autoRunEnabled = true;
    window.setConfig(config);

    auto* routingStatusLabel = window.findChild<QLabel*>(QStringLiteral("routingStatusLabel"));
    auto* coreStatusLabel = window.findChild<QLabel*>(QStringLiteral("coreStatusLabel"));
    auto* proxyStatusLabel = window.findChild<QLabel*>(QStringLiteral("proxyStatusLabel"));
    auto* autoRunStatusLabel = window.findChild<QLabel*>(QStringLiteral("autoRunStatusLabel"));
    auto* statisticsStatusLabel = window.findChild<QLabel*>(QStringLiteral("statisticsStatusLabel"));
    QVERIFY(routingStatusLabel != nullptr);
    QVERIFY(coreStatusLabel != nullptr);
    QVERIFY(proxyStatusLabel != nullptr);
    QVERIFY(autoRunStatusLabel != nullptr);
    QVERIFY(statisticsStatusLabel != nullptr);

    QVERIFY(!coreStatusLabel->property("semanticState").toString().isEmpty());
    QVERIFY(!proxyStatusLabel->property("semanticState").toString().isEmpty());
    QVERIFY(!autoRunStatusLabel->property("semanticState").toString().isEmpty());
    QVERIFY(!statisticsStatusLabel->property("semanticState").toString().isEmpty());
    QVERIFY(!coreStatusLabel->property("semanticState").toString().contains(QStringLiteral("QLabel")));
    QVERIFY(!proxyStatusLabel->property("semanticState").toString().contains(QStringLiteral("QLabel")));
    QVERIFY(!autoRunStatusLabel->property("semanticState").toString().contains(QStringLiteral("QLabel")));
    QVERIFY(!statisticsStatusLabel->property("semanticState").toString().contains(QStringLiteral("QLabel")));
}

void MainWindowTests::compactUiZonesDoNotExceedServerTableFont()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    RoutingItem routing;
    routing.remarks = QStringLiteral("Bypass Mainland Route");
    config.routingItems = {routing};
    config.routingIndex = 0;
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    auto* routingCombo = window.findChild<QComboBox*>(QStringLiteral("routingModeCombo"));
    auto* subscriptionTabBar = window.findChild<QTabBar*>(QStringLiteral("subscriptionTabBar"));
    auto* subscriptionTabBarContainer = window.findChild<QWidget*>(QStringLiteral("subscriptionTabBarContainer"));
    auto* serverHeaderRow = window.findChild<QWidget*>(QStringLiteral("serverHeaderRow"));
    auto* rootSplitter = window.findChild<QSplitter*>(QStringLiteral("rootSplitter"));
    auto* serverFilterEdit = window.findChild<QLineEdit*>(QStringLiteral("serverFilterEdit"));
    auto* logFilterEdit = window.findChild<QLineEdit*>(QStringLiteral("logFilterEdit"));
    auto* routingStatusLabel = window.findChild<QLabel*>(QStringLiteral("routingStatusLabel"));
    auto* currentServerStatusLabel = window.findChild<QLabel*>(QStringLiteral("currentServerStatusLabel"));
    QVERIFY(serverView != nullptr);
    QVERIFY(routingCombo != nullptr);
    QVERIFY(subscriptionTabBar != nullptr);
    QVERIFY(subscriptionTabBarContainer != nullptr);
    QVERIFY(serverHeaderRow != nullptr);
    QVERIFY(rootSplitter != nullptr);
    QVERIFY(serverFilterEdit != nullptr);
    QVERIFY(logFilterEdit != nullptr);
    QVERIFY(routingStatusLabel != nullptr);
    QVERIFY(currentServerStatusLabel != nullptr);

    const qreal serverTableFontSize = serverView->font().pointSizeF();
    QVERIFY(routingCombo->font().pointSizeF() <= serverTableFontSize);
    QVERIFY(subscriptionTabBar->font().pointSizeF() <= serverTableFontSize);
    QVERIFY(!subscriptionTabBar->expanding());
    QVERIFY(!subscriptionTabBar->drawBase());
    QVERIFY(subscriptionTabBarContainer->layout() != nullptr);
    QCOMPARE(serverFilterEdit->parentWidget(), subscriptionTabBarContainer);
    const QMargins subscriptionTabBarMargins = subscriptionTabBarContainer->layout()->contentsMargins();
    QCOMPARE(subscriptionTabBarMargins.left(), 0);
    QCOMPARE(subscriptionTabBarMargins.top(), 0);
    QCOMPARE(subscriptionTabBarMargins.right(), 0);
    QCOMPARE(subscriptionTabBarMargins.bottom(), 0);
    auto* serverPanelLayout = qobject_cast<QBoxLayout*>(serverHeaderRow->parentWidget()->layout());
    QVERIFY(serverPanelLayout != nullptr);
    QCOMPARE(serverPanelLayout->spacing(), 0);
    const QMargins rootSplitterMargins = rootSplitter->contentsMargins();
    QCOMPARE(rootSplitterMargins.left(), 0);
    QCOMPARE(rootSplitterMargins.top(), 4);
    QCOMPARE(rootSplitterMargins.right(), 0);
    QCOMPARE(rootSplitterMargins.bottom(), 0);
    QVERIFY(serverFilterEdit->font().pointSizeF() <= serverTableFontSize);
    QVERIFY(logFilterEdit->font().pointSizeF() <= serverTableFontSize);
    QVERIFY(routingStatusLabel->font().pointSizeF() <= serverTableFontSize);
    QVERIFY(currentServerStatusLabel->font().pointSizeF() <= serverTableFontSize);
    QCOMPARE(routingCombo->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
    QCOMPARE(serverFilterEdit->sizePolicy().horizontalPolicy(), QSizePolicy::Preferred);
    QCOMPARE(logFilterEdit->sizePolicy().horizontalPolicy(), QSizePolicy::Preferred);
    QCOMPARE(routingCombo->maximumWidth(), routingCombo->minimumWidth());
    QVERIFY(serverFilterEdit->maximumWidth() > serverFilterEdit->minimumWidth());
    QVERIFY(logFilterEdit->maximumWidth() > logFilterEdit->minimumWidth());
    QVERIFY(routingCombo->minimumWidth() >= QFontMetrics(routingCombo->font()).horizontalAdvance(routingCombo->currentText()));

    const int routingComboWidth = routingCombo->width();
    window.setCoreRunning(false, true);
    QCoreApplication::processEvents();
    QCOMPARE(routingCombo->width(), routingComboWidth);
    window.setCoreRunning(true, false);
    QCoreApplication::processEvents();
    QCOMPARE(routingCombo->width(), routingComboWidth);
}

QTEST_MAIN(MainWindowTests)

#include "MainWindowTests.moc"
