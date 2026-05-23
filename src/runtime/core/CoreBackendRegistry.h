#pragma once

#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"

class ICoreBackend;

QList<const ICoreBackend*> coreBackends();
const ICoreBackend* coreBackend(CoreType coreType);
const ICoreBackend* coreBackendForExecutableName(const QString& fileName);
QString normalizeCoreVersionTag(QString value);
