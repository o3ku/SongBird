#pragma once

#include "common/OperationResult.h"
#include "domain/models/Config.h"

class ProxyAvailabilityCheckService {
public:
    OperationResult check(const Config& config) const;
};
