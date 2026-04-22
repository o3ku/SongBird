#include "app/SingleInstanceBootstrap.h"

#include "platform/windows/SingleInstanceGuard.h"

SingleInstanceBootstrap::SingleInstanceBootstrap()
    : guard_(std::make_unique<SingleInstanceGuard>(QStringLiteral("v2rayq-single-instance")))
{
}

SingleInstanceBootstrap::~SingleInstanceBootstrap() = default;

bool SingleInstanceBootstrap::tryAcquire()
{
    return guard_ != nullptr && guard_->tryAcquire();
}
