#pragma once

#include <memory>

class SingleInstanceGuard;

class SingleInstanceBootstrap {
public:
    SingleInstanceBootstrap();
    ~SingleInstanceBootstrap();

    bool tryAcquire();
    void release();

    static void releaseCurrentInstance();
    static bool reacquireCurrentInstance();

private:
    static SingleInstanceBootstrap* currentInstance_;
    std::unique_ptr<SingleInstanceGuard> guard_;
};
