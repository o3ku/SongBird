#pragma once

#include "domain/models/Config.h"

class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;

    virtual Config load() = 0;
    virtual bool save(const Config& config) = 0;
};
