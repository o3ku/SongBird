#include "platform/windows/WindowsUwpLoopbackSupport.h"

#include <algorithm>
#include <utility>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QIODevice>
#include <QObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

namespace {

WindowsUwpPackageInfo packageFromJson(const QJsonObject& object)
{
    WindowsUwpPackageInfo package;
    package.name = object.value(QStringLiteral("Name")).toString().trimmed();
    package.packageFamilyName = object.value(QStringLiteral("PackageFamilyName")).toString().trimmed();
    package.packageFullName = object.value(QStringLiteral("PackageFullName")).toString().trimmed();
    package.publisher = object.value(QStringLiteral("Publisher")).toString().trimmed();
    package.installLocation = object.value(QStringLiteral("InstallLocation")).toString().trimmed();
    return package;
}

void appendPackageFromJson(
    QList<WindowsUwpPackageInfo>& packages,
    QSet<QString>& seenPackageFamilyNames,
    const QJsonValue& value)
{
    if (!value.isObject()) {
        return;
    }

    WindowsUwpPackageInfo package = packageFromJson(value.toObject());
    if (package.packageFamilyName.trimmed().isEmpty()) {
        return;
    }
    if (seenPackageFamilyNames.contains(package.packageFamilyName)) {
        return;
    }

    seenPackageFamilyNames.insert(package.packageFamilyName);
    packages.append(std::move(package));
}

QString psSingleQuoted(QString value)
{
    value.replace(QChar('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(value);
}

} // namespace

QString WindowsUwpLoopbackSupport::checkNetIsolationPath()
{
    const QString systemRoot = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"));
    return QDir::toNativeSeparators(QDir(systemRoot).filePath(QStringLiteral("System32/CheckNetIsolation.exe")));
}

QList<WindowsUwpPackageInfo> WindowsUwpLoopbackSupport::parsePackageListJson(
    const QString& output,
    OperationResult* result)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(output.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (result != nullptr) {
            *result = OperationResult::fail(
                QObject::tr("Failed to parse UWP package list: %1").arg(parseError.errorString()));
        }
        return {};
    }

    QList<WindowsUwpPackageInfo> packages;
    QSet<QString> seenPackageFamilyNames;
    if (document.isArray()) {
        const QJsonArray array = document.array();
        packages.reserve(array.size());
        for (const QJsonValue& value : array) {
            appendPackageFromJson(packages, seenPackageFamilyNames, value);
        }
    } else if (document.isObject()) {
        appendPackageFromJson(packages, seenPackageFamilyNames, document.object());
    }

    if (result != nullptr) {
        *result = OperationResult::ok();
    }
    return packages;
}

QSet<QString> WindowsUwpLoopbackSupport::parseExemptPackageFamilyNames(const QString& output)
{
    QSet<QString> packageNames;
    const QRegularExpression nameLine(QStringLiteral("^\\s*Name:\\s*(.+?)\\s*$"));
    const QStringList lines = output.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QRegularExpressionMatch match = nameLine.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        const QString packageName = match.captured(1).trimmed();
        if (!packageName.isEmpty() && packageName != QStringLiteral("AppContainer NOT FOUND")) {
            packageNames.insert(packageName);
        }
    }

    return packageNames;
}

OperationResult WindowsUwpLoopbackSupport::writeElevatedLoopbackScript(
    const QString& scriptPath,
    const QString& donePath,
    const QString& logPath,
    const QHash<QString, bool>& enabledByPackageFamilyName)
{
    QSaveFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::fail(QObject::tr("Failed to create temporary loopback script."));
    }

    QStringList packages = enabledByPackageFamilyName.keys();
    std::sort(packages.begin(), packages.end());

    QTextStream stream(&script);
#if QT_VERSION_MAJOR >= 6
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    stream << "$ErrorActionPreference = 'Stop'\n";
    stream << "$check = " << psSingleQuoted(checkNetIsolationPath()) << "\n";
    stream << "$log = " << psSingleQuoted(QDir::toNativeSeparators(logPath)) << "\n";
    stream << "$done = " << psSingleQuoted(QDir::toNativeSeparators(donePath)) << "\n";
    stream << "try {\n";
    stream << "  \"SongBird UWP loopback update started\" | Out-File -FilePath $log -Encoding UTF8\n";
    for (const QString& packageFamilyName : packages) {
        const QString operation = enabledByPackageFamilyName.value(packageFamilyName)
            ? QStringLiteral("-a")
            : QStringLiteral("-d");
        stream << "  & $check LoopbackExempt " << operation
               << " ('-n=' + " << psSingleQuoted(packageFamilyName) << ")"
               << " *>&1 | Out-File -FilePath $log -Append -Encoding UTF8\n";
        stream << "  if ($LASTEXITCODE -ne 0) { throw 'CheckNetIsolation failed for "
               << packageFamilyName << "' }\n";
    }
    stream << "  '0' | Out-File -FilePath $done -Encoding ASCII\n";
    stream << "} catch {\n";
    stream << "  $_ | Out-File -FilePath $log -Append -Encoding UTF8\n";
    stream << "  '1' | Out-File -FilePath $done -Encoding ASCII\n";
    stream << "}\n";
    if (!script.commit()) {
        return OperationResult::fail(QObject::tr("Failed to write temporary loopback script."));
    }

    return OperationResult::ok();
}
