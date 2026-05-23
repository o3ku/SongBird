#pragma once

#include <functional>

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include "app/CoreStartupCheckpoint.h"

class CoreStartupChecklistController : public QObject {
    Q_OBJECT

public:
    explicit CoreStartupChecklistController(QObject* parent = nullptr);

    void reset(const QStringList& steps, bool showOverlay);
    void setCheckpointStatus(
        CoreStartupCheckpointStatus status,
        const QString& step,
        const QString& detail = {});
    void clear();
    void clearAfterStableRun(int delayMs, std::function<bool()> stillStablePredicate);

    void requestOverlay();

    bool hasSteps() const;
    bool overlayRequested() const;
    bool overlayActive() const;
    CoreStartupCheckpointStatus statusOf(const QString& step) const;

    void setKeepOnNextStop(bool keep);
    bool consumeKeepOnNextStop();

signals:
    void itemsChanged(const QStringList& items);
    void cleared();

private:
    void syncOverlay();

    QStringList steps_;
    QHash<QString, CoreStartupCheckpointStatus> stepStatus_;
    QHash<QString, QString> stepDetails_;
    bool overlayRequested_ = false;
    bool overlayShown_ = false;
    qint64 overlayShownAtMs_ = 0;
    int overlayClearGeneration_ = 0;
    int stableRunGeneration_ = 0;
    bool keepOnNextStop_ = false;
};
