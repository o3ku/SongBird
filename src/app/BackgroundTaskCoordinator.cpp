#include "app/BackgroundTaskCoordinator.h"

#include <utility>

#include <QCoreApplication>

BackgroundTaskCoordinator::BackgroundTaskCoordinator(QObject* parent)
    : QObject(parent)
{
}

void BackgroundTaskCoordinator::setBlockingPredicate(std::function<bool()> predicate)
{
    blockingPredicate_ = std::move(predicate);
}

bool BackgroundTaskCoordinator::tryBegin(Kind kind)
{
    if (kind == Kind::None) {
        return false;
    }

    if (blockingPredicate_ && blockingPredicate_()) {
        emit blockedByCoreStartup();
        return false;
    }

    if (activeKind_ != Kind::None) {
        return false;
    }

    activeKind_ = kind;
    emitCurrent();
    return true;
}

void BackgroundTaskCoordinator::finish(Kind kind)
{
    if (activeKind_ != kind) {
        return;
    }

    activeKind_ = Kind::None;
    emitCurrent();
}

BackgroundTaskCoordinator::Kind BackgroundTaskCoordinator::active() const
{
    return activeKind_;
}

bool BackgroundTaskCoordinator::isActive() const
{
    return activeKind_ != Kind::None;
}

bool BackgroundTaskCoordinator::isKindActive(Kind kind) const
{
    return activeKind_ == kind;
}

void BackgroundTaskCoordinator::resetSpeedTestProgress()
{
    speedTestTotalCount_ = 0;
    speedTestCompletedCount_ = 0;
    speedTestProgressServerName_.clear();
}

void BackgroundTaskCoordinator::setSpeedTestTotalCount(int total)
{
    speedTestTotalCount_ = total;
    speedTestCompletedCount_ = 0;
    speedTestProgressServerName_.clear();
}

void BackgroundTaskCoordinator::recordSpeedTestResult(const QString& serverName)
{
    if (speedTestTotalCount_ <= 0) {
        return;
    }
    speedTestCompletedCount_ = qMin(speedTestCompletedCount_ + 1, speedTestTotalCount_);
    speedTestProgressServerName_ = serverName;
}

void BackgroundTaskCoordinator::syncState()
{
    emitCurrent();
}

QString BackgroundTaskCoordinator::describe(Kind kind) const
{
    switch (kind) {
    case Kind::SpeedTest:
        if (speedTestTotalCount_ > 0) {
            if (!speedTestProgressServerName_.trimmed().isEmpty() && speedTestCompletedCount_ > 0) {
                return QCoreApplication::translate("AppBootstrap", "Testing %1 (%2/%3)")
                    .arg(speedTestProgressServerName_.trimmed())
                    .arg(speedTestCompletedCount_)
                    .arg(speedTestTotalCount_);
            }

            return QCoreApplication::translate("AppBootstrap", "Running URL Test (%1/%2)")
                .arg(speedTestCompletedCount_)
                .arg(speedTestTotalCount_);
        }
        return QCoreApplication::translate("AppBootstrap", "Running URL Test");
    case Kind::SubscriptionUpdate:
        return QCoreApplication::translate("AppBootstrap", "Updating subscriptions");
    case Kind::ProxyAvailabilityCheck:
        return QCoreApplication::translate("AppBootstrap", "Checking proxy availability");
    case Kind::CoreUpdate:
        return QCoreApplication::translate("AppBootstrap", "Updating core");
    case Kind::GeoUpdate:
        return QCoreApplication::translate("AppBootstrap", "Updating Geo resources");
    case Kind::None:
    default:
        return {};
    }
}

void BackgroundTaskCoordinator::emitCurrent()
{
    emit runningChanged(activeKind_ != Kind::None);
    emit descriptionChanged(describe(activeKind_));
}
