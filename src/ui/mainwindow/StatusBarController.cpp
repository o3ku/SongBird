#include "ui/mainwindow/StatusBarController.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QWidget>

#include "common/AppPlatform.h"
#include "ui/theme/AppTheme.h"

namespace {

QString elideTextWithThreeDots(const QFontMetrics& fontMetrics, const QString& text, int availableWidth)
{
    static const QString ellipsis = QStringLiteral("...");
    if (availableWidth <= 0 || fontMetrics.horizontalAdvance(text) <= availableWidth) {
        return text;
    }

    if (fontMetrics.horizontalAdvance(ellipsis) >= availableWidth) {
        return ellipsis;
    }

    int low = 0;
    int high = text.size();
    while (low < high) {
        const int mid = (low + high + 1) / 2;
        const QString candidate = text.left(mid) + ellipsis;
        if (fontMetrics.horizontalAdvance(candidate) <= availableWidth) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }

    return text.left(low) + ellipsis;
}

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

void StatusBarController::setSnapshot(const Snapshot& snapshot)
{
    snapshot_ = snapshot;
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
        visibleText = elideTextWithThreeDots(
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
        const QString currentServerText = snapshot_.currentServerName.trimmed().isEmpty()
            ? noServerPlaceholderText()
            : snapshot_.currentServerName.trimmed();
        QString labelText = QObject::tr("Current: %1").arg(currentServerText);
        if (!snapshot_.currentServerLocation.isEmpty()) {
            labelText += QStringLiteral(" | %1").arg(snapshot_.currentServerLocation);
        }
        if (!snapshot_.currentServerWarning.isEmpty()) {
            labelText += QStringLiteral(" | %1").arg(snapshot_.currentServerWarning);
        }
        currentServerStatusLabel_->setText(labelText);
    }

    if (routingStatusLabel_ != nullptr) {
        const QString listenText = snapshot_.listenSummary.isEmpty()
            ? QObject::tr("Unavailable")
            : snapshot_.listenSummary;
        routingStatusLabel_->setText(QObject::tr("Listening: %1").arg(listenText));
    }

    if (transientStatusLabel_ != nullptr) {
        refreshTransientStatusLabel(owner_ != nullptr && owner_->isVisible());
    }
}

QString StatusBarController::currentTransientStatusText() const
{
    if (!transientStatusMessage_.isEmpty()) {
        return transientStatusMessage_;
    }

    if (snapshot_.backgroundTaskRunning && !snapshot_.backgroundTaskDescription.isEmpty()) {
        return QObject::tr("Task: %1").arg(snapshot_.backgroundTaskDescription);
    }

    return QObject::tr("Ready");
}

bool StatusBarController::shouldSuppressRoutineStatus() const
{
    return !transientStatusMessage_.isEmpty()
        || (snapshot_.backgroundTaskRunning && !snapshot_.backgroundTaskDescription.isEmpty());
}
