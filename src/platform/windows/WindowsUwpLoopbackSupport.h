#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include "common/OperationResult.h"
#include "platform/windows/WindowsUwpLoopbackService.h"

namespace WindowsUwpLoopbackSupport {

QString checkNetIsolationPath();
QList<WindowsUwpPackageInfo> parsePackageListJson(const QString& output, OperationResult* result);
QSet<QString> parseExemptPackageFamilyNames(const QString& output);
OperationResult writeElevatedLoopbackScript(
    const QString& scriptPath,
    const QString& donePath,
    const QString& logPath,
    const QHash<QString, bool>& enabledByPackageFamilyName);

} // namespace WindowsUwpLoopbackSupport
