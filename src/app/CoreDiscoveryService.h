#pragma once

#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

class CoreDiscoveryService
{
public:
    QStringList resolveCoreCandidates(CoreType coreType, const QString& configPath) const;
    QString locateFirstExistingFile(const QStringList& candidates) const;
    QString expectedCoreFilesText(const QStringList& candidates) const;
    QString detectCoreVersion(CoreType coreType, const QString& program) const;
};
