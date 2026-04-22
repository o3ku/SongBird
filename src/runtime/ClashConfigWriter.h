#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class ClashConfigWriter {
public:
    static OperationResult writeClientConfig(
        const Config& config,
        const VmessItem& server,
        const QString& outputPath,
        int controllerPort);

private:
    static QString patchYamlContent(
        const QString& source,
        const Config& config,
        int controllerPort);
    static QString formatScalar(const QString& value);
    static QString formatBool(bool value);
    static bool replaceOrAppend(
        QString& text,
        const QString& key,
        const QString& valueLine);
    static void removeTopLevelKey(QString& text, const QString& key);
};
