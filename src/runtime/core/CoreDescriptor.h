#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

struct CoreDescriptor {
    CoreType type = CoreType::Unknown;
    QString displayName;
    QList<ConfigType> supportedConfigTypes;
    QStringList executableNames;
    int protocolPriority = 0;
    QList<CoreType> auxiliaryTunCoreTypes;
    bool requiresLegacyGeoFiles = false;
};
