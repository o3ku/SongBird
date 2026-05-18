#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QElapsedTimer>
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
    void globalHotkeySettingsActionEmitsSignal();
    void toolbarOrdersSettingsSubscriptionsAndRoutingButtons();
    void routingToolbarButtonEmitsRoutingSettingsSignal();
    void toolbarReplacesReloadAndProxyModeComboWithToggleButtons();
    void toolbarRemovesServerButton();
    void toolbarUsesFullWidthLayoutAndCompactVerticalMargins();
    void sharePanelShowsSelectedServerShareLink();
    void sharePanelClearsWhenSelectionIsCleared();
    void shareLinkTextEditDoesNotForceTallMinimumHeight();
    void qrCodeRendererRemovesQuietZone();
    void proxyToggleButtonUsesCheckedStateAndEmitsSignals();
    void systemProxyModeActionEmitsSingleSelectionSignal();
    void tunToggleButtonUsesCheckedStateAndEmitsSignals();
    void restoreAndCaptureUiStatePreservesRuntimeToggles();
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
    void copySubscriptionUrlActionCopiesCurrentSubscriptionUrl();
    void currentServerStatusUsesNoServerPlaceholderWhenEmpty();
    void currentServerStatusAppendsLocation();
    void currentServerStatusAppendsManualVerificationWarning();
    void coreStatusRemainsStartingUntilStrictActivation();
    void runtimeStatusLabelsRemainVisibleInStatusBar();
    void serverSelectionDoesNotShowTransientStatusMessage();
    void routineTransientStatusDoesNotOverrideStatusLabel();
    void transientStatusTemporarilyOverridesBackgroundTaskMessage();
    void longTransientStatusUsesThreeDotsAndTooltip();
    void requestExitShowsConfirmationEvenWhenHideToTrayIsEnabled();
    void requestExitConfirmationUsesMainWindowAsParentWhenHidden();
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

Config createGroupedServerSelectionConfig()
{
    Config config = createServerSelectionConfig();
    config.subscriptions = {
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
    config.servers[0].subId = QStringLiteral("sub-1");
    config.servers[1].subId = QStringLiteral("sub-1");
    config.servers[2].subId = QStringLiteral("sub-2");
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

void MainWindowTests::toolbarReplacesReloadAndProxyModeComboWithToggleButtons()
{
    MainWindow window;

    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("reloadButton")) == nullptr);
    QVERIFY(window.findChild<QComboBox*>(QStringLiteral("systemProxyModeCombo")) == nullptr);
    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton")) != nullptr);
    QVERIFY(window.findChild<QToolButton*>(QStringLiteral("tunToggleButton")) != nullptr);
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
    auto* settingsButton = window.findChild<QToolButton*>(QStringLiteral("settingButton"));
    auto* routingButton = window.findChild<QToolButton*>(QStringLiteral("routingButton"));
    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    auto* tunButton = window.findChild<QToolButton*>(QStringLiteral("tunToggleButton"));
    auto* routingCombo = window.findChild<QComboBox*>(QStringLiteral("routingModeCombo"));
    QVERIFY(toolBar != nullptr);
    QVERIFY(settingsButton != nullptr);
    QVERIFY(routingButton != nullptr);
    QVERIFY(proxyButton != nullptr);
    QVERIFY(tunButton != nullptr);
    QVERIFY(routingCombo != nullptr);
    QCOMPARE(toolBar->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);

    const QMargins margins = toolBar->contentsMargins();
    QCOMPARE(margins.left(), 0);
    QCOMPARE(margins.top(), 2);
    QCOMPARE(margins.right(), 0);
    QCOMPARE(margins.bottom(), 2);
    QCOMPARE(settingsButton->height(), 28);
    QCOMPARE(routingButton->height(), 28);
    QCOMPARE(proxyButton->height(), 28);
    QCOMPARE(tunButton->height(), 28);
    QCOMPARE(routingCombo->height(), 28);
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

    QCOMPARE(shareLinkLabel->toPlainText(), ShareUrlBuilder::build(config.servers.constFirst()));
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
    window.setCoreProcessRunning(true);
    window.setCoreRunning(true, false);
    QCoreApplication::processEvents();
    QCOMPARE(proxyButton->text(), QStringLiteral("PROXY"));
    QVERIFY(proxyButton->isChecked());

    proxyButton->click();
    QCoreApplication::processEvents();
    QCOMPARE(disableSpy.count(), 1);

    window.setProxyEnabled(false);
    window.setCoreProcessRunning(true);
    window.setCoreRunning(true, false);
    QCoreApplication::processEvents();
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(!proxyButton->isEnabled());
}

void MainWindowTests::systemProxyModeActionEmitsSingleSelectionSignal()
{
    MainWindow window;
    QSignalSpy modeSpy(&window, SIGNAL(systemProxyModeSelected(int)));
    QVERIFY(modeSpy.isValid());

    auto* clearProxyAction = window.findChild<QAction*>(QStringLiteral("clearProxyAction"));
    QVERIFY(clearProxyAction != nullptr);

    clearProxyAction->trigger();
    QCoreApplication::processEvents();

    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(
        modeSpy.at(0).at(0).toInt(),
        toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear));
}

void MainWindowTests::tunToggleButtonUsesCheckedStateAndEmitsSignals()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.tunModeItem.enableTun = false;
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

void MainWindowTests::restoreAndCaptureUiStatePreservesRuntimeToggles()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.mainProxyEnabled = true;
    config.mainQrPreviewVisible = true;

    window.setConfig(config);
    window.restoreUiState(config);
    window.show();
    QCoreApplication::processEvents();

    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    auto* tunButton = window.findChild<QToolButton*>(QStringLiteral("tunToggleButton"));
    auto* shareAction = window.findChild<QAction*>(QStringLiteral("toggleQrPanelAction"));
    auto* qrPanel = window.findChild<QWidget*>(QStringLiteral("qrPanel"));
    QVERIFY(proxyButton != nullptr);
    QVERIFY(tunButton != nullptr);
    QVERIFY(shareAction != nullptr);
    QVERIFY(qrPanel != nullptr);
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(shareAction->isChecked());
    QVERIFY(qrPanel->isVisible());

    Config captured;
    window.captureUiState(captured);

    QVERIFY(!captured.mainProxyEnabled);
    QVERIFY(captured.mainQrPreviewVisible);
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
    serverView->selectRow(1);
    serverView->setCurrentIndex(serverView->model()->index(1, 2));
    QCoreApplication::processEvents();

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

void MainWindowTests::speedTestRunningSuspendsDynamicSortingUntilBatchFinishes()
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

    serverView->sortByColumn(7, Qt::AscendingOrder);
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

    serverView->selectionModel()->select(
        serverView->model()->index(0, 0),
        QItemSelectionModel::Select | QItemSelectionModel::Rows);
    serverView->selectionModel()->select(
        serverView->model()->index(1, 0),
        QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QCoreApplication::processEvents();

    QVERIFY(serverView->selectionModel()->isRowSelected(0, QModelIndex()));
    QVERIFY(serverView->selectionModel()->isRowSelected(1, QModelIndex()));
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
    QCOMPARE(QApplication::clipboard()->text(), ShareUrlBuilder::build(config.servers.at(0)));
    QVERIFY(singleSelectionActions.contains(QStringLiteral("Copy Url")));

    QStringList multiSelectionActions;
    QApplication::clipboard()->clear();
    serverView->selectionModel()->select(
        QItemSelection(firstIndex, secondIndex),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QCoreApplication::processEvents();

    QVERIFY(triggerServerContextMenuAction(
        window,
        serverView->visualRect(secondIndex).center(),
        QStringLiteral("copyUrlAction"),
        &multiSelectionActions));
    QCOMPARE(
        QApplication::clipboard()->text(),
        ShareUrlBuilder::build(config.servers.at(0)) + QChar('\n') + ShareUrlBuilder::build(config.servers.at(1)));
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

    auto* coreStatusLabel = window.findChild<QLabel*>(QStringLiteral("coreStatusLabel"));
    auto* proxyButton = window.findChild<QToolButton*>(QStringLiteral("proxyToggleButton"));
    QVERIFY(coreStatusLabel != nullptr);
    QVERIFY(proxyButton != nullptr);

    window.setCoreProcessRunning(true);
    window.setCoreRunning(false, false);
    QCoreApplication::processEvents();

    QCOMPARE(coreStatusLabel->text(), QStringLiteral("Core: Starting"));
    QCOMPARE(proxyButton->toolTip(), QStringLiteral("Proxy state is synchronizing. Wait until the core and system proxy reach the same target state."));
    QVERIFY(!proxyButton->isChecked());
    QVERIFY(!proxyButton->isEnabled());
}

void MainWindowTests::runtimeStatusLabelsRemainVisibleInStatusBar()
{
    MainWindow window;
    Config config = createServerSelectionConfig();
    config.autoRunEnabled = true;
    window.setConfig(config);
    window.show();
    QCoreApplication::processEvents();

    StatisticsSessionState statisticsState;
    statisticsState.enabled = true;
    statisticsState.running = true;
    statisticsState.runtimeConfigReady = true;
    statisticsState.pollingAvailable = true;
    statisticsState.statePort = 10808;
    statisticsState.coreType = CoreType::Xray;

    window.setSystemProxyState(static_cast<int>(SystemProxyMode::ForcedChange), true);
    window.setCoreProcessRunning(true);
    window.setCoreRunning(true, false);
    window.setAutoRunEnabled(true);
    window.setStatisticsSessionState(statisticsState);
    QCoreApplication::processEvents();

    auto* coreStatusLabel = window.findChild<QLabel*>(QStringLiteral("coreStatusLabel"));
    auto* proxyStatusLabel = window.findChild<QLabel*>(QStringLiteral("proxyStatusLabel"));
    auto* autoRunStatusLabel = window.findChild<QLabel*>(QStringLiteral("autoRunStatusLabel"));
    auto* statisticsStatusLabel = window.findChild<QLabel*>(QStringLiteral("statisticsStatusLabel"));
    QVERIFY(coreStatusLabel != nullptr);
    QVERIFY(proxyStatusLabel != nullptr);
    QVERIFY(autoRunStatusLabel != nullptr);
    QVERIFY(statisticsStatusLabel != nullptr);

    QVERIFY(coreStatusLabel->isVisible());
    QVERIFY(proxyStatusLabel->isVisible());
    QVERIFY(autoRunStatusLabel->isVisible());
    QVERIFY(statisticsStatusLabel->isVisible());
    QCOMPARE(coreStatusLabel->text(), QStringLiteral("Core: Running"));
    QCOMPARE(proxyStatusLabel->text(), QStringLiteral("Proxy: Global"));
    QCOMPARE(autoRunStatusLabel->text(), QStringLiteral("Auto Run: Enabled"));
    QCOMPARE(statisticsStatusLabel->text(), QStringLiteral("Stats API: 127.0.0.1:10808"));
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
        QCOMPARE(dialog->windowTitle(), QStringLiteral("Quit v2rayq"));
        QCOMPARE(dialog->text(), QStringLiteral("Quit v2rayq now?"));
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
    auto* mainToolBar = window.findChild<QToolBar*>(QStringLiteral("mainToolBar"));
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
    QCOMPARE(subscriptionTabBarMargins.left(), 0);
    QCOMPARE(subscriptionTabBarMargins.top(), 0);
    QCOMPARE(subscriptionTabBarMargins.right(), 0);
    QCOMPARE(subscriptionTabBarMargins.bottom(), 0);
    auto* serverPanelLayout = qobject_cast<QBoxLayout*>(serverHeaderRow->parentWidget()->layout());
    QVERIFY(serverPanelLayout != nullptr);
    QCOMPARE(serverPanelLayout->spacing(), 0);
    const QMargins rootSplitterMargins = rootSplitter->contentsMargins();
    QCOMPARE(rootSplitterMargins.left(), 0);
    QCOMPARE(rootSplitterMargins.top(), 0);
    QCOMPARE(rootSplitterMargins.right(), 0);
    QCOMPARE(rootSplitterMargins.bottom(), 0);
    QCOMPARE(rootSplitter->mapTo(&window, QPoint(0, 0)).y(), mainToolBar->geometry().bottom() + 1);
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
