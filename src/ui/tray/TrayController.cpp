#include "ui/tray/TrayController.h"

#include <QAction>
#include <QApplication>
#include <QActionGroup>
#include <QColor>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QSystemTrayIcon>

#include "ui/mainwindow/MainWindow.h"

namespace {

constexpr int TrayIconExtent = 64;

QIcon loadDefaultTrayIcon()
{
    QIcon icon(QStringLiteral(":/app/logo.ico"));
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    return icon;
}

QIcon resolveCustomRoutingIcon(const QList<RoutingItem>& routings, int currentRoutingIndex, bool advancedRoutingEnabled)
{
    if (!advancedRoutingEnabled || currentRoutingIndex < 0 || currentRoutingIndex >= routings.size()) {
        return {};
    }

    const QString customIconPath = routings.at(currentRoutingIndex).customIcon.trimmed();
    if (customIconPath.isEmpty() || !QFileInfo::exists(customIconPath)) {
        return {};
    }

    const QIcon icon(customIconPath);
    return icon.isNull() ? QIcon() : icon;
}

QPixmap resolveTrayBasePixmap(const QList<RoutingItem>& routings, int currentRoutingIndex, bool advancedRoutingEnabled)
{
    QIcon baseIcon = resolveCustomRoutingIcon(routings, currentRoutingIndex, advancedRoutingEnabled);
    if (baseIcon.isNull()) {
        baseIcon = loadDefaultTrayIcon();
    }

    QPixmap pixmap = baseIcon.pixmap(TrayIconExtent, TrayIconExtent);
    if (pixmap.isNull()) {
        pixmap = QPixmap(TrayIconExtent, TrayIconExtent);
        pixmap.fill(Qt::transparent);
    }

    return pixmap;
}

void applyWindowIcon(const QIcon& icon, MainWindow* mainWindow)
{
    if (icon.isNull()) {
        return;
    }

    QApplication::setWindowIcon(icon);
    if (mainWindow != nullptr) {
        mainWindow->setWindowIcon(icon);
    }
}

void drawProxyBadge(QPainter& painter, const QRectF& rect, const QColor& fillColor)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawEllipse(rect);
}

} // namespace

TrayController::TrayController(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , mainWindow_(mainWindow)
{
}

bool TrayController::initialize()
{
    if (mainWindow_ == nullptr || !QSystemTrayIcon::isSystemTrayAvailable()) {
        return false;
    }

    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setObjectName(QStringLiteral("trayIcon"));
    trayIcon_->setIcon(loadDefaultTrayIcon());
    trayIcon_->setToolTip(QStringLiteral("v2rayq"));

    trayMenu_ = new QMenu(mainWindow_);
    trayMenu_->setObjectName(QStringLiteral("trayMenu"));
    currentServerAction_ = trayMenu_->addAction(QStringLiteral("Current: None"));
    currentServerAction_->setObjectName(QStringLiteral("trayCurrentServerAction"));
    currentServerAction_->setEnabled(false);
    trayMenu_->addSeparator();
    serversMenu_ = trayMenu_->addMenu(QStringLiteral("Switch Server"));
    serversMenu_->setObjectName(QStringLiteral("trayServersMenu"));
    routingsMenu_ = trayMenu_->addMenu(QStringLiteral("Switch Routing"));
    routingsMenu_->setObjectName(QStringLiteral("trayRoutingsMenu"));
    trayMenu_->addSeparator();
    showHideAction_ = trayMenu_->addAction(QStringLiteral("Hide Window"));
    showHideAction_->setObjectName(QStringLiteral("trayShowHideAction"));
    startCoreAction_ = trayMenu_->addAction(QStringLiteral("Start Core"));
    startCoreAction_->setObjectName(QStringLiteral("trayStartCoreAction"));
    stopCoreAction_ = trayMenu_->addAction(QStringLiteral("Stop Core"));
    stopCoreAction_->setObjectName(QStringLiteral("trayStopCoreAction"));
    trayMenu_->addSeparator();
    updateSubscriptionsAction_ = trayMenu_->addAction(QStringLiteral("Update Subscriptions"));
    updateSubscriptionsAction_->setObjectName(QStringLiteral("trayUpdateSubscriptionsAction"));
    importClipboardAction_ = trayMenu_->addAction(QStringLiteral("Import Clipboard"));
    importClipboardAction_->setObjectName(QStringLiteral("trayImportClipboardAction"));
    reloadConfigAction_ = trayMenu_->addAction(QStringLiteral("Reload Config"));
    reloadConfigAction_->setObjectName(QStringLiteral("trayReloadConfigAction"));
    trayMenu_->addSeparator();
    systemProxyMenu_ = trayMenu_->addMenu(QStringLiteral("System Proxy"));
    systemProxyMenu_->setObjectName(QStringLiteral("traySystemProxyMenu"));
    auto* systemProxyGroup = new QActionGroup(systemProxyMenu_);
    systemProxyGroup->setExclusive(true);
    clearProxyAction_ = systemProxyMenu_->addAction(QStringLiteral("Clear"));
    clearProxyAction_->setObjectName(QStringLiteral("trayClearProxyAction"));
    clearProxyAction_->setCheckable(true);
    systemProxyGroup->addAction(clearProxyAction_);
    globalProxyAction_ = systemProxyMenu_->addAction(QStringLiteral("Global"));
    globalProxyAction_->setObjectName(QStringLiteral("trayGlobalProxyAction"));
    globalProxyAction_->setCheckable(true);
    systemProxyGroup->addAction(globalProxyAction_);
    unchangedProxyAction_ = systemProxyMenu_->addAction(QStringLiteral("Unchanged"));
    unchangedProxyAction_->setObjectName(QStringLiteral("trayUnchangedProxyAction"));
    unchangedProxyAction_->setCheckable(true);
    systemProxyGroup->addAction(unchangedProxyAction_);
    pacProxyAction_ = systemProxyMenu_->addAction(QStringLiteral("PAC"));
    pacProxyAction_->setObjectName(QStringLiteral("trayPacProxyAction"));
    pacProxyAction_->setCheckable(true);
    systemProxyGroup->addAction(pacProxyAction_);
    toggleAutoRunAction_ = trayMenu_->addAction(QStringLiteral("Enable Auto Run"));
    toggleAutoRunAction_->setObjectName(QStringLiteral("trayToggleAutoRunAction"));
    trayMenu_->addSeparator();
    quitAction_ = trayMenu_->addAction(QStringLiteral("Quit"));
    quitAction_->setObjectName(QStringLiteral("trayQuitAction"));

    trayIcon_->setContextMenu(trayMenu_);

    connect(showHideAction_, &QAction::triggered, this, &TrayController::toggleMainWindow);
    connect(startCoreAction_, &QAction::triggered, this, &TrayController::startCoreRequested);
    connect(stopCoreAction_, &QAction::triggered, this, &TrayController::stopCoreRequested);
    connect(updateSubscriptionsAction_, &QAction::triggered, this, &TrayController::updateSubscriptionsRequested);
    connect(importClipboardAction_, &QAction::triggered, this, &TrayController::importFromClipboardRequested);
    connect(reloadConfigAction_, &QAction::triggered, this, &TrayController::reloadConfigRequested);
    connect(clearProxyAction_, &QAction::triggered, this, [this]() {
        emit systemProxyModeRequested(toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear));
    });
    connect(globalProxyAction_, &QAction::triggered, this, [this]() {
        emit systemProxyModeRequested(toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange));
    });
    connect(unchangedProxyAction_, &QAction::triggered, this, [this]() {
        emit systemProxyModeRequested(toLegacySystemProxyModeValue(SystemProxyMode::Unchanged));
    });
    connect(pacProxyAction_, &QAction::triggered, this, [this]() {
        emit systemProxyModeRequested(toLegacySystemProxyModeValue(SystemProxyMode::Pac));
    });
    connect(toggleAutoRunAction_, &QAction::triggered, this, &TrayController::toggleAutoRunRequested);
    connect(quitAction_, &QAction::triggered, this, &TrayController::quitRequested);
    connect(trayMenu_, &QMenu::aboutToShow, this, &TrayController::updateMenuText);
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            toggleMainWindow();
        }
    });

    updateMenuText();
    trayIcon_->show();
    return true;
}

bool TrayController::isAvailable() const
{
    return trayIcon_ != nullptr;
}

void TrayController::setCurrentServerName(const QString& name)
{
    currentServerName_ = name.trimmed();
    updateMenuText();
}

void TrayController::setCoreRunning(bool enabled)
{
    coreRunning_ = enabled;
    updateMenuText();
}

void TrayController::setSystemProxyState(int mode, bool enabled)
{
    systemProxyMode_ = normalizeSystemProxyMode(mode);
    systemProxyApplied_ = enabled;
    updateMenuText();
}

void TrayController::setProxyEnabled(bool enabled)
{
    setSystemProxyState(
        enabled ? toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange)
                : toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear),
        enabled);
}

void TrayController::setAutoRunEnabled(bool enabled)
{
    autoRunEnabled_ = enabled;
    updateMenuText();
}

void TrayController::setRoutingSummary(const QString& value, bool advancedEnabled)
{
    routingSummary_ = value.trimmed();
    advancedRoutingEnabled_ = advancedEnabled;
    updateMenuText();
}

void TrayController::setStatisticsSummary(const QString& value)
{
    statisticsSummary_ = value.trimmed();
    updateMenuText();
}

void TrayController::setTrafficSummary(const QString& value)
{
    trafficSummary_ = value.trimmed();
    updateMenuText();
}

void TrayController::showMessage(const QString& title, const QString& message, bool error, int timeoutMs)
{
    if (trayIcon_ == nullptr || message.trimmed().isEmpty()) {
        return;
    }

    trayIcon_->showMessage(
        title,
        message,
        error ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information,
        timeoutMs);
}

void TrayController::setServers(const QList<VmessItem>& servers, const QString& currentIndexId, int limit)
{
    servers_ = servers;
    currentServerId_ = currentIndexId.trimmed();
    serverMenuLimit_ = limit;
    rebuildServerMenu();
}

void TrayController::setRoutings(const QList<RoutingItem>& routings, int currentRoutingIndex, bool advancedEnabled)
{
    routings_ = routings;
    currentRoutingIndex_ = currentRoutingIndex;
    advancedRoutingEnabled_ = advancedEnabled;
    rebuildRoutingMenu();
    updateMenuText();
}

void TrayController::showMainWindow()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    mainWindow_->show();
    if (mainWindow_->isMinimized()) {
        mainWindow_->showNormal();
    }
    mainWindow_->raise();
    mainWindow_->activateWindow();
    updateMenuText();
}

void TrayController::hideMainWindow()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    mainWindow_->hide();
    updateMenuText();
}

void TrayController::toggleMainWindow()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    if (mainWindow_->isVisible()) {
        hideMainWindow();
        return;
    }

    showMainWindow();
}

void TrayController::updateMenuText()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    if (currentServerAction_ != nullptr) {
        currentServerAction_->setText(QStringLiteral("Current: %1").arg(
            currentServerName_));
    }

    if (showHideAction_ != nullptr) {
        showHideAction_->setText(mainWindow_->isVisible()
            ? QStringLiteral("Hide Window")
            : QStringLiteral("Show Window"));
    }

    if (startCoreAction_ != nullptr) {
        startCoreAction_->setEnabled(!coreRunning_);
    }

    if (stopCoreAction_ != nullptr) {
        stopCoreAction_->setEnabled(coreRunning_);
    }

    if (clearProxyAction_ != nullptr) {
        clearProxyAction_->setChecked(systemProxyMode_ == SystemProxyMode::ForcedClear);
    }

    if (globalProxyAction_ != nullptr) {
        globalProxyAction_->setChecked(systemProxyMode_ == SystemProxyMode::ForcedChange);
    }

    if (unchangedProxyAction_ != nullptr) {
        unchangedProxyAction_->setChecked(systemProxyMode_ == SystemProxyMode::Unchanged);
    }

    if (pacProxyAction_ != nullptr) {
        pacProxyAction_->setChecked(systemProxyMode_ == SystemProxyMode::Pac);
    }

    if (toggleAutoRunAction_ != nullptr) {
        toggleAutoRunAction_->setText(autoRunEnabled_
            ? QStringLiteral("Disable Auto Run")
            : QStringLiteral("Enable Auto Run"));
    }

    if (serversMenu_ != nullptr) {
        serversMenu_->setEnabled(!servers_.isEmpty());
    }

    if (routingsMenu_ != nullptr) {
        routingsMenu_->menuAction()->setVisible(advancedRoutingEnabled_);
        routingsMenu_->setEnabled(advancedRoutingEnabled_ && !routings_.isEmpty());
    }

    if (trayIcon_ != nullptr) {
        QString proxyText = systemProxyModeDisplayName(systemProxyMode_);
        const bool expectedApplied = expectedSystemProxyEnabled(systemProxyMode_);
        if (systemProxyMode_ != SystemProxyMode::Unchanged && systemProxyApplied_ != expectedApplied) {
            proxyText += QStringLiteral(" (not applied)");
        }

        QString tooltip = QStringLiteral("v2rayq | %1 | Core %2 | Proxy %3 | Auto Run %4")
                              .arg(currentServerName_.isEmpty() ? QStringLiteral("No default server") : currentServerName_)
                              .arg(coreRunning_ ? QStringLiteral("Running") : QStringLiteral("Stopped"))
                              .arg(proxyText)
                              .arg(autoRunEnabled_ ? QStringLiteral("Enabled") : QStringLiteral("Disabled"));
        if (!routingSummary_.isEmpty()) {
            tooltip += QStringLiteral(" | Routing %1").arg(routingSummary_);
        }
        if (!statisticsSummary_.isEmpty()) {
            tooltip += QStringLiteral(" | ") + statisticsSummary_;
        }
        if (!trafficSummary_.isEmpty()) {
            tooltip += QStringLiteral(" | ") + trafficSummary_;
        }
        trayIcon_->setToolTip(tooltip);
    }

    updateTrayIcon();
}

void TrayController::updateTrayIcon()
{
    if (trayIcon_ == nullptr) {
        return;
    }

    QPixmap pixmap = resolveTrayBasePixmap(routings_, currentRoutingIndex_, advancedRoutingEnabled_);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    switch (systemProxyMode_) {
    case SystemProxyMode::ForcedChange:
        drawProxyBadge(painter, QRectF(32.0, 32.0, 32.0, 32.0), QColor(QStringLiteral("#00C853")));
        break;
    case SystemProxyMode::Unchanged:
        drawProxyBadge(painter, QRectF(32.0, 32.0, 32.0, 32.0), QColor(QStringLiteral("#FACC15")));
        break;
    case SystemProxyMode::ForcedClear:
    default:
        break;
    }

    const QIcon icon(pixmap);
    trayIcon_->setIcon(icon);
    applyWindowIcon(icon, mainWindow_);
}

void TrayController::rebuildServerMenu()
{
    if (serversMenu_ == nullptr) {
        return;
    }

    serversMenu_->clear();
    if (servers_.isEmpty()) {
        QAction* placeholder = serversMenu_->addAction(QStringLiteral("No servers"));
        placeholder->setEnabled(false);
        return;
    }

    auto* group = new QActionGroup(serversMenu_);
    group->setExclusive(true);

    const int effectiveLimit = serverMenuLimit_ <= 0
        ? servers_.size()
        : qMin(serverMenuLimit_, servers_.size());
    for (int index = 0; index < effectiveLimit; ++index) {
        const VmessItem& item = servers_.at(index);
        QAction* action = serversMenu_->addAction(describeServer(item));
        action->setCheckable(true);
        action->setChecked(item.indexId == currentServerId_);
        action->setData(item.indexId);
        action->setObjectName(QStringLiteral("trayServerAction_%1").arg(index));
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, action]() {
            emit defaultServerRequested(action->data().toString());
        });
    }

    if (effectiveLimit < servers_.size()) {
        serversMenu_->addSeparator();
        QAction* remainingAction = serversMenu_->addAction(
            QStringLiteral("%1 more server(s) hidden").arg(servers_.size() - effectiveLimit));
        remainingAction->setEnabled(false);
    }
}

void TrayController::rebuildRoutingMenu()
{
    if (routingsMenu_ == nullptr) {
        return;
    }

    routingsMenu_->clear();
    routingsMenu_->menuAction()->setVisible(advancedRoutingEnabled_);
    if (!advancedRoutingEnabled_) {
        return;
    }

    if (routings_.isEmpty()) {
        QAction* placeholder = routingsMenu_->addAction(QStringLiteral("No routing entries"));
        placeholder->setEnabled(false);
        return;
    }

    auto* group = new QActionGroup(routingsMenu_);
    group->setExclusive(true);

    for (int index = 0; index < routings_.size(); ++index) {
        QAction* action = routingsMenu_->addAction(describeRouting(routings_.at(index), index));
        action->setCheckable(true);
        action->setChecked(index == currentRoutingIndex_);
        action->setData(index);
        action->setObjectName(QStringLiteral("trayRoutingAction_%1").arg(index));
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, action]() {
            emit routingRequested(action->data().toInt());
        });
    }
}

QString TrayController::describeServer(const VmessItem& item)
{
    const QString remarks = item.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    if (!item.address.trimmed().isEmpty() && item.port > 0) {
        return QStringLiteral("%1:%2").arg(item.address.trimmed()).arg(item.port);
    }

    return item.address.trimmed().isEmpty() ? QStringLiteral("Unnamed Server") : item.address.trimmed();
}

QString TrayController::describeRouting(const RoutingItem& item, int index)
{
    const QString remarks = item.remarks.trimmed();
    return remarks.isEmpty()
        ? QStringLiteral("Routing %1").arg(index + 1)
        : remarks;
}
