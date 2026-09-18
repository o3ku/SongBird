#include "app/CoreStartupChecklist.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>

#include <utility>

namespace {

constexpr int CoreStartupOverlayMinimumVisibleMs = 650;

QString overlayDetailText(QString detail)
{
    detail = detail.trimmed();
    const int urlSeparatorIndex = detail.indexOf(QStringLiteral(": http"), 0, Qt::CaseInsensitive);
    if (urlSeparatorIndex > 0) {
        return detail.left(urlSeparatorIndex).trimmed();
    }
    return detail;
}

QString proxySessionText(const char* sourceText)
{
    return QCoreApplication::translate("ProxySession", sourceText);
}

} // namespace

CoreStartupChecklist::CoreStartupChecklist(
    QObject* timerContext,
    UpdateCallback updateCallback,
    ClearCallback clearCallback)
    : timerContext_(timerContext)
    , updateCallback_(std::move(updateCallback))
    , clearCallback_(std::move(clearCallback))
{
}

bool CoreStartupChecklist::isEmpty() const
{
    return steps_.isEmpty();
}

bool CoreStartupChecklist::overlayRequested() const
{
    return overlayRequested_;
}

bool CoreStartupChecklist::visible() const
{
    return overlayShown_ || overlayRequested_;
}

CoreStartupCheckpointStatus CoreStartupChecklist::status(const QString& step) const
{
    return stepStatus_.value(step, CoreStartupCheckpointStatus::Pending);
}

void CoreStartupChecklist::prepare(bool tunEnabled, bool showOverlay)
{
    steps_.clear();
    // Keep this order in sync with ProxySession::startInternal, which validates
    // the core config (generating the runtime config) before validating the
    // runtime resources (rule sets are read from that generated config).
    steps_
        << proxySessionText("Environment cleanup")
        << proxySessionText("Validate core application")
        << proxySessionText("Validate core config")
        << proxySessionText("Validate runtime resources");
    if (tunEnabled) {
        steps_.append(proxySessionText("Start TUN runtime"));
    }
    steps_
        << proxySessionText("Start core process")
        << proxySessionText("Check outbound location")
        << proxySessionText("Apply system proxy");
    stepStatus_.clear();
    stepDetails_.clear();
    overlayRequested_ = showOverlay;
    overlayShown_ = false;
    ++stableRunGeneration_;
    for (const QString& step : steps_) {
        stepStatus_.insert(step, CoreStartupCheckpointStatus::Pending);
    }

    if (showOverlay) {
        // Defer the first overlay sync to the event loop. Pumping queued
        // events here (processEvents) re-entered the startup flow while
        // prepare() was still on the stack; a queued sync renders at the
        // next event loop iteration without that re-entrancy. syncOverlay()
        // itself is a no-op once clear() has reset overlayRequested_.
        if (timerContext_ != nullptr) {
            QTimer::singleShot(0, timerContext_, [this]() { syncOverlay(); });
        } else {
            syncOverlay();
        }
    }
}

void CoreStartupChecklist::requestOverlay()
{
    if (steps_.isEmpty()) {
        return;
    }

    overlayRequested_ = true;
    syncOverlay();
}

void CoreStartupChecklist::setStatus(
    CoreStartupCheckpointStatus status,
    const QString& step,
    const QString& detail)
{
    if (!steps_.contains(step)) {
        steps_.append(step);
    }
    stepStatus_.insert(step, status);
    stepDetails_.insert(step, detail.trimmed());

    if (overlayRequested_) {
        syncOverlay();
    }
}

void CoreStartupChecklist::clear()
{
    if (overlayShown_ && overlayShownAtMs_ > 0) {
        const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - overlayShownAtMs_;
        if (elapsedMs < CoreStartupOverlayMinimumVisibleMs && timerContext_ != nullptr) {
            const int generation = ++clearGeneration_;
            QTimer::singleShot(
                static_cast<int>(CoreStartupOverlayMinimumVisibleMs - elapsedMs),
                timerContext_,
                [this, generation]() {
                    if (generation == clearGeneration_) {
                        clear();
                    }
                });
            return;
        }
    }

    steps_.clear();
    stepStatus_.clear();
    stepDetails_.clear();
    const bool shouldClearOverlay = overlayShown_;
    overlayRequested_ = false;
    overlayShown_ = false;
    overlayShownAtMs_ = 0;
    ++clearGeneration_;
    ++stableRunGeneration_;
    if (shouldClearOverlay && clearCallback_) {
        clearCallback_();
    }
}

void CoreStartupChecklist::clearAfterStableRun(int delayMs, const std::function<bool()>& shouldClear)
{
    if (timerContext_ == nullptr) {
        return;
    }

    const int generation = ++stableRunGeneration_;
    QTimer::singleShot(delayMs, timerContext_, [this, generation, shouldClear]() {
        if (generation != stableRunGeneration_) {
            return;
        }
        if (shouldClear && !shouldClear()) {
            return;
        }
        clear();
    });
}

void CoreStartupChecklist::keepForUserDismissal(const QString& step, const QString& detail)
{
    keepOnNextStop_ = true;
    setStatus(CoreStartupCheckpointStatus::Failed, step, detail);
}

void CoreStartupChecklist::markKeepOnNextStop()
{
    keepOnNextStop_ = true;
}

bool CoreStartupChecklist::consumeKeepOnNextStop()
{
    const bool keep = keepOnNextStop_;
    keepOnNextStop_ = false;
    return keep;
}

void CoreStartupChecklist::syncOverlay()
{
    if (!overlayRequested_) {
        return;
    }

    QStringList items;
    items.reserve(steps_.size());
    for (const QString& itemStep : steps_) {
        items.append(coreStartupChecklistItem(
            status(itemStep),
            itemStep,
            overlayDetailText(stepDetails_.value(itemStep))));
    }
    if (updateCallback_) {
        updateCallback_(items);
    }
    overlayShown_ = true;
    if (overlayShownAtMs_ <= 0) {
        overlayShownAtMs_ = QDateTime::currentMSecsSinceEpoch();
    }
}
