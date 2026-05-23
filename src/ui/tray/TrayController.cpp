#include "ui/tray/TrayController.h"

#include "common/AppPlatform.h"

#include <QAction>
#include <QApplication>
#include <QActionGroup>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QtGlobal>

#include <algorithm>

#include "common/ServerDisplayName.h"
#include "services/SpeedTestServiceInternal.h"
#include "ui/mainwindow/MainWindow.h"

namespace {

constexpr int TrayServerNameTextMaxWidth = 300;
constexpr int TrayCurrentServerNameTextMaxWidth = 260;
constexpr int TrayCurrentServerTooltipNameTextMaxWidth = 360;
constexpr int TrayServerMenuMaxCount = 20;

struct TrayServerSortKey
{
    int resultRank = 0;
    double latencyMs = 0;
};

QString trayText(const char* sourceText)
{
    return QCoreApplication::translate("TrayController", sourceText);
}

QString elidedMenuText(QMenu* menu, const QString& text)
{
    if (menu == nullptr) {
        return text;
    }

    return menu->fontMetrics().elidedText(
        text,
        Qt::ElideRight,
        TrayServerNameTextMaxWidth);
}

QString elidedTrayText(QMenu* menu, const QString& text, int maximumWidth)
{
    if (menu == nullptr) {
        return text;
    }

    return menu->fontMetrics().elidedText(
        text,
        Qt::ElideRight,
        maximumWidth);
}

QString formatTrayTestResult(const QString& value)
{
    const QString normalized = value.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    double latencyMs = -1;
    if (SpeedTestServiceInternal::tryParseUrlProbeLatency(normalized, latencyMs)) {
        return QStringLiteral("%1 ms").arg(qRound(latencyMs));
    }

    return QStringLiteral("unavailable");
}

TrayServerSortKey makeTrayServerSortKey(const TrayServerEntry& item)
{
    const QString normalized = item.testResult.trimmed();
    if (normalized.isEmpty()) {
        return TrayServerSortKey{1, 0};
    }

    double latencyMs = -1;
    if (SpeedTestServiceInternal::tryParseUrlProbeLatency(normalized, latencyMs)) {
        return TrayServerSortKey{0, latencyMs};
    }

    return TrayServerSortKey{2, 0};
}

QString formatServerMenuText(QMenu* menu, const TrayServerEntry& item)
{
    const QString name = elidedMenuText(menu, item.displayName);
    const QString result = formatTrayTestResult(item.testResult);
    return result.isEmpty()
        ? name
        : QStringLiteral("%1 | %2").arg(name, result);
}

QIcon loadDefaultTrayIcon()
{
    QIcon icon(QStringLiteral(":/app/logo.svg"));
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    return icon;
}

QIcon loadFireTrayIcon()
{
    return QIcon(QStringLiteral(":/app/logo-fire.svg"));
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
    trayIcon_->setToolTip(QStringLiteral("SongBird"));

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
    quitAction_ = trayMenu_->addAction(trayText("Quit"));
    quitAction_->setObjectName(QStringLiteral("trayQuitAction"));

    trayIcon_->setContextMenu(trayMenu_);

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

void TrayController::setProxyUiState(ProxyUiState state)
{
    if (proxyUiState_ == state) {
        return;
    }

    proxyUiState_ = state;
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

void TrayController::setServers(const QList<VmessItem>& servers, const QString& currentIndexId)
{
    servers_.clear();
    servers_.reserve(servers.size());
    for (const VmessItem& item : servers) {
        servers_.append(TrayServerEntry{item.indexId, serverDisplayName(item), item.testResult});
    }
    currentServerId_ = currentIndexId.trimmed();
    rebuildServerMenu();
}

void TrayController::setRoutings(const QList<RoutingItem>& routings, int currentRoutingIndex, bool advancedEnabled)
{
    routings_.clear();
    routings_.reserve(routings.size());
    for (int index = 0; index < routings.size(); ++index) {
        const RoutingItem& item = routings.at(index);
        routings_.append(TrayRoutingEntry{
            describeRouting(item, index),
            item.customIcon});
    }
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
            : elidedTrayText(
                trayMenu_,
                currentServerName_.trimmed(),
                TrayCurrentServerNameTextMaxWidth);
        currentServerAction_->setText(trayText("Current: %1").arg(
            currentServerText));
        currentServerAction_->setToolTip(currentServerName_.trimmed().isEmpty()
            ? QString()
            : elidedTrayText(
                trayMenu_,
                currentServerName_.trimmed(),
                TrayCurrentServerTooltipNameTextMaxWidth));
    }

    if (serversMenu_ != nullptr) {
        serversMenu_->setEnabled(!servers_.isEmpty());
    }

    if (routingsMenu_ != nullptr) {
        routingsMenu_->setEnabled(!advancedRoutingEnabled_ || !routings_.isEmpty());
    }

    updateToolTip();
    updateTrayIcon();
}

void TrayController::updateToolTip()
{
    if (trayIcon_ == nullptr) {
        return;
    }

    const bool active = proxyUiState_ == ProxyUiState::Active;
    const QString proxyText = active && systemProxyApplied_ ? trayText("On") : trayText("Off");
    const QString currentServerText = currentServerName_.isEmpty()
        ? trayText("No default server")
        : currentServerName_;
    const QString currentServerDisplayText = elidedTrayText(
        trayMenu_,
        currentServerText,
        TrayCurrentServerNameTextMaxWidth);

    const QString coreStatusText = [this]() {
        switch (proxyUiState_) {
        case ProxyUiState::Active:
            return trayText("Running");
        case ProxyUiState::Transitioning:
            return trayText("Starting");
        case ProxyUiState::Inconsistent:
            return trayText("Starting");
        case ProxyUiState::Idle:
            break;
        }
        return trayText("Stopped");
    }();

    QString tooltip = QStringLiteral("SongBird | %1 | %2 | %3 | %4")
                          .arg(currentServerDisplayText)
                          .arg(trayText("Core %1").arg(coreStatusText))
                          .arg(trayText("Proxy %1").arg(proxyText))
                          .arg(trayText("Auto Run %1").arg(autoRunEnabled_ ? trayText("Enabled") : trayText("Disabled")));
    if (!routingSummary_.isEmpty()) {
        tooltip += QStringLiteral(" | ") + trayText("Routing %1").arg(routingSummary_);
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

    const QIcon icon = proxyUiState_ == ProxyUiState::Active && systemProxyApplied_
        ? loadFireTrayIcon()
        : loadDefaultTrayIcon();

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

    QList<int> serverIndexes;
    serverIndexes.reserve(servers_.size());
    int currentServerIndex = -1;
    for (int index = 0; index < servers_.size(); ++index) {
        if (servers_.at(index).indexId == currentServerId_) {
            currentServerIndex = index;
            continue;
        }
        serverIndexes.append(index);
    }

    std::stable_sort(serverIndexes.begin(), serverIndexes.end(), [this](int left, int right) {
        const TrayServerSortKey leftKey = makeTrayServerSortKey(servers_.at(left));
        const TrayServerSortKey rightKey = makeTrayServerSortKey(servers_.at(right));
        if (leftKey.resultRank != rightKey.resultRank) {
            return leftKey.resultRank < rightKey.resultRank;
        }
        if (leftKey.latencyMs != rightKey.latencyMs) {
            return leftKey.latencyMs < rightKey.latencyMs;
        }
        return left < right;
    });

    QList<int> visibleIndexes;
    visibleIndexes.reserve(qMin(TrayServerMenuMaxCount, servers_.size()));
    if (currentServerIndex >= 0) {
        visibleIndexes.append(currentServerIndex);
    }
    for (int index : serverIndexes) {
        if (visibleIndexes.size() >= TrayServerMenuMaxCount) {
            break;
        }
        visibleIndexes.append(index);
    }

    for (int menuIndex = 0; menuIndex < visibleIndexes.size(); ++menuIndex) {
        const int serverIndex = visibleIndexes.at(menuIndex);
        const TrayServerEntry& item = servers_.at(serverIndex);
        QAction* action = serversMenu_->addAction(formatServerMenuText(serversMenu_, item));
        const QString result = formatTrayTestResult(item.testResult);
        const QString tooltipName = elidedTrayText(
            serversMenu_,
            item.displayName,
            TrayCurrentServerTooltipNameTextMaxWidth);
        action->setToolTip(result.isEmpty()
            ? tooltipName
            : QStringLiteral("%1 | %2").arg(tooltipName, result));
        action->setCheckable(true);
        action->setChecked(item.indexId == currentServerId_);
        action->setData(item.indexId);
        action->setObjectName(QStringLiteral("trayServerAction_%1").arg(menuIndex));
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, action]() {
            emit defaultServerRequested(action->data().toString());
        });
    }
}

void TrayController::rebuildRoutingMenu()
{
    if (routingsMenu_ == nullptr) {
        return;
    }

    routingsMenu_->clear();
    if (!advancedRoutingEnabled_) {
        QAction* placeholder = routingsMenu_->addAction(trayText("Advanced routing is disabled"));
        placeholder->setEnabled(false);
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
        QAction* action = routingsMenu_->addAction(routings_.at(index).displayName);
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

QString TrayController::describeRouting(const RoutingItem& item, int index)
{
    const QString remarks = item.remarks.trimmed();
    return remarks.isEmpty()
        ? trayText("Routing %1").arg(index + 1)
        : remarks;
}
