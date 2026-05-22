#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFontMetrics>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QTextOption>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetList>

#include "domain/models/Config.h"
#include "subscription/ShareUrlBuilder.h"
#include "ui/mainwindow/LogItemDelegate.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/mainwindow/ServerTableView.h"
#include "ui/models/LogListModel.h"
#include "ui/qr/QrCodeRenderer.h"

class MainWindowTests : public QObject {
    Q_OBJECT

private slots:
    void appendLogKeepsBottomWhenCursorAtLastLine();
    void appendLogPreservesViewportWhenCursorNotAtLastLine();
    void appendLogPreservesViewportWhenFilterActive();
    void logStickToBottomButtonMatchesFilterHeight();
    void informationHeaderClickTogglesLogCollapse();
    void logContextMenuUsesViewportCoordinates();
    void toolbarOrdersSettingsSubscriptionsAndRoutingButtons();
    void routingToolbarButtonEmitsRoutingSettingsSignal();
    void toolbarUsesFullWidthLayoutAndCompactVerticalMargins();
    void runtimeToolbarButtonsUseRoutingComboDefaultBorder();
    void sharePanelShowsSelectedServerShareLink();
    void sharePanelClearsWhenSelectionIsCleared();
    void shareLinkTextEditDoesNotForceTallMinimumHeight();
    void qrCodeRendererRemovesQuietZone();
    void proxyToggleButtonUsesCheckedStateAndEmitsSignals();
    void proxyToggleButtonRemainsEnabledToDisableActiveProxyWithoutServers();
    void proxyToggleButtonAllowsStartWhenNoCompatibleCoreInstalled();
    void tunToggleButtonUsesCheckedStateAndEmitsSignals();
    void tunToggleButtonOnlyEmitsTunChangeWhenBothWereOff();
    void tunToggleButtonOnlyEmitsTunChangeWhenCoreWasOffButProxyStateWasStale();
    void restoreAndCaptureUiStatePreservesQrPreviewVisibility();
    void logDelegateUsesViewportWidthForSingleLineHeight();
    void logDelegateKeepsReportedSingleLineAtWideWidth();
    void logViewRelayoutsItemHeightAfterResize();
    void enterOnServerTableSetsCurrentWithoutMovingSelection();
    void speedTestRefreshKeepsCurrentSelectionOnTriggeredRow();
    void defaultServerRefreshKeepsCurrentSelectionOnTriggeredRow();
    void speedTestResultUpdateKeepsSelectionOnSameServerWhenSorted();
    void speedTestRunningSuspendsDynamicSortingUntilBatchFinishes();
    void serverTableDisablesDragReorderingAndKeepsMultiSelection();
    void serverContextMenuKeepsExistingMultiSelection();
    void serverContextMenuSelectsClickedUnselectedRow();
    void serverContextMenuCopyUrlActionSupportsSingleAndMultiSelection();
    void serverContextMenuOnUngroupedBlankAreaShowsAddServerOnly();
    void setConfigReappliesFallbackSubscriptionFilterWhenSelectedTabDisappears();
    void coreStartupChecklistOverlayCoversMainWindowUntilCleared();
    void coreStartupChecklistOverlayShowsCoreDownloadProgress();
    void coreStartupChecklistOverlayShowsGeoDownloadProgress();
    void subscriptionUpdateOverlayCentersTextWithoutActionArea();
    void copySubscriptionUrlActionCopiesCurrentSubscriptionUrl();
    void currentServerStatusUsesNoServerPlaceholderWhenEmpty();
    void currentServerStatusAppendsLocation();
    void currentServerStatusAppendsManualVerificationWarning();
    void coreStatusRemainsStartingUntilStrictActivation();
    void runtimeStatusLabelsRemainVisibleInStatusBar();
    void serverSelectionDoesNotShowTransientStatusMessage();
    void routineTransientStatusShowsWhenStatusBarIsIdle();
    void routineTransientStatusDoesNotOverrideStatusLabel();
    void transientStatusTemporarilyOverridesBackgroundTaskMessage();
    void longTransientStatusUsesThreeDotsAndTooltip();
    void requestExitShowsConfirmationEvenWhenHideToTrayIsEnabled();
    void requestExitConfirmationUsesMainWindowAsParentWhenHidden();
    void statusLabelsUseThemePropertiesInsteadOfInlineStyleSheets();
    void compactUiZonesDoNotExceedServerTableFont();
};

namespace {

QString emojiMark(ushort codePoint)
{
    return QString(QChar(codePoint));
}

QString checklistItem(ushort mark, const QString& text)
{
    return QStringLiteral("%1 %2").arg(emojiMark(mark), text);
}

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

void selectServerRow(QTableView* serverView, int row)
{
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->model() != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QModelIndex index = serverView->model()->index(row, 0);
    QVERIFY(index.isValid());
    serverView->selectionModel()->setCurrentIndex(
        index,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current);
}

int serverRowByDisplayName(QTableView* serverView, const QString& displayName)
{
    if (serverView == nullptr || serverView->model() == nullptr) {
        return -1;
    }

    constexpr int RemarksColumn = 2;
    for (int row = 0; row < serverView->model()->rowCount(); ++row) {
        if (serverView->model()->index(row, RemarksColumn).data(Qt::DisplayRole).toString() == displayName) {
            return row;
        }
    }

    return -1;
}

void selectServerByDisplayName(QTableView* serverView, const QString& displayName)
{
    const int row = serverRowByDisplayName(serverView, displayName);
    QVERIFY(row >= 0);
    selectServerRow(serverView, row);
}

void selectServerRows(QTableView* serverView, int firstRow, int lastRow)
{
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->model() != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QModelIndex firstIndex = serverView->model()->index(firstRow, 0);
    const QModelIndex lastIndex = serverView->model()->index(lastRow, 0);
    QVERIFY(firstIndex.isValid());
    QVERIFY(lastIndex.isValid());
    serverView->selectionModel()->select(
        QItemSelection(firstIndex, lastIndex),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    serverView->selectionModel()->setCurrentIndex(
        lastIndex,
        QItemSelectionModel::NoUpdate);
}

void selectServerRowsByDisplayName(QTableView* serverView, const QStringList& displayNames)
{
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->model() != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    serverView->selectionModel()->clearSelection();
    for (const QString& displayName : displayNames) {
        const int row = serverRowByDisplayName(serverView, displayName);
        QVERIFY(row >= 0);
        const QModelIndex index = serverView->model()->index(row, 0);
        serverView->selectionModel()->select(
            index,
            QItemSelectionModel::Select | QItemSelectionModel::Rows);
        serverView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    }
}

bool waitForClipboardText(const QString& expectedText, int timeoutMs = 1000)
{
    if (QApplication::clipboard() == nullptr) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        if (QApplication::clipboard()->text() == expectedText) {
            return true;
        }
    }

    return QApplication::clipboard()->text() == expectedText;
}

QStringList selectedServerDisplayNames(QTableView* serverView)
{
    QStringList names;
    if (serverView == nullptr || serverView->model() == nullptr || serverView->selectionModel() == nullptr) {
        return names;
    }

    constexpr int RemarksColumn = 2;
    const QModelIndexList selectedRows = serverView->selectionModel()->selectedRows();
    for (const QModelIndex& rowIndex : selectedRows) {
        names.append(serverView->model()->index(rowIndex.row(), RemarksColumn).data(Qt::DisplayRole).toString());
    }
    names.sort();
    return names;
}

VmessItem createServer(const QString& id, const QString& remarks, int sequence)
{
    VmessItem item;
    item.indexId = id;
    item.remarks = remarks;
    item.address = QStringLiteral("127.0.0.%1").arg(sequence);
    item.port = 10000 + sequence;
    item.configType = ConfigType::VMess;
    return item;
}

Config createServerSelectionConfig()
{
    Config config;
    config.collection().servers = {
        createServer(QStringLiteral("server-1"), QStringLiteral("First"), 1),
        createServer(QStringLiteral("server-2"), QStringLiteral("Second"), 2),
        createServer(QStringLiteral("server-3"), QStringLiteral("Third"), 3)};
    config.currentIndexId = QStringLiteral("server-1");
    return config;
}

QString shareUrlForServerRemarks(const Config& config, const QString& remarks)
{
    for (const VmessItem& server : config.collection().servers) {
        if (server.remarks == remarks) {
            return ShareUrlBuilder::build(server);
        }
    }

    return {};
}

Config createGroupedServerSelectionConfig()
{
    Config config = createServerSelectionConfig();
    config.collection().subscriptions = {
        SubItem{
            QStringLiteral("sub-1"),
            QStringLiteral("Group 1"),
            QStringLiteral("https://example.com/sub-1"),
            true,
            QStringLiteral("ua")},
        SubItem{
            QStringLiteral("sub-2"),
            QStringLiteral("Group 2"),
            QStringLiteral("https://example.com/sub-2"),
            true,
            QStringLiteral("ua")}};
    config.collection().servers[0].subId = QStringLiteral("sub-1");
    config.collection().servers[1].subId = QStringLiteral("sub-1");
    config.collection().servers[2].subId = QStringLiteral("sub-2");
    return config;
}

bool triggerServerContextMenuAction(
    MainWindow& window,
    const QPoint& menuPoint,
    const QString& actionObjectName,
    QStringList* actionTexts = nullptr)
{
    bool triggered = false;
    auto inspectMenu = [&triggered, &actionObjectName, actionTexts](QMenu* menu) {
        if (menu == nullptr) {
            return false;
        }

        for (QAction* action : menu->actions()) {
            if (action == nullptr || action->isSeparator()) {
                continue;
            }

            if (actionTexts != nullptr && !actionTexts->contains(action->text())) {
                actionTexts->append(action->text());
            }

            if (action->objectName() == actionObjectName) {
                action->trigger();
                triggered = true;
            }
        }

        menu->close();
        return true;
    };

    QMetaObject::invokeMethod(
        &window,
        "showServerContextMenu",
        Qt::DirectConnection,
        Q_ARG(QPoint, menuPoint));

    QElapsedTimer timer;
    timer.start();
    while (!triggered && timer.elapsed() < 1000) {
        if (inspectMenu(qobject_cast<QMenu*>(QApplication::activePopupWidget()))) {
            if (triggered) {
                break;
            }
        }

        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (inspectMenu(qobject_cast<QMenu*>(widget)) && triggered) {
                break;
            }
        }
        if (triggered) {
            break;
        }

        for (QWidget* widget : QApplication::allWidgets()) {
            if (inspectMenu(qobject_cast<QMenu*>(widget)) && triggered) {
                break;
            }
        }
        if (triggered) {
            break;
        }

        QTest::qWait(10);
    }

    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (auto* menu = qobject_cast<QMenu*>(widget)) {
            menu->close();
        }
    }

    return triggered;
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

void MainWindowTests::informationHeaderClickTogglesLogCollapse()
{
    MainWindow window;
    window.resize(1000, 640);
    window.show();
    QCoreApplication::processEvents();

    auto* logHeaderRow = window.findChild<QWidget*>(QStringLiteral("logHeaderRow"));
    auto* logView = window.findChild<QListView*>(QStringLiteral("logView"));
    auto* filterEdit = window.findChild<QLineEdit*>(QStringLiteral("logFilterEdit"));
    QVERIFY(logHeaderRow != nullptr);
    QVERIFY(logView != nullptr);
    QVERIFY(filterEdit != nullptr);
    QVERIFY(logView->isVisible());

    QTest::mouseClick(logHeaderRow, Qt::LeftButton, Qt::NoModifier, QPoint(6, logHeaderRow->height() / 2));
    QCoreApplication::processEvents();
    QVERIFY(!logView->isVisible());
    QCOMPARE(window.findChild<QWidget*>(QStringLiteral("logPanel"))->maximumHeight(), logHeaderRow->sizeHint().height());

    QTest::mouseClick(filterEdit, Qt::LeftButton, Qt::NoModifier, QPoint(4, filterEdit->height() / 2));
    QCoreApplication::processEvents();
    QVERIFY(!logView->isVisible());

    QTest::mouseClick(logHeaderRow, Qt::LeftButton, Qt::NoModifier, QPoint(6, logHeaderRow->height() / 2));
    QCoreApplication::processEvents();
    QVERIFY(logView->isVisible());
}

void MainWindowTests::logContextMenuUsesViewportCoordinates()
{
    MainWindow window;
    window.resize(1000, 640);
    window.show();
    QCoreApplication::processEvents();
    appendManyLogs(window, 8, QStringLiteral("context"));

    auto* logView = window.findChild<QListView*>(QStringLiteral("logView"));
    auto* logMenu = window.findChild<QMenu*>(QStringLiteral("logContextMenu"));
    QVERIFY(logView != nullptr);
    QVERIFY(logView->viewport() != nullptr);
    QVERIFY(logMenu != nullptr);
    QVERIFY(logView->model() != nullptr);
    QVERIFY(logView->model()->rowCount() > 0);

    const QPoint menuPoint = logView->visualRect(logView->model()->index(0, 0)).center();
    const QPoint expectedGlobal = logView->viewport()->mapToGlobal(menuPoint);

    QContextMenuEvent event(QContextMenuEvent::Mouse, menuPoint, expectedGlobal);
    QApplication::sendEvent(logView->viewport(), &event);
    QTRY_VERIFY(QApplication::activePopupWidget() == logMenu);
    QCOMPARE(logMenu->pos(), expectedGlobal);

    logMenu->close();
    QTRY_VERIFY(QApplication::activePopupWidget() == nullptr);
}

void MainWindowTests::toolbarOrdersSettingsSubscriptionsAndRoutingButtons()
{
    MainWindow window;
    window.show();
    QCoreApplication::processEvents();

    auto* settingsButton = window.findChild<QToolButton*>(QStringLiteral("settingButton"));
    auto* subButton = window.findChild<QToolButton*>(QStringLiteral("subButton"));
    auto* routingButton = window.findChild<QToolButton*>(QStringLiteral("routingButton"));
    QVERIFY(settingsButton != nullptr);
    QVERIFY(subButton != nullptr);
    QVERIFY(routingButton != nullptr);

    QVERIFY(settingsButton->x() < subButton->x());
    QVERIFY(subButton->x() < routingButton->x());
    QCOMPARE(settingsButton->text(), QStringLiteral("Settings"));
    QCOMPARE(subButton->text(), QStringLiteral("Subscriptions"));
    QCOMPARE(routingButton->text(), QStringLiteral("Routing"));
}

void MainWindowTests::routingToolbarButtonEmitsRoutingSettingsSignal()
{
    MainWindow window;
    QSignalSpy spy(&window, SIGNAL(openSettingsAtRoutingTabRequested()));
    QVERIFY(spy.isValid());

    auto* button = window.findChild<QToolButton*>(QStringLiteral("routingButton"));
    QVERIFY(button != nullptr);
    button->click();

    QCOMPARE(spy.count(), 1);
}

void MainWindowTests::toolbarUsesFullWidthLayoutAndCompactVerticalMargins()
{
    MainWindow window;

    auto* toolBar = window.findChild<QToolBar*>(QStringLiteral("mainToolBar"));
    auto* settingsButton = window.findChild<QToolButton*>(QStringLiteral("settingButton"));
    auto* routingButton = window.findChild<QToolButton*>(QStringLiteral("routingButton"));
    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    auto* tunButton = window.findChild<QToolButton*>(QStringLiteral("tunToggleButton"));
    auto* routingCombo = window.findChild<QComboBox*>(QStringLiteral("routingModeCombo"));
    auto* toolbarHost = window.findChild<QWidget*>(QStringLiteral("mainToolbarHost"));
    QVERIFY(toolBar != nullptr);
    QVERIFY(settingsButton != nullptr);
    QVERIFY(routingButton != nullptr);
    QVERIFY(proxyButton != nullptr);
    QVERIFY(tunButton != nullptr);
    QVERIFY(routingCombo != nullptr);
    QVERIFY(toolbarHost != nullptr);
    QCOMPARE(toolBar->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);

    auto* toolbarLayout = toolbarHost->layout();
    QVERIFY(toolbarLayout != nullptr);
    toolbarLayout->activate();
    const QMargins margins = toolbarLayout->contentsMargins();
    QCOMPARE(margins.left(), 4);
    QCOMPARE(margins.top(), 4);
    QCOMPARE(margins.right(), 4);
    QCOMPARE(margins.bottom(), 4);
    QCOMPARE(toolbarHost->height(), 37);
    QCOMPARE(toolBar->height(), 28);
    QCOMPARE(toolBar->mapTo(toolbarHost, QPoint(0, 0)).y(), 4);
    QCOMPARE(settingsButton->mapTo(toolbarHost, QPoint(0, 0)).y(), 4);
    QCOMPARE(settingsButton->height(), 28);
    QCOMPARE(routingButton->height(), 28);
    QCOMPARE(proxyButton->height(), 28);
    QCOMPARE(tunButton->height(), 28);
    QCOMPARE(routingCombo->height(), 28);
}

void MainWindowTests::runtimeToolbarButtonsUseRoutingComboDefaultBorder()
{
    QFile darkTheme(QStringLiteral(":/themes/Dark.qss"));
    QVERIFY(darkTheme.open(QIODevice::ReadOnly));
    const QString darkStyle = QString::fromUtf8(darkTheme.readAll());
    QVERIFY(darkStyle.contains(QStringLiteral(
        "QToolBar#mainToolBar QComboBox#routingModeCombo { border: 1px solid #36404a;")));
    QVERIFY(darkStyle.contains(QStringLiteral(
        "QToolBar#mainToolBar QToolButton#proxyToggleButton, QToolBar#mainToolBar QToolButton#tunToggleButton { border: 1px solid #36404a;")));
    QVERIFY(darkStyle.contains(QStringLiteral(
        "QToolBar#mainToolBar QToolButton#qrCodeButton { border: 1px solid #36404a;")));

    QFile lightTheme(QStringLiteral(":/themes/Light.qss"));
    QVERIFY(lightTheme.open(QIODevice::ReadOnly));
    const QString lightStyle = QString::fromUtf8(lightTheme.readAll());
    QVERIFY(lightStyle.contains(QStringLiteral(
        "QToolBar#mainToolBar QComboBox#routingModeCombo { border: 1px solid #d0d5dd;")));
    QVERIFY(lightStyle.contains(QStringLiteral(
        "QToolBar#mainToolBar QToolButton#proxyToggleButton, QToolBar#mainToolBar QToolButton#tunToggleButton { border: 1px solid #d0d5dd;")));
    QVERIFY(lightStyle.contains(QStringLiteral(
        "QToolBar#mainToolBar QToolButton#qrCodeButton { border: 1px solid #d0d5dd;")));
}

void MainWindowTests::sharePanelShowsSelectedServerShareLink()
{
    MainWindow window;
    window.resize(1000, 640);
    Config config = createServerSelectionConfig();
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
    QCOMPARE(shareLinkLabel->toPlainText(), ShareUrlBuilder::build(config.collection().servers.constFirst()));
    QCOMPARE(shareContentPanel->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(shareContentLayout->count(), 2);
    QCOMPARE(shareContentLayout->contentsMargins().top(), 0);
    QCOMPARE(shareContentLayout->contentsMargins().bottom(), 0);
    QCOMPARE(shareContentLayout->stretch(0), 1);
    QCOMPARE(shareContentLayout->stretch(1), 0);
    QCOMPARE(qrPlaceholder->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(qrPlaceholder->margin(), 10);
    QVERIFY(qrPlaceholder->height() <= qrPlaceholder->width());
    QTRY_VERIFY(!qrPlaceholder->pixmap()->isNull());
    const QPixmap qrPixmap = *qrPlaceholder->pixmap();
    const qreal qrPixmapDpr = qrPixmap.devicePixelRatio() <= 0.0 ? qreal(1.0) : qrPixmap.devicePixelRatio();
    const int qrPixmapLogicalWidth = qRound(qrPixmap.width() / qrPixmapDpr);
    const int qrPixmapLogicalHeight = qRound(qrPixmap.height() / qrPixmapDpr);
    QVERIFY(qrPixmapLogicalWidth <= qrPlaceholder->width() - (qrPlaceholder->margin() * 2));
    QVERIFY(qrPixmapLogicalHeight <= qrPlaceholder->height() - (qrPlaceholder->margin() * 2));
    QCOMPARE(shareLinkLabel->sizePolicy().verticalPolicy(), QSizePolicy::Preferred);
    QCOMPARE(shareLinkLabel->wordWrapMode(), QTextOption::WrapAnywhere);
    QCOMPARE(shareLinkLabel->property("shareLinkBottomPadding").toInt(), 10);
}

void MainWindowTests::sharePanelClearsWhenSelectionIsCleared()
{
    MainWindow window;
    window.resize(1000, 640);
    Config config = createServerSelectionConfig();
    window.restoreUiState(config);
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* shareAction = window.findChild<QAction*>(QStringLiteral("toggleQrPanelAction"));
    auto* shareLinkLabel = window.findChild<QPlainTextEdit*>(QStringLiteral("shareLinkLabel"));
    auto* qrPlaceholder = window.findChild<QLabel*>(QStringLiteral("qrPlaceholder"));
    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    auto* copyUrlAction = window.findChild<QAction*>(QStringLiteral("copyUrlAction"));
    QVERIFY(shareAction != nullptr);
    QVERIFY(shareLinkLabel != nullptr);
    QVERIFY(qrPlaceholder != nullptr);
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);
    QVERIFY(copyUrlAction != nullptr);

    shareAction->trigger();
    QCoreApplication::processEvents();

    QCOMPARE(shareLinkLabel->toPlainText(), ShareUrlBuilder::build(config.collection().servers.constFirst()));
    QVERIFY(copyUrlAction->isEnabled());

    serverView->clearSelection();
    QCoreApplication::processEvents();

    QCOMPARE(shareLinkLabel->toPlainText(), QString());
    QCOMPARE(qrPlaceholder->text(), QStringLiteral("QR preview placeholder"));
    QVERIFY(!copyUrlAction->isEnabled());
}

void MainWindowTests::shareLinkTextEditDoesNotForceTallMinimumHeight()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
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

void MainWindowTests::proxyToggleButtonUsesCheckedStateAndEmitsSignals()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.setExistingCoreTypes({CoreType::SingBox});
    QCoreApplication::processEvents();

    QSignalSpy enableSpy(&window, SIGNAL(enableSystemProxyRequested()));
    QSignalSpy disableSpy(&window, SIGNAL(disableSystemProxyRequested()));
    QVERIFY(enableSpy.isValid());
    QVERIFY(disableSpy.isValid());

    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(proxyButton != nullptr);
    QCOMPARE(proxyButton->text(), QStringLiteral("START"));
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(proxyButton->isEnabled());

    proxyButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(enableSpy.count(), 1);

    window.setProxyEnabled(true);
    window.setCoreProcessRunning(true);
    window.setCoreRunning(true, false);
    QCoreApplication::processEvents();
    QCOMPARE(proxyButton->text(), QStringLiteral("START"));
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(!proxyButton->isEnabled());

    window.setCurrentServerLocation(QStringLiteral("United States, Los Angeles"));
    QCoreApplication::processEvents();
    QCOMPARE(proxyButton->text(), QStringLiteral("STOP"));
    QVERIFY(proxyButton->isChecked());
    QVERIFY(proxyButton->isEnabled());

    proxyButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(disableSpy.count(), 1);

    window.setProxyEnabled(false);
    window.setCoreProcessRunning(true);
    window.setCoreRunning(true, false);
    window.setCurrentServerLocation(QStringLiteral("United States, Los Angeles"));
    QCoreApplication::processEvents();
    QCOMPARE(proxyButton->text(), QStringLiteral("START"));
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(!proxyButton->isEnabled());
}

void MainWindowTests::proxyToggleButtonRemainsEnabledToDisableActiveProxyWithoutServers()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.setExistingCoreTypes({CoreType::SingBox});
    window.setProxyEnabled(true);
    window.setCoreProcessRunning(true);
    window.setCoreRunning(true, false);
    window.setCurrentServerLocation(QStringLiteral("United States, Los Angeles"));
    QCoreApplication::processEvents();

    QSignalSpy disableSpy(&window, SIGNAL(disableSystemProxyRequested()));
    QVERIFY(disableSpy.isValid());

    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(proxyButton != nullptr);
    QVERIFY(proxyButton->isChecked());
    QVERIFY(proxyButton->isEnabled());

    config.collection().servers.clear();
    config.currentIndexId.clear();
    window.setConfig(config);
    QCoreApplication::processEvents();

    QVERIFY(proxyButton->isChecked());
    QVERIFY(proxyButton->isEnabled());
    QCOMPARE(proxyButton->toolTip(), QStringLiteral("Stop the running core and clear system proxy."));

    proxyButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(disableSpy.count(), 1);
}

void MainWindowTests::proxyToggleButtonAllowsStartWhenNoCompatibleCoreInstalled()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.setExistingCoreTypes({});
    QCoreApplication::processEvents();

    QSignalSpy enableSpy(&window, SIGNAL(enableSystemProxyRequested()));
    QVERIFY(enableSpy.isValid());

    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(proxyButton != nullptr);
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(proxyButton->isEnabled());
    QCOMPARE(
        proxyButton->toolTip(),
        QStringLiteral("Start the core with the active server and enable system proxy."));

    proxyButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(enableSpy.count(), 1);
}

void MainWindowTests::tunToggleButtonUsesCheckedStateAndEmitsSignals()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.tun().tunModeItem.enableTun = false;
    window.setConfig(config);
    QCoreApplication::processEvents();

    QSignalSpy tunSpy(&window, SIGNAL(tunEnabledChanged(bool)));
    QVERIFY(tunSpy.isValid());

    auto* tunButton = window.findChild<QToolButton*>(QStringLiteral("tunToggleButton"));
    QVERIFY(tunButton != nullptr);
    QCOMPARE(tunButton->text(), QStringLiteral("TUN"));
    QVERIFY(!tunButton->isChecked());
    QVERIFY(tunButton->isEnabled());

    tunButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(tunSpy.count(), 1);
    QCOMPARE(tunSpy.at(0).at(0).toBool(), true);

    window.setTunEnabled(true);
    QCoreApplication::processEvents();
    QVERIFY(tunButton->isChecked());

    tunButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(tunSpy.count(), 2);
    QCOMPARE(tunSpy.at(1).at(0).toBool(), false);
}

void MainWindowTests::tunToggleButtonOnlyEmitsTunChangeWhenBothWereOff()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.tun().tunModeItem.enableTun = false;
    window.setConfig(config);
    window.setExistingCoreTypes({CoreType::SingBox});
    window.setCoreProcessRunning(false);
    window.setCoreRunning(false, false);
    window.setProxyEnabled(false);
    QCoreApplication::processEvents();

    QSignalSpy tunSpy(&window, SIGNAL(tunEnabledChanged(bool)));
    QSignalSpy enableProxySpy(&window, SIGNAL(enableSystemProxyRequested()));
    QVERIFY(tunSpy.isValid());
    QVERIFY(enableProxySpy.isValid());

    auto* tunButton = window.findChild<QToolButton*>(QStringLiteral("tunToggleButton"));
    QVERIFY(tunButton != nullptr);
    QVERIFY(!tunButton->isChecked());

    tunButton->click();
    QCoreApplication::processEvents();

    QCOMPARE(tunSpy.count(), 1);
    QCOMPARE(tunSpy.at(0).at(0).toBool(), true);
    QCOMPARE(enableProxySpy.count(), 0);
}

void MainWindowTests::tunToggleButtonOnlyEmitsTunChangeWhenCoreWasOffButProxyStateWasStale()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.tun().tunModeItem.enableTun = false;
    window.setConfig(config);
    window.setExistingCoreTypes({CoreType::SingBox});
    window.setCoreProcessRunning(false);
    window.setCoreRunning(false, false);
    window.setProxyEnabled(true);
    QCoreApplication::processEvents();

    QSignalSpy tunSpy(&window, SIGNAL(tunEnabledChanged(bool)));
    QSignalSpy enableProxySpy(&window, SIGNAL(enableSystemProxyRequested()));
    QVERIFY(tunSpy.isValid());
    QVERIFY(enableProxySpy.isValid());

    auto* tunButton = window.findChild<QToolButton*>(QStringLiteral("tunToggleButton"));
    QVERIFY(tunButton != nullptr);

    tunButton->click();
    QCoreApplication::processEvents();

    QCOMPARE(tunSpy.count(), 1);
    QCOMPARE(tunSpy.at(0).at(0).toBool(), true);
    QCOMPARE(enableProxySpy.count(), 0);
}

void MainWindowTests::restoreAndCaptureUiStatePreservesQrPreviewVisibility()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.ui().mainQrPreviewVisible = true;

    window.setConfig(config);
    window.restoreUiState(config);
    window.show();
    QCoreApplication::processEvents();

    auto* shareAction = window.findChild<QAction*>(QStringLiteral("toggleQrPanelAction"));
    auto* qrPanel = window.findChild<QWidget*>(QStringLiteral("qrPanel"));
    QVERIFY(shareAction != nullptr);
    QVERIFY(qrPanel != nullptr);
    QVERIFY(shareAction->isChecked());
    QVERIFY(qrPanel->isVisible());

    Config captured;
    window.captureUiState(captured);

    QVERIFY(captured.ui().mainQrPreviewVisible);
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
    const QString text = QStringLiteral("[16:27:54] 2026/04/09 16:27:54.067987 [Info] infra/conf/serial: Reading config: &{Name:C:\\Program Files\\SongBird\\runtime\\config.generated.json Format:json}");

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
    const QString text = QStringLiteral("[16:27:54] 2026/04/09 16:27:54.067987 [Info] infra/conf/serial: Reading config: &{Name:C:\\Program Files\\SongBird\\runtime\\config.generated.json Format:json}");

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
    selectServerByDisplayName(serverView, QStringLiteral("Second"));

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
    selectServerByDisplayName(serverView, QStringLiteral("Second"));

    QSignalSpy spy(&window, SIGNAL(testServersRequested(QStringList)));
    QVERIFY(spy.isValid());

    auto* action = window.findChild<QAction*>(QStringLiteral("testAction"));
    QVERIFY(action != nullptr);
    action->trigger();
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    const QStringList requestedIds = spy.takeFirst().at(0).toStringList();
    QCOMPARE(requestedIds, QStringList{QStringLiteral("server-2")});

    config.collection().servers[1].testResult = QStringLiteral("...");
    window.setConfig(config);
    QCoreApplication::processEvents();

    QCOMPARE(serverView->currentIndex().row(), 1);
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));
}

void MainWindowTests::defaultServerRefreshKeepsCurrentSelectionOnTriggeredRow()
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
    selectServerByDisplayName(serverView, QStringLiteral("Second"));

    QSignalSpy spy(&window, SIGNAL(setDefaultServerRequested(QString)));
    QVERIFY(spy.isValid());
    connect(&window, &MainWindow::setDefaultServerRequested, &window, [&window, &config](const QString& indexId) {
        config.currentIndexId = indexId;
        window.setConfig(config);
    });

    auto* action = window.findChild<QAction*>(QStringLiteral("setDefaultServerAction"));
    QVERIFY(action != nullptr);
    action->trigger();
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("server-2"));
    QCOMPARE(serverView->currentIndex().row(), 1);
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));
}

void MainWindowTests::speedTestResultUpdateKeepsSelectionOnSameServerWhenSorted()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.collection().servers[0].testResult = QStringLiteral("20 ms");
    config.collection().servers[1].testResult = QStringLiteral("30 ms");
    config.collection().servers[2].testResult = QStringLiteral("10 ms");
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);

    serverView->sortByColumn(4, Qt::AscendingOrder);
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

void MainWindowTests::speedTestRunningSuspendsDynamicSortingUntilBatchFinishes()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.collection().servers[0].testResult = QStringLiteral("20 ms");
    config.collection().servers[1].testResult = QStringLiteral("30 ms");
    config.collection().servers[2].testResult = QStringLiteral("10 ms");
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);

    serverView->sortByColumn(4, Qt::AscendingOrder);
    QCoreApplication::processEvents();

    QCOMPARE(serverView->model()->index(0, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Third"));
    QCOMPARE(serverView->model()->index(1, 2).data(Qt::DisplayRole).toString(), QStringLiteral("First"));
    QCOMPARE(serverView->model()->index(2, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Second"));

    window.setSpeedTestRunning(true);
    window.updateServerTestResult(QStringLiteral("server-2"), QStringLiteral("5 ms"));
    QCoreApplication::processEvents();

    QCOMPARE(serverView->model()->index(0, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Third"));
    QCOMPARE(serverView->model()->index(1, 2).data(Qt::DisplayRole).toString(), QStringLiteral("First"));
    QCOMPARE(serverView->model()->index(2, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Second"));

    window.setSpeedTestRunning(false);
    QCoreApplication::processEvents();

    QCOMPARE(serverView->model()->index(0, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Second"));
    QCOMPARE(serverView->model()->index(1, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Third"));
    QCOMPARE(serverView->model()->index(2, 2).data(Qt::DisplayRole).toString(), QStringLiteral("First"));
}

void MainWindowTests::serverTableDisablesDragReorderingAndKeepsMultiSelection()
{
    MainWindow window;
    Config config = createGroupedServerSelectionConfig();
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<ServerTableView*>(QStringLiteral("serverTableView"));
    auto* serverFilterEdit = window.findChild<QLineEdit*>(QStringLiteral("serverFilterEdit"));
    QVERIFY(serverView != nullptr);
    QVERIFY(serverFilterEdit != nullptr);
    QVERIFY(window.selectSubscriptionTab(QStringLiteral("sub-1")));
    QCOMPARE(serverView->model()->rowCount(), 2);
    QVERIFY(!serverView->rowsReorderEnabled());
    QVERIFY(serverView->toolTip().isEmpty());

    selectServerRowsByDisplayName(serverView, {QStringLiteral("First"), QStringLiteral("Second")});
    QCOMPARE(selectedServerDisplayNames(serverView), QStringList({QStringLiteral("First"), QStringLiteral("Second")}));
    QCOMPARE(serverView->selectionModel()->selectedRows().size(), 2);
    QVERIFY(!serverView->moveSelectedRowsTo(2));

    serverFilterEdit->setText(QStringLiteral("First"));
    QCoreApplication::processEvents();
    QVERIFY(!serverView->rowsReorderEnabled());
    QVERIFY(serverView->toolTip().isEmpty());
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

    const int firstRow = serverRowByDisplayName(serverView, QStringLiteral("First"));
    const int secondRow = serverRowByDisplayName(serverView, QStringLiteral("Second"));
    QVERIFY(firstRow >= 0);
    QVERIFY(secondRow >= 0);
    const QModelIndex firstIndex = serverView->model()->index(firstRow, 0);
    const QModelIndex secondIndex = serverView->model()->index(secondRow, 0);
    QVERIFY(firstIndex.isValid());
    QVERIFY(secondIndex.isValid());

    selectServerRowsByDisplayName(serverView, {QStringLiteral("First"), QStringLiteral("Second")});
    QCOMPARE(selectedServerDisplayNames(serverView), QStringList({QStringLiteral("First"), QStringLiteral("Second")}));

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

    QCOMPARE(selectedServerDisplayNames(serverView), QStringList({QStringLiteral("First"), QStringLiteral("Second")}));
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

void MainWindowTests::serverContextMenuCopyUrlActionSupportsSingleAndMultiSelection()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);
    QVERIFY(QApplication::clipboard() != nullptr);

    const QModelIndex firstIndex = serverView->model()->index(0, 0);
    const QModelIndex secondIndex = serverView->model()->index(1, 0);
    QVERIFY(firstIndex.isValid());
    QVERIFY(secondIndex.isValid());

    QStringList singleSelectionActions;
    QApplication::clipboard()->clear();
    serverView->selectionModel()->select(
        QItemSelection(firstIndex, firstIndex),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QCoreApplication::processEvents();

    QVERIFY(triggerServerContextMenuAction(
        window,
        serverView->visualRect(firstIndex).center(),
        QStringLiteral("copyUrlAction"),
        &singleSelectionActions));
    const QString firstShareUrl = shareUrlForServerRemarks(config, QStringLiteral("First"));
    QVERIFY(!firstShareUrl.isEmpty());
    QVERIFY(waitForClipboardText(firstShareUrl));
    QVERIFY(singleSelectionActions.contains(QStringLiteral("Copy Url")));

    QStringList multiSelectionActions;
    QApplication::clipboard()->clear();
    selectServerRowsByDisplayName(serverView, {QStringLiteral("First"), QStringLiteral("Second")});
    QCoreApplication::processEvents();

    QVERIFY(triggerServerContextMenuAction(
        window,
        serverView->visualRect(secondIndex).center(),
        QStringLiteral("copyUrlAction"),
        &multiSelectionActions));
    const QString secondShareUrl = shareUrlForServerRemarks(config, QStringLiteral("Second"));
    QVERIFY(!secondShareUrl.isEmpty());
    QVERIFY(waitForClipboardText(firstShareUrl + QChar('\n') + secondShareUrl));
    QVERIFY(multiSelectionActions.contains(QStringLiteral("Copy Url")));
}

void MainWindowTests::serverContextMenuOnUngroupedBlankAreaShowsAddServerOnly()
{
    MainWindow window;
    Config config;
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    QVERIFY(window.selectSubscriptionTab(QStringLiteral("ungrouped")));
    QCOMPARE(serverView->model()->rowCount(), 0);

    // Verify that addServerAction_ exists and has the expected objectName
    auto* addServerAction = window.findChild<QAction*>(QStringLiteral("addServerAction"));
    QVERIFY(addServerAction != nullptr);

    // Verify blank-area click yields invalid index and the ungrouped tab is selected
    const QPoint blankPoint(serverView->viewport()->rect().center());
    QVERIFY(!serverView->indexAt(blankPoint).isValid());
}

void MainWindowTests::setConfigReappliesFallbackSubscriptionFilterWhenSelectedTabDisappears()
{
    MainWindow window;
    Config initialConfig = createGroupedServerSelectionConfig();
    window.setConfig(initialConfig);
    QVERIFY(window.selectSubscriptionTab(QStringLiteral("sub-2")));
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    auto* subscriptionTabBar = window.findChild<QTabBar*>(QStringLiteral("subscriptionTabBar"));
    QVERIFY(serverView != nullptr);
    QVERIFY(subscriptionTabBar != nullptr);
    QCOMPARE(subscriptionTabBar->tabData(subscriptionTabBar->currentIndex()).toString(), QStringLiteral("sub:sub-2"));
    QCOMPARE(serverView->model()->rowCount(), 1);
    QCOMPARE(serverView->model()->index(0, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Third"));

    Config updatedConfig = initialConfig;
    updatedConfig.collection().subscriptions = {
        SubItem{
            QStringLiteral("sub-1"),
            QStringLiteral("Group 1"),
            QStringLiteral("https://example.com/sub-1"),
            true,
            QStringLiteral("ua")}};
    updatedConfig.collection().servers.removeLast();
    updatedConfig.currentIndexId = QStringLiteral("server-1");
    window.setConfig(updatedConfig);
    QCoreApplication::processEvents();

    QCOMPARE(subscriptionTabBar->tabData(subscriptionTabBar->currentIndex()).toString(), QStringLiteral("sub:sub-1"));
    QCOMPARE(serverView->model()->rowCount(), 2);
    QCOMPARE(serverView->model()->index(0, 2).data(Qt::DisplayRole).toString(), QStringLiteral("First"));
    QCOMPARE(serverView->model()->index(1, 2).data(Qt::DisplayRole).toString(), QStringLiteral("Second"));
}

void MainWindowTests::coreStartupChecklistOverlayCoversMainWindowUntilCleared()
{
    MainWindow window;
    window.setConfig(createServerSelectionConfig());
    window.resize(900, 600);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(serverView != nullptr);
    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(proxyButton != nullptr);
    auto* overlay = window.findChild<QWidget*>(QStringLiteral("loadingOverlay"));
    QVERIFY(overlay != nullptr);
    auto* contentWidget = window.findChild<QWidget*>(QStringLiteral("loadingContentWidget"));
    QVERIFY(contentWidget != nullptr);
    auto* dismissButton = window.findChild<QPushButton*>(QStringLiteral("loadingDismissButton"));
    QVERIFY(dismissButton != nullptr);

    QSignalSpy enableSpy(&window, SIGNAL(enableSystemProxyRequested()));
    QVERIFY(enableSpy.isValid());

    window.setCoreStartupChecklist({
        checklistItem(0x2705, QStringLiteral("Environment cleanup")),
        checklistItem(0x23F3, QStringLiteral("Generate runtime config"))
    });
    QCoreApplication::processEvents();

    QVERIFY(!overlay->isHidden());
    QVERIFY(!dismissButton->isVisible());
    QCOMPARE(overlay->geometry(), window.rect());
    QVERIFY(contentWidget->width() >= 360);
    QVERIFY(contentWidget->width() <= 720);
    QCOMPARE(contentWidget->geometry().center().x(), overlay->rect().center().x());
    const QPoint proxyButtonGlobalCenter = proxyButton->mapToGlobal(proxyButton->rect().center());
    QWidget* clickedWidget = QApplication::widgetAt(proxyButtonGlobalCenter);
    if (clickedWidget != nullptr) {
        QVERIFY(clickedWidget == overlay || overlay->isAncestorOf(clickedWidget));
        QTest::mouseClick(
            clickedWidget,
            Qt::LeftButton,
            Qt::NoModifier,
            clickedWidget->mapFromGlobal(proxyButtonGlobalCenter));
    } else {
        QVERIFY(overlay->rect().contains(overlay->mapFromGlobal(proxyButtonGlobalCenter)));
        QTest::mouseClick(
            overlay,
            Qt::LeftButton,
            Qt::NoModifier,
            overlay->mapFromGlobal(proxyButtonGlobalCenter));
    }
    QCoreApplication::processEvents();
    QCOMPARE(enableSpy.count(), 0);

    const QList<QLabel*> labels = overlay->findChildren<QLabel*>(QStringLiteral("loadingChecklistItem"));
    QCOMPARE(labels.size(), 2);
    QCOMPARE(labels.at(0)->alignment() & Qt::AlignHorizontal_Mask, Qt::AlignLeft);
    QCOMPARE(overlay->findChild<QLabel*>(QStringLiteral("loadingTitleLabel"))->alignment() & Qt::AlignHorizontal_Mask, Qt::AlignLeft);
    QCOMPARE(labels.at(0)->text(), checklistItem(0x2705, QStringLiteral("Environment cleanup")));
    QCOMPARE(labels.at(1)->text(), checklistItem(0x23F3, QStringLiteral("Generate runtime config")));
    const int firstItemYBeforeFailure = labels.at(0)->mapTo(contentWidget, QPoint(0, 0)).y();

    window.clearCoreStartupChecklist();
    QCoreApplication::processEvents();

    QVERIFY(overlay->isHidden());
    QVERIFY(serverView->isEnabled());

    window.setCoreStartupChecklist({
        checklistItem(0x2705, QStringLiteral("Read active server")),
        checklistItem(0x274C, QStringLiteral("Start core process - executable missing")),
        checklistItem(0x274C, QStringLiteral("Next step - Install or update sing-box, then start Proxy/TUN again."))
    });
    QCoreApplication::processEvents();

    QVERIFY(!overlay->isHidden());
    QVERIFY(dismissButton->isVisible());
    QCOMPARE(
        overlay->findChild<QLabel*>(QStringLiteral("loadingTitleLabel"))->text(),
        QStringLiteral("Proxy startup failed"));
    const QList<QLabel*> failedLabels = overlay->findChildren<QLabel*>(QStringLiteral("loadingChecklistItem"));
    QCOMPARE(failedLabels.size(), 3);
    QCOMPARE(failedLabels.at(0)->mapTo(contentWidget, QPoint(0, 0)).y(), firstItemYBeforeFailure);

    dismissButton->click();
    QCoreApplication::processEvents();
    QVERIFY(overlay->isHidden());
}

void MainWindowTests::coreStartupChecklistOverlayShowsGeoDownloadProgress()
{
    MainWindow window;
    window.setConfig(createServerSelectionConfig());
    window.resize(900, 600);
    window.show();
    QCoreApplication::processEvents();

    auto* overlay = window.findChild<QWidget*>(QStringLiteral("loadingOverlay"));
    QVERIFY(overlay != nullptr);
    auto* contentWidget = window.findChild<QWidget*>(QStringLiteral("loadingContentWidget"));
    QVERIFY(contentWidget != nullptr);
    auto* dismissButton = window.findChild<QPushButton*>(QStringLiteral("loadingDismissButton"));
    QVERIFY(dismissButton != nullptr);

    window.setCoreStartupChecklist({
        checklistItem(0x2705, QStringLiteral("Environment cleanup")),
        checklistItem(0x2705, QStringLiteral("Generate runtime config")),
        checklistItem(0x23F3, QStringLiteral("Validate geo files"))
    });
    QCoreApplication::processEvents();

    QVERIFY(!overlay->isHidden());
    QVERIFY(!dismissButton->isVisible());
    QVERIFY(contentWidget->width() >= 360);
    QVERIFY(contentWidget->width() <= 720);
    QCOMPARE(contentWidget->geometry().center().x(), overlay->rect().center().x());

    const QList<QLabel*> labels = overlay->findChildren<QLabel*>(QStringLiteral("loadingChecklistItem"));
    QCOMPARE(labels.size(), 3);
    QCOMPARE(
        labels.at(2)->text(),
        checklistItem(0x23F3, QStringLiteral("Validate geo files")));
    QCOMPARE(
        overlay->findChild<QLabel*>(QStringLiteral("loadingTitleLabel"))->text(),
        QStringLiteral("Starting system proxy..."));
}

void MainWindowTests::coreStartupChecklistOverlayShowsCoreDownloadProgress()
{
    MainWindow window;
    window.setConfig(createServerSelectionConfig());
    window.resize(900, 600);
    window.show();
    QCoreApplication::processEvents();

    auto* overlay = window.findChild<QWidget*>(QStringLiteral("loadingOverlay"));
    QVERIFY(overlay != nullptr);
    auto* contentWidget = window.findChild<QWidget*>(QStringLiteral("loadingContentWidget"));
    QVERIFY(contentWidget != nullptr);
    auto* dismissButton = window.findChild<QPushButton*>(QStringLiteral("loadingDismissButton"));
    QVERIFY(dismissButton != nullptr);

    window.setCoreStartupChecklist({
        checklistItem(0x2705, QStringLiteral("Environment cleanup")),
        checklistItem(0x23F3, QStringLiteral("Validate core application")),
        checklistItem(0x26AA, QStringLiteral("Generate runtime config"))
    });
    QCoreApplication::processEvents();

    QVERIFY(!overlay->isHidden());
    QVERIFY(!dismissButton->isVisible());
    QVERIFY(contentWidget->width() >= 360);
    QVERIFY(contentWidget->width() <= 720);
    QCOMPARE(contentWidget->geometry().center().x(), overlay->rect().center().x());

    const QList<QLabel*> labels = overlay->findChildren<QLabel*>(QStringLiteral("loadingChecklistItem"));
    QCOMPARE(labels.size(), 3);
    QCOMPARE(
        labels.at(1)->text(),
        checklistItem(0x23F3, QStringLiteral("Validate core application")));
    QCOMPARE(
        overlay->findChild<QLabel*>(QStringLiteral("loadingTitleLabel"))->text(),
        QStringLiteral("Starting system proxy..."));
}

void MainWindowTests::subscriptionUpdateOverlayCentersTextWithoutActionArea()
{
    MainWindow window;
    window.setConfig(createServerSelectionConfig());
    window.resize(900, 600);
    window.show();
    QCoreApplication::processEvents();

    auto* overlay = window.findChild<QWidget*>(QStringLiteral("loadingOverlay"));
    QVERIFY(overlay != nullptr);
    auto* titleLabel = window.findChild<QLabel*>(QStringLiteral("loadingTitleLabel"));
    QVERIFY(titleLabel != nullptr);
    auto* itemsWidget = window.findChild<QWidget*>(QStringLiteral("loadingItemsWidget"));
    QVERIFY(itemsWidget != nullptr);
    auto* actionWidget = window.findChild<QWidget*>(QStringLiteral("loadingActionWidget"));
    QVERIFY(actionWidget != nullptr);
    auto* dismissButton = window.findChild<QPushButton*>(QStringLiteral("loadingDismissButton"));
    QVERIFY(dismissButton != nullptr);

    window.setSubscriptionUpdateRunning(true);
    QCoreApplication::processEvents();

    QVERIFY(!overlay->isHidden());
    QCOMPARE(titleLabel->text(), QStringLiteral("Updating subscriptions..."));
    QCOMPARE(titleLabel->alignment() & Qt::AlignHorizontal_Mask, Qt::AlignHCenter);
    QVERIFY(!itemsWidget->isVisible());
    QVERIFY(!actionWidget->isVisible());
    QVERIFY(!dismissButton->isVisible());

    window.setSubscriptionUpdateRunning(false);
    QCoreApplication::processEvents();
    QVERIFY(overlay->isHidden());
}

void MainWindowTests::copySubscriptionUrlActionCopiesCurrentSubscriptionUrl()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.collection().subscriptions = {SubItem{
        QStringLiteral("sub-1"),
        QStringLiteral("Test Sub"),
        QStringLiteral("https://example.com/subscription"),
        true,
        QStringLiteral("test-agent")}};
    config.ui().mainSelectedSubId = QStringLiteral("sub-1");
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

void MainWindowTests::currentServerStatusUsesNoServerPlaceholderWhenEmpty()
{
    MainWindow window;
    window.setConfig(Config());

    auto* currentServerStatusLabel = window.findChild<QLabel*>(QStringLiteral("currentServerStatusLabel"));
    QVERIFY(currentServerStatusLabel != nullptr);
    QCOMPARE(currentServerStatusLabel->text(), QStringLiteral("Current: <No Server>"));

    window.setCurrentServerName(QStringLiteral("Test Server"));
    QCOMPARE(currentServerStatusLabel->text(), QStringLiteral("Current: Test Server"));

    window.setCurrentServerName(QString());
    QCOMPARE(currentServerStatusLabel->text(), QStringLiteral("Current: <No Server>"));
}

void MainWindowTests::currentServerStatusAppendsLocation()
{
    MainWindow window;
    window.setConfig(Config());

    auto* currentServerStatusLabel =
        window.findChild<QLabel*>(QStringLiteral("currentServerStatusLabel"));
    QVERIFY(currentServerStatusLabel != nullptr);

    window.setCurrentServerName(QStringLiteral("Test Server"));
    window.setCurrentServerLocation(QStringLiteral("United States, Los Angeles"));
    QCOMPARE(
        currentServerStatusLabel->text(),
        QStringLiteral("Current: Test Server | United States, Los Angeles"));

    window.setCurrentServerLocation(QString());
    QCOMPARE(currentServerStatusLabel->text(), QStringLiteral("Current: Test Server"));
}

void MainWindowTests::currentServerStatusAppendsManualVerificationWarning()
{
    MainWindow window;
    window.setConfig(Config());

    auto* currentServerStatusLabel = window.findChild<QLabel*>(QStringLiteral("currentServerStatusLabel"));
    QVERIFY(currentServerStatusLabel != nullptr);

    window.setCurrentServerName(QStringLiteral("Test Server"));
    window.setCurrentServerLocation(QStringLiteral("United States, Los Angeles"));
    window.setCurrentServerWarning(QStringLiteral("Please verify manually"));
    QCOMPARE(
        currentServerStatusLabel->text(),
        QStringLiteral("Current: Test Server | United States, Los Angeles | Please verify manually"));

    window.setCurrentServerWarning(QString());
    QCOMPARE(
        currentServerStatusLabel->text(),
        QStringLiteral("Current: Test Server | United States, Los Angeles"));
}

void MainWindowTests::coreStatusRemainsStartingUntilStrictActivation()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.setExistingCoreTypes({CoreType::SingBox});

    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(proxyButton != nullptr);

    window.setCoreProcessRunning(true);
    window.setCoreRunning(false, false);
    QCoreApplication::processEvents();

    QCOMPARE(proxyButton->toolTip(), QStringLiteral("Proxy state is synchronizing. Wait until the core and system proxy reach the target state."));
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(!proxyButton->isEnabled());
}

void MainWindowTests::runtimeStatusLabelsRemainVisibleInStatusBar()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.ui().autoRunEnabled = true;
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* currentServerStatusLabel = window.findChild<QLabel*>(QStringLiteral("currentServerStatusLabel"));
    auto* routingStatusLabel = window.findChild<QLabel*>(QStringLiteral("routingStatusLabel"));
    QVERIFY(currentServerStatusLabel != nullptr);
    QVERIFY(routingStatusLabel != nullptr);

    QVERIFY(currentServerStatusLabel->isVisible());
    QVERIFY(routingStatusLabel->isVisible());
}

void MainWindowTests::serverSelectionDoesNotShowTransientStatusMessage()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* transientStatusLabel = window.findChild<QLabel*>(QStringLiteral("transientStatusLabel"));
    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    QVERIFY(transientStatusLabel != nullptr);
    QVERIFY(serverView != nullptr);
    QVERIFY(serverView->selectionModel() != nullptr);

    const QModelIndex secondIndex = serverView->model()->index(1, 0);
    QVERIFY(secondIndex.isValid());
    serverView->selectionModel()->select(
        QItemSelection(secondIndex, secondIndex),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QCoreApplication::processEvents();

    QCOMPARE(transientStatusLabel->text(), QStringLiteral("Ready"));
}

void MainWindowTests::routineTransientStatusShowsWhenStatusBarIsIdle()
{
    MainWindow window;

    auto* transientStatusLabel = window.findChild<QLabel*>(QStringLiteral("transientStatusLabel"));
    QVERIFY(transientStatusLabel != nullptr);
    QCOMPARE(transientStatusLabel->text(), QStringLiteral("Ready"));

    window.showTransientStatus(
        QStringLiteral("Copied 1 URL(s) to the clipboard."),
        50,
        MainWindow::TransientStatusPriority::Routine);
    QCOMPARE(transientStatusLabel->text(), QStringLiteral("Copied 1 URL(s) to the clipboard."));

    QTRY_COMPARE(transientStatusLabel->text(), QStringLiteral("Ready"));
}

void MainWindowTests::routineTransientStatusDoesNotOverrideStatusLabel()
{
    MainWindow window;
    window.setBackgroundTaskRunning(true);
    window.setBackgroundTaskDescription(QStringLiteral("Updating subscriptions"));

    auto* transientStatusLabel = window.findChild<QLabel*>(QStringLiteral("transientStatusLabel"));
    QVERIFY(transientStatusLabel != nullptr);
    const QString backgroundText = transientStatusLabel->text();
    QCOMPARE(backgroundText, QStringLiteral("Task: Updating subscriptions"));

    window.showTransientStatus(
        QStringLiteral("Copied 1 URL(s) to the clipboard."),
        50,
        MainWindow::TransientStatusPriority::Routine);
    QCOMPARE(transientStatusLabel->text(), backgroundText);
}

void MainWindowTests::transientStatusTemporarilyOverridesBackgroundTaskMessage()
{
    MainWindow window;
    window.setBackgroundTaskRunning(true);
    window.setBackgroundTaskDescription(QStringLiteral("Updating subscriptions"));

    auto* transientStatusLabel = window.findChild<QLabel*>(QStringLiteral("transientStatusLabel"));
    QVERIFY(transientStatusLabel != nullptr);
    const QString backgroundText = transientStatusLabel->text();
    QCOMPARE(backgroundText, QStringLiteral("Task: Updating subscriptions"));

    window.showTransientStatus(QStringLiteral("Copied 1 URL(s) to the clipboard."), 50);
    QCOMPARE(transientStatusLabel->text(), QStringLiteral("Copied 1 URL(s) to the clipboard."));

    QTRY_COMPARE(transientStatusLabel->text(), backgroundText);
}

void MainWindowTests::longTransientStatusUsesThreeDotsAndTooltip()
{
    MainWindow window;
    window.resize(360, 640);
    window.show();
    QCoreApplication::processEvents();

    const QString longMessage =
        QStringLiteral("This is a deliberately long transient status message that should not fit in the status bar.");
    auto* transientStatusLabel = window.findChild<QLabel*>(QStringLiteral("transientStatusLabel"));
    QVERIFY(transientStatusLabel != nullptr);
    transientStatusLabel->setFixedWidth(140);
    QCoreApplication::processEvents();

    window.showTransientStatus(longMessage, 0);
    QCoreApplication::processEvents();

    QVERIFY(transientStatusLabel->text().endsWith(QStringLiteral("...")));
    QVERIFY(transientStatusLabel->text().size() < longMessage.size());
    QCOMPARE(transientStatusLabel->toolTip(), longMessage);
}

void MainWindowTests::requestExitShowsConfirmationEvenWhenHideToTrayIsEnabled()
{
    MainWindow window;
    window.setHideToTrayEnabled(true);
    window.show();
    QCoreApplication::processEvents();

    QSignalSpy hiddenSpy(&window, SIGNAL(hiddenToTray()));
    QVERIFY(hiddenSpy.isValid());

    bool confirmationShown = false;
    QTimer::singleShot(0, [&confirmationShown]() {
        auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(dialog != nullptr);
        confirmationShown = true;
        QCOMPARE(dialog->windowTitle(), QStringLiteral("Quit SongBird"));
        QCOMPARE(dialog->text(), QStringLiteral("Quit SongBird now?"));
        QAbstractButton* noButton = dialog->button(QMessageBox::No);
        QVERIFY(noButton != nullptr);
        QTest::mouseClick(noButton, Qt::LeftButton);
    });

    QVERIFY(!window.requestExit());
    QVERIFY(confirmationShown);
    QVERIFY(window.isVisible());
    QCOMPARE(hiddenSpy.count(), 0);
}

void MainWindowTests::requestExitConfirmationUsesMainWindowAsParentWhenHidden()
{
    MainWindow window;
    window.hide();
    QCoreApplication::processEvents();

    bool confirmationParentedToWindow = false;
    QTimer::singleShot(0, [&confirmationParentedToWindow, &window]() {
        auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(dialog != nullptr);
        confirmationParentedToWindow = dialog->parentWidget() == &window;
        QAbstractButton* noButton = dialog->button(QMessageBox::No);
        QVERIFY(noButton != nullptr);
        QTest::mouseClick(noButton, Qt::LeftButton);
    });

    QVERIFY(!window.requestExit());
    QVERIFY(confirmationParentedToWindow);
}

void MainWindowTests::statusLabelsUseThemePropertiesInsteadOfInlineStyleSheets()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.ui().autoRunEnabled = true;
    window.setConfig(config);

    auto* routingStatusLabel = window.findChild<QLabel*>(QStringLiteral("routingStatusLabel"));
    QVERIFY(routingStatusLabel != nullptr);
}

void MainWindowTests::compactUiZonesDoNotExceedServerTableFont()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    RoutingItem routing;
    routing.remarks = QStringLiteral("Bypass Mainland Route");
    config.collection().routingItems = {routing};
    config.collection().routingIndex = 0;
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    auto* serverView = window.findChild<QTableView*>(QStringLiteral("serverTableView"));
    auto* routingCombo = window.findChild<QComboBox*>(QStringLiteral("routingModeCombo"));
    auto* subscriptionTabBar = window.findChild<QTabBar*>(QStringLiteral("subscriptionTabBar"));
    auto* subscriptionTabBarContainer = window.findChild<QWidget*>(QStringLiteral("subscriptionTabBarContainer"));
    auto* serverHeaderRow = window.findChild<QWidget*>(QStringLiteral("serverHeaderRow"));
    auto* rootSplitter = window.findChild<QSplitter*>(QStringLiteral("rootSplitter"));
    auto* mainToolBar = window.findChild<QToolBar*>(QStringLiteral("mainToolBar"));
    auto* mainToolbarHost = window.findChild<QWidget*>(QStringLiteral("mainToolbarHost"));
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
    QVERIFY(mainToolBar != nullptr);
    QVERIFY(mainToolbarHost != nullptr);
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
    QVERIFY(subscriptionTabBarContainer->testAttribute(Qt::WA_StyledBackground));
    QVERIFY(serverHeaderRow->testAttribute(Qt::WA_StyledBackground));
    QCOMPARE(serverFilterEdit->parentWidget(), subscriptionTabBarContainer);
    const QMargins subscriptionTabBarMargins = subscriptionTabBarContainer->layout()->contentsMargins();
    QCOMPARE(subscriptionTabBarMargins.left(), 6);
    QCOMPARE(subscriptionTabBarMargins.top(), 4);
    QCOMPARE(subscriptionTabBarMargins.right(), 4);
    QCOMPARE(subscriptionTabBarMargins.bottom(), 4);
    auto* serverPanelLayout = qobject_cast<QBoxLayout*>(serverHeaderRow->parentWidget()->layout());
    QVERIFY(serverPanelLayout != nullptr);
    QCOMPARE(serverPanelLayout->spacing(), 0);
    const QMargins rootSplitterMargins = rootSplitter->contentsMargins();
    QCOMPARE(rootSplitterMargins.left(), 0);
    QCOMPARE(rootSplitterMargins.top(), 0);
    QCOMPARE(rootSplitterMargins.right(), 0);
    QCOMPARE(rootSplitterMargins.bottom(), 0);
    QCOMPARE(rootSplitter->mapTo(&window, QPoint(0, 0)).y(), mainToolbarHost->geometry().bottom() + 1);
    QVERIFY(serverFilterEdit->font().pointSizeF() <= serverTableFontSize);
    QVERIFY(logFilterEdit->font().pointSizeF() <= serverTableFontSize);
    QVERIFY(routingStatusLabel->font().pointSizeF() <= serverTableFontSize);
    QVERIFY(currentServerStatusLabel->font().pointSizeF() <= serverTableFontSize);
    QCOMPARE(routingCombo->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
    QCOMPARE(serverFilterEdit->sizePolicy().horizontalPolicy(), QSizePolicy::Preferred);
    QCOMPARE(logFilterEdit->sizePolicy().horizontalPolicy(), QSizePolicy::Preferred);
    QCOMPARE(routingCombo->maximumWidth(), routingCombo->minimumWidth());

    auto* logHeaderRow = window.findChild<QWidget*>(QStringLiteral("logHeaderRow"));
    QVERIFY(logHeaderRow != nullptr);
    QVERIFY(logHeaderRow->testAttribute(Qt::WA_StyledBackground));
    QVERIFY(serverFilterEdit->maximumWidth() > serverFilterEdit->minimumWidth());
    QVERIFY(logFilterEdit->maximumWidth() > logFilterEdit->minimumWidth());
    QVERIFY(routingCombo->minimumWidth() >= QFontMetrics(routingCombo->font()).horizontalAdvance(routingCombo->currentText()));

    const int routingComboWidth = routingCombo->width();
    window.setCoreRunning(false, true);
    QCoreApplication::processEvents();
    QCOMPARE(routingCombo->width(), routingComboWidth);
    window.setCoreProcessRunning(true);
    window.setCoreRunning(true, false);
    QCoreApplication::processEvents();
    QCOMPARE(routingCombo->width(), routingComboWidth);
}

QTEST_MAIN(MainWindowTests)

#include "MainWindowTests.moc"

