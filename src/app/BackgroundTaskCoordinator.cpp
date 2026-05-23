#include "app/BackgroundTaskCoordinator.h"

#include <utility>

#include <QCoreApplication>

BackgroundTaskCoordinator::BackgroundTaskCoordinator(QObject* parent)
    : QObject(parent)
{
}

bool BackgroundTaskCoordinator::Token::isValid() const
{
    return kind != Kind::None && generation != 0;
}

void BackgroundTaskCoordinator::setBlockingPredicate(std::function<bool()> predicate)
{
    blockingPredicate_ = std::move(predicate);
}

BackgroundTaskCoordinator::Token BackgroundTaskCoordinator::tryBeginUserTask(Kind kind)
{
    return tryBegin(kind, StartScope::UserTask);
}

BackgroundTaskCoordinator::Token BackgroundTaskCoordinator::tryBeginStartupTask(Kind kind)
{
    return tryBegin(kind, StartScope::StartupTask);
}

BackgroundTaskCoordinator::Token BackgroundTaskCoordinator::tryBegin(Kind kind, StartScope scope)
{
    if (kind == Kind::None) {
        return {};
    }

    if (scope == StartScope::UserTask && blockingPredicate_ && blockingPredicate_()) {
        emit blockedByCoreStartup();
        return {};
    }

    if (activeKind_ != Kind::None) {
        return {};
    }

    activeKind_ = kind;
    activeGeneration_ = ++generation_;
    emitCurrent();
    return Token{kind, activeGeneration_};
}

bool BackgroundTaskCoordinator::finish(const Token& token)
{
    if (!isCurrent(token)) {
        return false;
    }

    activeKind_ = Kind::None;
    activeGeneration_ = 0;
    emitCurrent();
    return true;
}

void BackgroundTaskCoordinator::cancelActive()
{
    if (activeKind_ == Kind::None) {
        return;
    }

    activeKind_ = Kind::None;
    activeGeneration_ = 0;
    ++generation_;
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

bool BackgroundTaskCoordinator::isCurrent(const Token& token) const
{
    return token.isValid()
        && activeKind_ == token.kind
        && activeGeneration_ == token.generation;
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
