#pragma once

#include <memory>

#include <QString>

class QSharedMemory;

class SingleInstanceGuard {
public:
    explicit SingleInstanceGuard(QString key);
    ~SingleInstanceGuard();

    bool tryAcquire();

private:
    QString key_;
    std::unique_ptr<QSharedMemory> sharedMemory_;
};
