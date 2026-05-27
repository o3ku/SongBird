#pragma once

#include <QString>

#include "domain/models/VmessItem.h"
#include "runtime/CoreInfo.h"

struct SpeedTestRequestItem {
    QString indexId;
    QString displayName;
    ConfigType configType = ConfigType::Unknown;
    VmessItem runtimeServer;
    CoreInfo coreInfo;
};
