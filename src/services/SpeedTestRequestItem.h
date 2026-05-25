#pragma once

#include "domain/models/VmessItem.h"
#include "runtime/CoreInfo.h"

struct SpeedTestRequestItem {
    VmessItem server;
    VmessItem runtimeServer;
    CoreInfo coreInfo;
};
