#include "app/CoreStartupChecklistController.h"

#include <utility>

#include <QDateTime>
#include <QTimer>

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

}

CoreStartupChecklistController::CoreStartupChecklistController(QObject* parent)
    : QObject(parent)
{
}

void CoreStartupChecklistController::reset(const QStringList& steps, bool showOverlay)
{
    steps_ = steps;
    stepStatus_.clear();
    stepDetails_.clear();
    overlayRequested_ = showOverlay;
    overlayShown_ = false;
    ++stableRunGeneration_;
    for (const QString& step : steps_) {
        stepStatus_.insert(step, CoreStartupCheckpointStatus::Pending);
    }

    if (showOverlay) {
        syncOverlay();
    }
}

void CoreStartupChecklistController::setCheckpointStatus(
    CoreStartupCheckpointStatus status,
    const QString& step,
    const QString& detail)
{
    if (!steps_.contains(step)) {
        steps_.append(step);
    }
    stepStatus_.insert(step, status);
    stepDetails_.insert(step, detail.trimmed());

    if (!overlayRequested_) {
        return;
    }

    syncOverlay();
}

void CoreStartupChecklistController::clear()
{
    if (overlayShown_ && overlayShownAtMs_ > 0) {
        const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - overlayShownAtMs_;
        if (elapsedMs < CoreStartupOverlayMinimumVisibleMs) {
            const int generation = ++overlayClearGeneration_;
            QTimer::singleShot(
                static_cast<int>(CoreStartupOverlayMinimumVisibleMs - elapsedMs),
                this,
                [this, generation]() {
                    if (generation == overlayClearGeneration_) {
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
    ++overlayClearGeneration_;
    ++stableRunGeneration_;
    if (shouldClearOverlay) {
        emit cleared();
    }
}

void CoreStartupChecklistController::clearAfterStableRun(
    int delayMs,
    std::function<bool()> stillStablePredicate)
{
    const int generation = ++stableRunGeneration_;
    QTimer::singleShot(delayMs, this, [this, generation, predicate = std::move(stillStablePredicate)]() {
        if (generation != stableRunGeneration_) {
            return;
        }
        if (predicate && !predicate()) {
            return;
        }
        clear();
    });
}

void CoreStartupChecklistController::requestOverlay()
{
    if (overlayRequested_) {
        return;
    }
    overlayRequested_ = true;
    syncOverlay();
}

bool CoreStartupChecklistController::hasSteps() const
{
    return !steps_.isEmpty();
}

bool CoreStartupChecklistController::overlayRequested() const
{
    return overlayRequested_;
}

bool CoreStartupChecklistController::overlayShown() const
{
    return overlayShown_;
}

bool CoreStartupChecklistController::overlayActive() const
{
    return overlayShown_ || overlayRequested_;
}

CoreStartupCheckpointStatus CoreStartupChecklistController::statusOf(const QString& step) const
{
    return stepStatus_.value(step, CoreStartupCheckpointStatus::Pending);
}

void CoreStartupChecklistController::setKeepOnNextStop(bool keep)
{
    keepOnNextStop_ = keep;
}

bool CoreStartupChecklistController::consumeKeepOnNextStop()
{
    const bool current = keepOnNextStop_;
    keepOnNextStop_ = false;
    return current;
}

void CoreStartupChecklistController::syncOverlay()
{
    if (!overlayRequested_) {
        return;
    }

    QStringList items;
    items.reserve(steps_.size());
    for (const QString& itemStep : steps_) {
        const CoreStartupCheckpointStatus itemStatus = stepStatus_.value(
            itemStep,
            CoreStartupCheckpointStatus::Pending);
        items.append(coreStartupChecklistItem(
            itemStatus,
            itemStep,
            overlayDetailText(stepDetails_.value(itemStep))));
    }
    emit itemsChanged(items);
    overlayShown_ = true;
    if (overlayShownAtMs_ <= 0) {
        overlayShownAtMs_ = QDateTime::currentMSecsSinceEpoch();
    }
}
