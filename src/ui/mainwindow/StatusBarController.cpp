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

void applySemanticState(QLabel* label, const QString& state)
{
    if (label == nullptr) {
        return;
    }

    label->setProperty("semanticState", state);
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
}

} // namespace

StatusBarController::StatusBarController(
    QWidget* owner,
    QStatusBar* statusBar,
    std::function<void()> settingsRequested,
    std::function<void(const QPoint&)> systemProxyMenuRequested)
    : owner_(owner)
    , settingsRequested_(std::move(settingsRequested))
    , systemProxyMenuRequested_(std::move(systemProxyMenuRequested))
{
    currentServerStatusLabel_ = new QLabel(owner);
    routingStatusLabel_ = new QLabel(owner);
    coreStatusLabel_ = new QLabel(owner);
    proxyStatusLabel_ = new QLabel(owner);
    autoRunStatusLabel_ = new QLabel(owner);
    statisticsStatusLabel_ = new QLabel(owner);
    transientStatusLabel_ = new QLabel(owner);

    currentServerStatusLabel_->setObjectName(QStringLiteral("currentServerStatusLabel"));
    routingStatusLabel_->setObjectName(QStringLiteral("routingStatusLabel"));
    coreStatusLabel_->setObjectName(QStringLiteral("coreStatusLabel"));
    proxyStatusLabel_->setObjectName(QStringLiteral("proxyStatusLabel"));
    autoRunStatusLabel_->setObjectName(QStringLiteral("autoRunStatusLabel"));
    statisticsStatusLabel_->setObjectName(QStringLiteral("statisticsStatusLabel"));
    transientStatusLabel_->setObjectName(QStringLiteral("transientStatusLabel"));

    AppTheme::applyCompactFont({
        statusBar,
        currentServerStatusLabel_,
        routingStatusLabel_,
        coreStatusLabel_,
        proxyStatusLabel_,
        autoRunStatusLabel_,
        statisticsStatusLabel_,
        transientStatusLabel_});

    proxyStatusLabel_->setCursor(Qt::PointingHandCursor);
    proxyStatusLabel_->setToolTip(QObject::tr("Click to change system proxy mode."));
    proxyStatusLabel_->installEventFilter(owner);
    routingStatusLabel_->setCursor(Qt::PointingHandCursor);
    routingStatusLabel_->setToolTip(QObject::tr("Click to open settings."));
    routingStatusLabel_->installEventFilter(owner);
    transientStatusLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    transientStatusLabel_->setMinimumWidth(0);
    transientStatusLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    transientStatusLabel_->installEventFilter(owner);

    statusBar->addPermanentWidget(currentServerStatusLabel_);
    statusBar->addPermanentWidget(routingStatusLabel_);
    statusBar->addPermanentWidget(coreStatusLabel_);
    statusBar->addPermanentWidget(proxyStatusLabel_);
    statusBar->addPermanentWidget(autoRunStatusLabel_);
    statusBar->addPermanentWidget(statisticsStatusLabel_);
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
    const QString& currentCoreName,
    const StatisticsSessionState& statisticsState,
    bool systemProxyApplied,
    bool autoRunEnabled,
    bool coreProcessRunning,
    bool coreRunning,
    bool coreTransitionPending,
    bool backgroundTaskRunning,
    const QString& backgroundTaskDescription,
    SystemProxyMode systemProxyMode)
{
    snapshot_.currentServerName = currentServerName;
    snapshot_.currentServerLocation = currentServerLocation;
    snapshot_.currentServerWarning = currentServerWarning;
    snapshot_.listenSummary = listenSummary;
    snapshot_.currentCoreName = currentCoreName;
    snapshot_.statisticsState = statisticsState;
    snapshot_.systemProxyApplied = systemProxyApplied;
    snapshot_.autoRunEnabled = autoRunEnabled;
    snapshot_.coreProcessRunning = coreProcessRunning;
    snapshot_.coreRunning = coreRunning;
    snapshot_.coreTransitionPending = coreTransitionPending;
    snapshot_.backgroundTaskRunning = backgroundTaskRunning;
    snapshot_.backgroundTaskDescription = backgroundTaskDescription;
    snapshot_.systemProxyMode = systemProxyMode;
    updateStatusIndicators();
}

const StatusBarController::Snapshot& StatusBarController::snapshot() const
{
    return snapshot_;
}

void StatusBarController::showTransientStatus(const QString& message, int timeoutMs, TransientStatusPriority priority)
{
    if (priority == TransientStatusPriority::Routine) {
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

    if (watched == proxyStatusLabel_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::RightButton) {
                if (systemProxyMenuRequested_) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                    systemProxyMenuRequested_(mouseEvent->globalPosition().toPoint());
#else
                    systemProxyMenuRequested_(mouseEvent->globalPos());
#endif
                }
                return true;
            }
        }

        if (event->type() == QEvent::ContextMenu) {
            auto* contextEvent = static_cast<QContextMenuEvent*>(event);
            if (systemProxyMenuRequested_) {
                systemProxyMenuRequested_(contextEvent->globalPos());
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

    if (coreStatusLabel_ != nullptr) {
        QString coreStateText;
        QString color;
        if (snapshot_.coreTransitionPending) {
            coreStateText = snapshot_.coreProcessRunning ? QObject::tr("Stopping") : QObject::tr("Starting");
            color = AppTheme::warningStatusColor();
        } else if (snapshot_.coreRunning) {
            coreStateText = QObject::tr("Running");
            color = AppTheme::successStatusColor();
        } else if (snapshot_.coreProcessRunning) {
            coreStateText = QObject::tr("Starting");
            color = AppTheme::warningStatusColor();
        } else {
            coreStateText = QObject::tr("Stopped");
            color = AppTheme::errorStatusColor();
        }

        coreStatusLabel_->setText(QObject::tr("Core: %1").arg(coreStateText));
        applySemanticState(coreStatusLabel_, AppTheme::semanticStatusProperty(color));
    }

    if (proxyStatusLabel_ != nullptr) {
        const bool expectedApplied = expectedSystemProxyEnabled(snapshot_.systemProxyMode);
        const bool mismatch = snapshot_.systemProxyMode != SystemProxyMode::Unchanged
            && snapshot_.systemProxyApplied != expectedApplied;
        QString proxyText;
        QString color;
        switch (snapshot_.systemProxyMode) {
        case SystemProxyMode::ForcedChange:
            proxyText = QObject::tr("Global");
            color = AppTheme::successStatusColor();
            break;
        case SystemProxyMode::Unchanged:
            proxyText = QObject::tr("Unchanged");
            color = QStringLiteral("#4d4d4d");
            break;
        case SystemProxyMode::ForcedClear:
        default:
            proxyText = QObject::tr("Clear");
            color = AppTheme::errorStatusColor();
            break;
        }

        if (mismatch) {
            proxyText += QObject::tr(" (not applied)");
            color = AppTheme::warningStatusColor();
        }

        proxyStatusLabel_->setText(QObject::tr("Proxy: %1").arg(proxyText));
        applySemanticState(proxyStatusLabel_, AppTheme::semanticStatusProperty(color));
    }

    if (autoRunStatusLabel_ != nullptr) {
        autoRunStatusLabel_->setText(QObject::tr("Auto Run: %1").arg(
            snapshot_.autoRunEnabled ? QObject::tr("Enabled") : QObject::tr("Disabled")));
        applySemanticState(
            autoRunStatusLabel_,
            AppTheme::semanticStatusProperty(
                snapshot_.autoRunEnabled ? AppTheme::successStatusColor() : AppTheme::errorStatusColor()));
    }

    if (statisticsStatusLabel_ != nullptr) {
        QString summary = QObject::tr("Stats: Off");
        QString color = AppTheme::errorStatusColor();
        if (snapshot_.statisticsState.enabled) {
            if (!snapshot_.statisticsState.running) {
                summary = QObject::tr("Stats: Idle");
                color = AppTheme::warningStatusColor();
            } else if (snapshot_.statisticsState.externallyManaged) {
                summary = QObject::tr("Stats: External");
                color = QStringLiteral("#4d4d4d");
            } else if (snapshot_.statisticsState.runtimeConfigReady
                && snapshot_.statisticsState.statePort > 0
                && snapshot_.statisticsState.pollingAvailable) {
                summary = QObject::tr("Stats API: 127.0.0.1:%1").arg(snapshot_.statisticsState.statePort);
                color = AppTheme::successStatusColor();
            } else if (snapshot_.statisticsState.runtimeConfigReady && snapshot_.statisticsState.statePort > 0) {
                summary = QObject::tr("Stats API: 127.0.0.1:%1 (waiting)").arg(snapshot_.statisticsState.statePort);
                color = AppTheme::warningStatusColor();
            } else {
                summary = QObject::tr("Stats: Enabled");
                color = AppTheme::warningStatusColor();
            }
        }

        statisticsStatusLabel_->setText(summary);
        applySemanticState(statisticsStatusLabel_, AppTheme::semanticStatusProperty(color));
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
