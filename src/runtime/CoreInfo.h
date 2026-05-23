#pragma once

#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

struct CoreInfo {
    CoreType type = CoreType::Unknown;
    QString program;
    QStringList arguments;
    QString workingDirectory;
    bool captureOutput = true;
    bool asyncStart = false;
    bool appendConfigArgument = true;
    QString configPlaceholder = QStringLiteral("{config}");
};
