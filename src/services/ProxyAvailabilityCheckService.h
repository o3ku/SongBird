#pragma once

#include <QString>

#include "common/OperationResult.h"

struct ProxyAvailabilityCheckConfig {
    int localPort = 0;
    bool tunEnabled = false;
    QString speedPingTestUrl;
};

class ProxyAvailabilityCheckService {
public:
    OperationResult check(const ProxyAvailabilityCheckConfig& config) const;
};
