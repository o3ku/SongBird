#pragma once

#include <memory>

class SingleInstanceGuard;

class SingleInstanceBootstrap {
public:
    SingleInstanceBootstrap();
    ~SingleInstanceBootstrap();

    bool tryAcquire();

private:
    std::unique_ptr<SingleInstanceGuard> guard_;
};
