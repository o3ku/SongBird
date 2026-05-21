#include "app/SingleInstanceBootstrap.h"

#include "platform/windows/SingleInstanceGuard.h"

SingleInstanceBootstrap* SingleInstanceBootstrap::currentInstance_ = nullptr;

SingleInstanceBootstrap::SingleInstanceBootstrap()
    : guard_(std::make_unique<SingleInstanceGuard>(QStringLiteral("SongBird-single-instance")))
{
    currentInstance_ = this;
}

SingleInstanceBootstrap::~SingleInstanceBootstrap()
{
    if (currentInstance_ == this) {
        currentInstance_ = nullptr;
    }
}

bool SingleInstanceBootstrap::tryAcquire()
{
    return guard_ != nullptr && guard_->tryAcquire();
}

void SingleInstanceBootstrap::release()
{
    guard_.reset();
}

void SingleInstanceBootstrap::releaseCurrentInstance()
{
    if (currentInstance_ != nullptr) {
        currentInstance_->release();
    }
}

bool SingleInstanceBootstrap::reacquireCurrentInstance()
{
    if (currentInstance_ == nullptr) {
        return false;
    }

    if (currentInstance_->guard_ == nullptr) {
        currentInstance_->guard_ = std::make_unique<SingleInstanceGuard>(QStringLiteral("SongBird-single-instance"));
    }
    return currentInstance_->tryAcquire();
}
