#pragma once

#include <QObject>
#include <QProcess>

#include "runtime/ICoreProcessHost.h"

class CoreLifecycleService final : public QObject {
    Q_OBJECT

public:
    explicit CoreLifecycleService(ICoreProcessHost& host, QObject* parent = nullptr);

    OperationResult start(const CoreInfo& coreInfo, const QString& configPath);
    OperationResult stop();
    OperationResult reload();
    bool isRunning() const;

signals:
    void outputReceived(const QString& line);
    void exited(int exitCode, QProcess::ExitStatus status, bool stopRequested);

private:
    ICoreProcessHost& host_;
};
