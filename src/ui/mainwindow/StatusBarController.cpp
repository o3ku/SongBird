#include "ui/mainwindow/StatusBarController.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
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
    std::function<void()> currentServerRequested)
    : owner_(owner)
    , settingsRequested_(std::move(settingsRequested))
    , currentServerRequested_(std::move(currentServerRequested))
{
    currentServerStatusLabel_ = new QLabel(owner);
    routingStatusLabel_ = new QLabel(owner);
    transientStatusLabel_ = new QLabel(owner);

    currentServerStatusLabel_->setObjectName(QStringLiteral("currentServerStatusLabel"));
    routingStatusLabel_->setObjectName(QStringLiteral("routingStatusLabel"));
    transientStatusLabel_->setObjectName(QStringLiteral("transientStatusLabel"));

    AppTheme::applyCompactFont({
        statusBar,
        currentServerStatusLabel_,
        routingStatusLabel_,
        transientStatusLabel_});

    currentServerStatusLabel_->setCursor(Qt::PointingHandCursor);
    currentServerStatusLabel_->setToolTip(QObject::tr("Click to show the current server."));
    currentServerStatusLabel_->installEventFilter(owner);
    routingStatusLabel_->setCursor(Qt::PointingHandCursor);
    routingStatusLabel_->setToolTip(QObject::tr("Click to open settings."));
    routingStatusLabel_->installEventFilter(owner);
    transientStatusLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    transientStatusLabel_->setMinimumWidth(0);
    transientStatusLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    transientStatusLabel_->installEventFilter(owner);

    statusBar->addPermanentWidget(currentServerStatusLabel_);
    statusBar->addPermanentWidget(routingStatusLabel_);
    statusBar->addPermanentWidget(transientStatusLabel_, 1);

    updateStatusIndicators();
}

void StatusBarController::refresh(
    const QString& currentServerName,
    const QString& currentServerLocation,
    const QString& currentServerWarning,
    const QString& listenSummary,
    bool backgroundTaskRunning,
    const QString& backgroundTaskDescription)
{
    snapshot_.currentServerName = currentServerName;
    snapshot_.currentServerLocation = currentServerLocation;
    snapshot_.currentServerWarning = currentServerWarning;
    snapshot_.listenSummary = listenSummary;
    snapshot_.backgroundTaskRunning = backgroundTaskRunning;
    snapshot_.backgroundTaskDescription = backgroundTaskDescription;
    updateStatusIndicators();
}

const StatusBarController::Snapshot& StatusBarController::snapshot() const
{
    return snapshot_;
}

void StatusBarController::showTransientStatus(const QString& message, int timeoutMs, TransientStatusPriority priority)
{
    if (priority == TransientStatusPriority::Routine && shouldSuppressRoutineStatus()) {
        return;
    }

    if (transientStatusTimer_ == nullptr) {
        transientStatusTimer_ = new QTimer(owner_);
        transientStatusTimer_->setSingleShot(true);
        QObject::connect(transientStatusTimer_, &QTimer::timeout, owner_, [this]() {
            clearTransientStatus();
        });
    }

    const QString trimmedMessage = message.trimmed();
    if (trimmedMessage.isEmpty()) {
        clearTransientStatus();
        return;
    }

    transientStatusMessage_ = trimmedMessage;
    if (timeoutMs > 0) {
        transientStatusTimer_->start(timeoutMs);
    } else {
        transientStatusTimer_->stop();
    }

    updateStatusIndicators();
}

void StatusBarController::clearTransientStatus()
{
    if (transientStatusTimer_ != nullptr) {
        transientStatusTimer_->stop();
    }
    transientStatusMessage_.clear();
    updateStatusIndicators();
}

void StatusBarController::refreshTransientStatusLabel(bool ownerVisible)
{
    if (transientStatusLabel_ == nullptr) {
        return;
    }

    const QString fullText = currentTransientStatusText();
    QString visibleText = fullText;
    const int availableWidth = transientStatusLabel_->contentsRect().width();
    if (ownerVisible && availableWidth > 0) {
        visibleText = StatusBarSupport::elideTextWithThreeDots(
            transientStatusLabel_->fontMetrics(),
            fullText,
            availableWidth);
    }

    transientStatusLabel_->setText(visibleText);
    transientStatusLabel_->setToolTip(visibleText == fullText ? QString() : fullText);
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

    if (watched == transientStatusLabel_ && event != nullptr
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        refreshTransientStatusLabel(owner_ != nullptr && owner_->isVisible());
    }

    return false;
}

void StatusBarController::updateStatusIndicators()
{
    if (currentServerStatusLabel_ != nullptr) {
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
        routingStatusLabel_->setText(StatusBarSupport::routingStatusText(snapshot_.listenSummary));
    }

    if (transientStatusLabel_ != nullptr) {
        refreshTransientStatusLabel(owner_ != nullptr && owner_->isVisible());
    }
}

QString StatusBarController::currentTransientStatusText() const
{
    return StatusBarSupport::transientStatusText(
        transientStatusMessage_,
        snapshot_.backgroundTaskRunning,
        snapshot_.backgroundTaskDescription);
}

bool StatusBarController::shouldSuppressRoutineStatus() const
{
    return StatusBarSupport::shouldSuppressRoutineStatus(
        transientStatusMessage_,
        snapshot_.backgroundTaskRunning,
        snapshot_.backgroundTaskDescription);
}
