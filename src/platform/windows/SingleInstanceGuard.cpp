#include "platform/windows/SingleInstanceGuard.h"

#include <QSharedMemory>

#include <utility>

SingleInstanceGuard::SingleInstanceGuard(QString key)
    : key_(std::move(key))
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    if (sharedMemory_ && sharedMemory_->isAttached()) {
        sharedMemory_->detach();
    }
}

bool SingleInstanceGuard::tryAcquire()
{
    sharedMemory_ = std::make_unique<QSharedMemory>(key_);
    if (sharedMemory_->create(1)) {
        return true;
    }

    if (sharedMemory_->error() == QSharedMemory::AlreadyExists) {
        return false;
    }

    sharedMemory_->detach();
    return sharedMemory_->create(1);
}
