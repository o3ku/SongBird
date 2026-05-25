#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class XrayStreamConfigFragments {
public:
    static QJsonObject buildStreamSettings(const Config& config, const VmessItem& server);
};
