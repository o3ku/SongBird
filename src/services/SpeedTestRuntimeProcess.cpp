#include "services/SpeedTestRuntimeProcess.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>

namespace SpeedTestRuntimeProcess {

QString summarizeProcessOutput(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("<no output>");
    }

    QStringList lines = trimmed.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (QString& line : lines) {
        line = line.trimmed();
    }
    lines.erase(std::remove_if(lines.begin(), lines.end(), [](const QString& line) {
        return line.isEmpty();
    }), lines.end());

    const QString joined = lines.mid(0, 3).join(QStringLiteral(" | "));
    return joined.left(240);
}

QString readTextFileIfExists(const QString& path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QStringList buildCoreArguments(const CoreInfo& coreInfo, const QString& configFilePath)
{
    QStringList arguments = coreInfo.arguments;
    const QString normalizedPath = QDir::toNativeSeparators(configFilePath);

    if (!coreInfo.configPlaceholder.isEmpty()) {
        for (QString& argument : arguments) {
            if (argument == coreInfo.configPlaceholder) {
                argument = normalizedPath;
            }
        }
    }

    if (coreInfo.appendConfigArgument) {
        arguments << QStringLiteral("-config") << normalizedPath;
    }

    return arguments;
}

QString readProcessOutput(QProcess& process)
{
    return QString::fromUtf8(process.readAll()).trimmed();
}

void stopProcess(QProcess& process, int timeoutMs)
{
    process.terminate();
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(timeoutMs);
    }
}

} // namespace SpeedTestRuntimeProcess
