#pragma once

#include <QObject>

#include "runtime/ICoreProcessHost.h"

// Thin adapter that turns ICoreProcessHost's callback API into Qt signals.
//
// This class is exercised only by the end-to-end smoke test
// (tests/EndToEndSmokeTests.cpp) and is deliberately not part of the
// application build -- the production path drives ICoreProcessHost through
// ProxySession.
//
// Only the two events that test consumes are exposed. Note that `started` is
// waited on through QSignalSpy, which resolves the signal by *string* name, so
// it will not appear in a grep for CoreLifecycleService:: -- removing it looks
// safe and is not.
class CoreLifecycleService final : public QObject {
    Q_OBJECT

public:
    explicit CoreLifecycleService(ICoreProcessHost& host, QObject* parent = nullptr);

    OperationResult start(const CoreInfo& coreInfo, const QString& configPath);
    OperationResult stop(bool immediate = false);
    OperationResult reload();
    bool isRunning() const;

signals:
    void outputReceived(const QString& line);
    void started(const QString& message);

private:
    ICoreProcessHost& host_;
};
