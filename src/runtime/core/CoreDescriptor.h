#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

// A geodata database a core loads at startup. Cores ship without geodata, so the app has to place
// the file on disk before the core starts: Xray refuses to run without it, and mihomo would
// otherwise fetch it inside the startup preflight window, which costs tens of seconds.
struct CoreGeoFileRequirement {
    // Name the core looks for on disk. Not always the upstream name: mihomo reads "GeoSite.dat"
    // while the release publishes "geosite.dat".
    QString fileName;
    // Name in the `latest` release of `repositoryPath`.
    QString sourceFileName;
    // GitHub repository publishing the file under its `latest` release.
    QString repositoryPath;
};

struct CoreDescriptor {
    CoreType type = CoreType::Unknown;
    QString displayName;
    QList<ConfigType> supportedConfigTypes;
    QStringList executableNames;
    int protocolPriority = 0;
    QList<CoreType> auxiliaryTunCoreTypes;
    // Geo databases the core reads at startup. Empty when it needs none.
    QList<CoreGeoFileRequirement> geoFileRequirements;
    // Sub-directory of the application directory that the core is told to keep its own runtime data
    // in. Empty when the core reads its data from the directory it runs in.
    QString dataDirectoryName;
};
