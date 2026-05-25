#pragma once

#include <functional>

#include <QHash>
#include <QString>
#include <QStringList>

#include "app/CoreStartupCheckpoint.h"

class QObject;

class CoreStartupChecklist final
{
public:
    using UpdateCallback = std::function<void(const QStringList&)>;
    using ClearCallback = std::function<void()>;

    CoreStartupChecklist(QObject* timerContext, UpdateCallback updateCallback, ClearCallback clearCallback);

    bool isEmpty() const;
    bool overlayRequested() const;
    bool visible() const;
    CoreStartupCheckpointStatus status(const QString& step) const;

    void prepare(bool tunEnabled, bool showOverlay);
    void requestOverlay();
    void setStatus(
        CoreStartupCheckpointStatus status,
        const QString& step,
        const QString& detail = {});
    void clear();
    void clearAfterStableRun(int delayMs, const std::function<bool()>& shouldClear);
    void keepForUserDismissal(const QString& step, const QString& detail);
    void markKeepOnNextStop();
    bool consumeKeepOnNextStop();

private:
    void syncOverlay();

    QObject* timerContext_ = nullptr;
    UpdateCallback updateCallback_;
    ClearCallback clearCallback_;
    QStringList steps_;
    QHash<QString, CoreStartupCheckpointStatus> stepStatus_;
    QHash<QString, QString> stepDetails_;
    bool overlayRequested_ = false;
    bool overlayShown_ = false;
    qint64 overlayShownAtMs_ = 0;
    int clearGeneration_ = 0;
    int stableRunGeneration_ = 0;
    bool keepOnNextStop_ = false;
};
