#pragma once

#include <QString>
#include <QStringList>

struct CoreInfo {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    bool captureOutput = true;
    bool asyncStart = false;
    bool appendConfigArgument = true;
    QString configPlaceholder = QStringLiteral("{config}");
};
