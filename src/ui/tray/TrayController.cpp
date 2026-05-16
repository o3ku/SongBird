#include "ui/tray/TrayController.h"

#include "common/AppPlatform.h"

#include <QAction>
#include <QApplication>
#include <QActionGroup>
#include <QCoreApplication>
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

QString trayText(const char* sourceText)
{
    return QCoreApplication::translate("TrayController", sourceText);
}

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

    const qreal dpr = qMax(qreal(1.0), qApp ? qApp->devicePixelRatio() : qreal(1.0));
    const QSize logicalSize(TrayIconExtent, TrayIconExtent);
    const QSize deviceSize = logicalSize * dpr;

    QPixmap pixmap = baseIcon.pixmap(deviceSize);
    if (pixmap.isNull()) {
        pixmap = QPixmap(deviceSize);
        pixmap.fill(Qt::transparent);
    }
    pixmap.setDevicePixelRatio(dpr);

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

TrayController::~TrayController()
{
    if (trayIcon_ != nullptr) {
        trayIcon_->setContextMenu(nullptr);
        trayIcon_->hide();
    }
    if (trayMenu_ != nullptr && trayMenu_->parent() == nullptr) {
        delete trayMenu_;
        trayMenu_ = nullptr;
    }
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

    trayMenu_ = new QMenu();
    trayMenu_->setObjectName(QStringLiteral("trayMenu"));
    currentServerAction_ = trayMenu_->addAction(trayText("Current: %1").arg(noServerPlaceholderText()));
    currentServerAction_->setObjectName(QStringLiteral("trayCurrentServerAction"));
    currentServerAction_->setEnabled(false);
    trayMenu_->addSeparator();
    serversMenu_ = trayMenu_->addMenu(trayText("Switch Server"));
    serversMenu_->setObjectName(QStringLiteral("trayServersMenu"));
    routingsMenu_ = trayMenu_->addMenu(trayText("Switch Routing"));
    routingsMenu_->setObjectName(QStringLiteral("trayRoutingsMenu"));
    trayMenu_->addSeparator();
    startCoreAction_ = trayMenu_->addAction(trayText("Start Core"));
    startCoreAction_->setObjectName(QStringLiteral("trayStartCoreAction"));
    stopCoreAction_ = trayMenu_->addAction(trayText("Stop Core"));
    stopCoreAction_->setObjectName(QStringLiteral("trayStopCoreAction"));
    trayMenu_->addSeparator();
    quitAction_ = trayMenu_->addAction(trayText("Quit"));
    quitAction_->setObjectName(QStringLiteral("trayQuitAction"));

    trayIcon_->setContextMenu(trayMenu_);

    connect(startCoreAction_, &QAction::triggered, this, &TrayController::startCoreRequested);
    connect(stopCoreAction_, &QAction::triggered, this, &TrayController::stopCoreRequested);
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
    const QString trimmed = name.trimmed();
    if (currentServerName_ == trimmed) {
        return;
    }

    currentServerName_ = trimmed;
    updateMenuText();
}

void TrayController::setCoreRunning(bool enabled, bool pending)
{
    if (coreRunning_ == enabled && coreTransitionPending_ == pending) {
        return;
    }

    coreRunning_ = enabled;
    coreTransitionPending_ = pending;
    updateMenuText();
}

void TrayController::setSystemProxyState(int mode, bool enabled)
{
    const SystemProxyMode normalizedMode = normalizeSystemProxyMode(mode);
    if (systemProxyMode_ == normalizedMode && systemProxyApplied_ == enabled) {
        return;
    }

    systemProxyMode_ = normalizedMode;
    systemProxyApplied_ = enabled;
    updateMenuText();
}

void TrayController::setBackgroundTaskRunning(bool running)
{
    if (backgroundTaskRunning_ == running) {
        return;
    }

    backgroundTaskRunning_ = running;
    updateMenuText();
}

void TrayController::setBackgroundTaskDescription(const QString& description)
{
    const QString trimmed = description.trimmed();
    if (backgroundTaskDescription_ == trimmed) {
        return;
    }

    backgroundTaskDescription_ = trimmed;
    updateToolTip();
}

void TrayController::setAutoRunEnabled(bool enabled)
{
    if (autoRunEnabled_ == enabled) {
        return;
    }

    autoRunEnabled_ = enabled;
    updateMenuText();
}

void TrayController::setRoutingSummary(const QString& value, bool advancedEnabled)
{
    const QString trimmed = value.trimmed();
    if (routingSummary_ == trimmed && advancedRoutingEnabled_ == advancedEnabled) {
        return;
    }

    routingSummary_ = trimmed;
    advancedRoutingEnabled_ = advancedEnabled;
    updateMenuText();
}

void TrayController::setStatisticsSummary(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (statisticsSummary_ == trimmed) {
        return;
    }

    statisticsSummary_ = trimmed;
    updateToolTip();
}

void TrayController::setTrafficSummary(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trafficSummary_ == trimmed) {
        return;
    }

    trafficSummary_ = trimmed;
    updateToolTip();
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
        const QString currentServerText = currentServerName_.trimmed().isEmpty()
            ? noServerPlaceholderText()
            : currentServerName_.trimmed();
        currentServerAction_->setText(trayText("Current: %1").arg(
            currentServerText));
    }

    if (startCoreAction_ != nullptr) {
        startCoreAction_->setEnabled(!coreRunning_ && !coreTransitionPending_);
    }

    if (stopCoreAction_ != nullptr) {
        stopCoreAction_->setEnabled(coreRunning_ && !coreTransitionPending_);
    }

    if (serversMenu_ != nullptr) {
        serversMenu_->setEnabled(!servers_.isEmpty());
    }

    if (routingsMenu_ != nullptr) {
        routingsMenu_->menuAction()->setVisible(advancedRoutingEnabled_);
        routingsMenu_->setEnabled(advancedRoutingEnabled_ && !routings_.isEmpty());
    }

    updateToolTip();
    updateTrayIcon();
}

void TrayController::updateToolTip()
{
    if (trayIcon_ == nullptr) {
        return;
    }

    QString proxyText = systemProxyModeDisplayName(systemProxyMode_);
    const bool expectedApplied = expectedSystemProxyEnabled(systemProxyMode_);
    if (systemProxyMode_ != SystemProxyMode::Unchanged && systemProxyApplied_ != expectedApplied) {
        proxyText += trayText(" (not applied)");
    }

    QString tooltip = QStringLiteral("v2rayq | %1 | %2 | %3 | %4")
                          .arg(currentServerName_.isEmpty() ? trayText("No default server") : currentServerName_)
                          .arg(trayText("Core %1").arg(coreRunning_ ? trayText("Running") : trayText("Stopped")))
                          .arg(trayText("Proxy %1").arg(proxyText))
                          .arg(trayText("Auto Run %1").arg(autoRunEnabled_ ? trayText("Enabled") : trayText("Disabled")));
    if (!routingSummary_.isEmpty()) {
        tooltip += QStringLiteral(" | ") + trayText("Routing %1").arg(routingSummary_);
    }
    if (!statisticsSummary_.isEmpty()) {
        tooltip += QStringLiteral(" | ") + statisticsSummary_;
    }
    if (!trafficSummary_.isEmpty()) {
        tooltip += QStringLiteral(" | ") + trafficSummary_;
    }
    if (backgroundTaskRunning_ && !backgroundTaskDescription_.isEmpty()) {
        tooltip += QStringLiteral(" | ") + trayText("Task %1").arg(backgroundTaskDescription_);
    }
    trayIcon_->setToolTip(tooltip);
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
        QAction* placeholder = serversMenu_->addAction(trayText("No servers"));
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
            trayText("%1 more server(s) hidden").arg(servers_.size() - effectiveLimit));
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
        QAction* placeholder = routingsMenu_->addAction(trayText("No routing entries"));
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

    return item.address.trimmed().isEmpty() ? trayText("Unnamed Server") : item.address.trimmed();
}

QString TrayController::describeRouting(const RoutingItem& item, int index)
{
    const QString remarks = item.remarks.trimmed();
    return remarks.isEmpty()
        ? trayText("Routing %1").arg(index + 1)
        : remarks;
}
