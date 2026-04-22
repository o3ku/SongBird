#pragma once

#include <QString>
#include <QtGlobal>

#include "domain/models/VmessItem.h"

struct StatisticsSessionState {
    bool enabled = false;
    bool running = false;
    bool runtimeConfigReady = false;
    bool pollingAvailable = false;
    bool externallyManaged = false;
    CoreType coreType = CoreType::Unknown;
    QString activeServerId;
    int statePort = 0;
    int refreshRateSeconds = 0;
    quint64 lastDeltaUp = 0;
    quint64 lastDeltaDown = 0;
};
