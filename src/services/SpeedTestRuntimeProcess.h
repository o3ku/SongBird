#pragma once

#include <QString>
#include <QStringList>

#include "runtime/CoreInfo.h"

class QProcess;

namespace SpeedTestRuntimeProcess {

QString summarizeProcessOutput(const QString& value);
QString readTextFileIfExists(const QString& path);
QStringList buildCoreArguments(const CoreInfo& coreInfo, const QString& configFilePath);
QString readProcessOutput(QProcess& process);
void stopProcess(QProcess& process, int timeoutMs = 3000);

} // namespace SpeedTestRuntimeProcess
