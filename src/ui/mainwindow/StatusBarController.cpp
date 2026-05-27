#include "ui/mainwindow/StatusBarController.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QWidget>

#include "ui/mainwindow/StatusBarSupport.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int CurrentServerNameMaximumWidth = 260;
constexpr int CurrentServerTooltipNameMaximumWidth = 360;

} // namespace

StatusBarController::StatusBarController(
    QWidget* owner,
    QStatusBar* statusBar,
    std::function<void()> settingsRequested,
    std::function<void()> currentServerRequested,
    std::function<void()> updateDownloadRequested)
    : owner_(owner)
    , settingsRequested_(std::move(settingsRequested))
    , currentServerRequested_(std::move(currentServerRequested))
    , updateDownloadRequested_(std::move(updateDownloadRequested))
{
    currentServerStatusLabel_ = new QLabel(owner);
    routingStatusLabel_ = new QLabel(owner);
    backgroundTaskStatusLabel_ = new QLabel(owner);

    currentServerStatusLabel_->setObjectName(QStringLiteral("currentServerStatusLabel"));
    routingStatusLabel_->setObjectName(QStringLiteral("routingStatusLabel"));
    backgroundTaskStatusLabel_->setObjectName(QStringLiteral("backgroundTaskStatusLabel"));

    AppTheme::applyCompactFont({
        statusBar,
        currentServerStatusLabel_,
        routingStatusLabel_,
        backgroundTaskStatusLabel_});

    currentServerStatusLabel_->setCursor(Qt::PointingHandCursor);
    currentServerStatusLabel_->setToolTip(QObject::tr("Click to show the current server."));
    currentServerStatusLabel_->installEventFilter(owner);
    routingStatusLabel_->setCursor(Qt::PointingHandCursor);
    routingStatusLabel_->setToolTip(QObject::tr("Click to open settings."));
    routingStatusLabel_->installEventFilter(owner);
    backgroundTaskStatusLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    backgroundTaskStatusLabel_->setMinimumWidth(0);
    backgroundTaskStatusLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    backgroundTaskStatusLabel_->installEventFilter(owner);

    statusBar->addPermanentWidget(currentServerStatusLabel_);
    statusBar->addPermanentWidget(routingStatusLabel_);
    statusBar->addPermanentWidget(backgroundTaskStatusLabel_, 1);

    updateStatusIndicators();
}

void StatusBarController::refresh(
    const QString& currentServerName,
    const QString& currentServerLocation,
    const QString& currentServerWarning,
    const QString& listenSummary,
    bool backgroundTaskRunning,
    const QString& backgroundTaskDescription,
    const QString& idleStatusText,
    const QString& updateAvailableText,
    bool updateAvailable)
{
    snapshot_.currentServerName = currentServerName;
    snapshot_.currentServerLocation = currentServerLocation;
    snapshot_.currentServerWarning = currentServerWarning;
    snapshot_.listenSummary = listenSummary;
    snapshot_.backgroundTaskRunning = backgroundTaskRunning;
    snapshot_.backgroundTaskDescription = backgroundTaskDescription;
    snapshot_.idleStatusText = idleStatusText;
    snapshot_.updateAvailableText = updateAvailableText;
    snapshot_.updateAvailable = updateAvailable;
    updateStatusIndicators();
}

const StatusBarController::Snapshot& StatusBarController::snapshot() const
{
    return snapshot_;
}

void StatusBarController::refreshBackgroundTaskStatusLabel(bool ownerVisible)
{
    if (backgroundTaskStatusLabel_ == nullptr) {
        return;
    }

    const QString fullText = currentBackgroundTaskStatusText();
    QString visibleText = fullText;
    const int availableWidth = backgroundTaskStatusLabel_->contentsRect().width();
    if (ownerVisible && availableWidth > 0) {
        visibleText = StatusBarSupport::elideTextWithThreeDots(
            backgroundTaskStatusLabel_->fontMetrics(),
            fullText,
            availableWidth);
    }

    backgroundTaskStatusLabel_->setText(visibleText);
    if (snapshot_.updateAvailable && !snapshot_.backgroundTaskRunning) {
        backgroundTaskStatusLabel_->setToolTip(QObject::tr("Click to download the available update."));
    } else {
        backgroundTaskStatusLabel_->setToolTip(visibleText == fullText ? QString() : fullText);
    }
    updateBackgroundTaskStatusInteraction();
}

void StatusBarController::setCompactMode(bool compact)
{
    if (compactMode_ == compact) {
        return;
    }

    compactMode_ = compact;
    updateStatusIndicators();
}

bool StatusBarController::handleEvent(QObject* watched, QEvent* event)
{
    if (watched == currentServerStatusLabel_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (currentServerRequested_) {
                    currentServerRequested_();
                }
                return true;
            }
        }

        if (event->type() == QEvent::ContextMenu) {
            if (currentServerRequested_) {
                currentServerRequested_();
            }
            return true;
        }
    }

    if (watched == routingStatusLabel_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (settingsRequested_) {
                    settingsRequested_();
                }
                return true;
            }
        }

        if (event->type() == QEvent::ContextMenu) {
            if (settingsRequested_) {
                settingsRequested_();
            }
            return true;
        }
    }

    if (watched == backgroundTaskStatusLabel_ && event != nullptr
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        refreshBackgroundTaskStatusLabel(owner_ != nullptr && owner_->isVisible());
    }
    if (watched == backgroundTaskStatusLabel_ && event != nullptr
        && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton
            && snapshot_.updateAvailable
            && !snapshot_.backgroundTaskRunning) {
            if (updateDownloadRequested_) {
                updateDownloadRequested_();
            }
            return true;
        }
    }

    return false;
}

void StatusBarController::updateStatusIndicators()
{
    if (currentServerStatusLabel_ != nullptr) {
        currentServerStatusLabel_->setVisible(true);
        currentServerStatusLabel_->setText(StatusBarSupport::currentServerStatusText(
            currentServerStatusLabel_->fontMetrics(),
            snapshot_.currentServerName,
            snapshot_.currentServerLocation,
            snapshot_.currentServerWarning,
            CurrentServerNameMaximumWidth));
        currentServerStatusLabel_->setToolTip(StatusBarSupport::currentServerToolTip(
            currentServerStatusLabel_->fontMetrics(),
            snapshot_.currentServerName,
            CurrentServerTooltipNameMaximumWidth));
    }

    if (routingStatusLabel_ != nullptr) {
        routingStatusLabel_->setVisible(!compactMode_);
        routingStatusLabel_->setText(StatusBarSupport::routingStatusText(snapshot_.listenSummary));
    }

    if (backgroundTaskStatusLabel_ != nullptr) {
        refreshBackgroundTaskStatusLabel(owner_ != nullptr && owner_->isVisible());
    }
}

QString StatusBarController::currentBackgroundTaskStatusText() const
{
    return StatusBarSupport::backgroundTaskStatusText(
        snapshot_.backgroundTaskRunning,
        snapshot_.backgroundTaskDescription,
        snapshot_.idleStatusText,
        snapshot_.updateAvailableText);
}

void StatusBarController::updateBackgroundTaskStatusInteraction()
{
    if (backgroundTaskStatusLabel_ == nullptr) {
        return;
    }

    backgroundTaskStatusLabel_->setCursor(
        snapshot_.updateAvailable && !snapshot_.backgroundTaskRunning
            ? Qt::PointingHandCursor
            : Qt::ArrowCursor);
}
