#include "ui/tray/TrayController.h"

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

#include "ui/mainwindow/MainWindow.h"

namespace {

QString trayText(const char* sourceText)
{
    return QCoreApplication::translate("TrayController", sourceText);
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
    trayIcon_->setIcon(TrayMenuSupport::defaultTrayIcon());
    trayIcon_->setToolTip(QStringLiteral("SongBird"));

    trayMenu_ = new QMenu();
    trayMenu_->setObjectName(QStringLiteral("trayMenu"));
    currentServerAction_ = trayMenu_->addAction(TrayMenuSupport::currentServerActionText(trayMenu_, currentServerName_));
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
    servers_ = TrayMenuSupport::makeServerEntries(servers);
    currentServerId_ = currentIndexId.trimmed();
    TrayMenuSupport::rebuildServerMenu(
        serversMenu_,
        servers_,
        currentServerId_,
        this,
        [this](const QString& indexId) {
            emit defaultServerRequested(indexId);
        });
}

void TrayController::setRoutings(const QList<RoutingItem>& routings, int currentRoutingIndex, bool advancedEnabled)
{
    routings_ = TrayMenuSupport::makeRoutingEntries(routings);
    currentRoutingIndex_ = currentRoutingIndex;
    advancedRoutingEnabled_ = advancedEnabled;
    TrayMenuSupport::rebuildRoutingMenu(
        routingsMenu_,
        routings_,
        currentRoutingIndex_,
        advancedRoutingEnabled_,
        this,
        [this](int index) {
            emit routingRequested(index);
        });
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
        currentServerAction_->setText(TrayMenuSupport::currentServerActionText(trayMenu_, currentServerName_));
        currentServerAction_->setToolTip(TrayMenuSupport::currentServerActionToolTip(trayMenu_, currentServerName_));
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

    trayIcon_->setToolTip(TrayMenuSupport::buildToolTip(
        trayMenu_,
        currentServerName_,
        proxyUiState_,
        systemProxyApplied_,
        autoRunEnabled_,
        routingSummary_,
        backgroundTaskRunning_,
        backgroundTaskDescription_));
}

void TrayController::updateTrayIcon()
{
    if (trayIcon_ == nullptr) {
        return;
    }

    const QIcon icon = TrayMenuSupport::trayIconForState(proxyUiState_, systemProxyApplied_);

    trayIcon_->setIcon(icon);
    TrayMenuSupport::applyWindowIcon(icon, mainWindow_);
}
