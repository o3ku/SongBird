#include "app/AppUpdateInstallService.h"

#include "common/AppPlatform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

#include "app/MacAppUpdateScript.h"
#include "common/AppPaths.h"

#if defined(Q_OS_MACOS)
namespace {

bool launchMacUpdate(const QString& diskImagePath, const QString& activeConfigPath)
{
    const QFileInfo executable(QCoreApplication::applicationFilePath());
    QDir bundle = executable.dir();
    if (bundle.dirName() != QStringLiteral("MacOS") || !bundle.cdUp()
        || bundle.dirName() != QStringLiteral("Contents") || !bundle.cdUp()
        || !bundle.dirName().endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) {
        return false;
    }
    const QFileInfo bundleInfo(bundle.absolutePath());
    if (bundleInfo.isSymLink() || !QFileInfo(bundleInfo.absolutePath()).isWritable()) {
        return false;
    }

    const QString dataDirectory = AppPaths::userDataDirectory();
    const QString updates = QDir(dataDirectory).filePath(QStringLiteral("updates"));
    if (!QDir().mkpath(updates)) {
        return false;
    }
    QTemporaryDir work(QDir(updates).filePath(QStringLiteral("install-XXXXXX")));
    if (!work.isValid()) {
        return false;
    }

    const QString logPath = QDir(work.path()).filePath(QStringLiteral("install.log"));
    const auto title = QCoreApplication::translate("AppBootstrap", "Update Failed");
    const auto failureMessage = QCoreApplication::translate(
        "AppBootstrap", "SongBird could not install the update. See the update log at %1.").arg(logPath);

    QStringList originalArguments = QCoreApplication::arguments();
    const QString configPath = QFileInfo(activeConfigPath).absoluteFilePath();
    const QString legacyPrefix = executable.absolutePath() + QLatin1Char('/');
    const QString installedConfigPath = configPath.startsWith(legacyPrefix)
        ? QDir(dataDirectory).filePath(configPath.mid(legacyPrefix.size()))
        : configPath;

    QStringList arguments{
        QStringLiteral("-c"),
        QString::fromLatin1(kMacAppUpdateScript),
        QStringLiteral("songbird-update"),
        QString::number(QCoreApplication::applicationPid()),
        QFileInfo(diskImagePath).absoluteFilePath(),
        bundle.absolutePath(),
        dataDirectory,
        work.path(),
        title,
        failureMessage,
        QStringLiteral("--config=") + installedConfigPath,
        QStringLiteral("--config=") + configPath
    };
    // The updated process receives the resolved path, including when the original path was
    // relative to a terminal working directory or moved out of the old bundle.
    originalArguments.removeFirst();
    for (int i = 0; i < originalArguments.size(); ++i) {
        const QString& argument = originalArguments.at(i);
        if (argument == QStringLiteral("--config") || argument == QStringLiteral("--restart-wait-pid")) {
            ++i;
        } else if (!argument.startsWith(QStringLiteral("--config="))
                   && !argument.startsWith(QStringLiteral("--restart-wait-pid="))) {
            arguments.append(argument);
        }
    }
    const bool started = QProcess::startDetached(QStringLiteral("/bin/bash"), arguments);
    if (started) {
        work.setAutoRemove(false);
    }
    return started;
}

} // namespace
#endif

AppUpdateInstallService::AppUpdateInstallService(ElevatedLauncher elevatedLauncher)
    : elevatedLauncher_(std::move(elevatedLauncher))
{
}

bool AppUpdateInstallService::launchUpdateScript(const QString& packagePath, const QString& configPath) const
{
    const QString currentExecutablePath = QCoreApplication::applicationFilePath();
    const QFileInfo currentExecutableInfo(currentExecutablePath);
    const QFileInfo newExecutableInfo(packagePath);
    if (!currentExecutableInfo.exists() || !newExecutableInfo.exists()) {
        return false;
    }

#if defined(Q_OS_MACOS)
    return newExecutableInfo.suffix().compare(QStringLiteral("dmg"), Qt::CaseInsensitive) == 0
        && launchMacUpdate(packagePath, configPath);
#else
    Q_UNUSED(configPath);
    QDir updateDirectory = newExecutableInfo.dir();
    if (!updateDirectory.exists()) {
        return false;
    }

    const QString scriptPath = updateDirectory.filePath(QStringLiteral("update-songbird.ps1"));
    const QString backupExecutablePath = currentExecutableInfo.dir().filePath(
        QStringLiteral("%1.old.exe").arg(currentExecutableInfo.completeBaseName()));
    const QString logPath = updateDirectory.filePath(QStringLiteral("update-songbird.log"));

    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream stream(&scriptFile);
    stream << "param(\n";
    stream << "  [int]$OldPid,\n";
    stream << "  [string]$CurrentExe,\n";
    stream << "  [string]$NewExe,\n";
    stream << "  [string]$BackupExe,\n";
    stream << "  [string]$LogPath\n";
    stream << ")\n";
    stream << "$ErrorActionPreference = 'Stop'\n";
    stream << "function Write-UpdateLog([string]$Message) {\n";
    stream << "  try { Add-Content -LiteralPath $LogPath -Value ((Get-Date -Format o) + ' ' + $Message) -Encoding UTF8 } catch {}\n";
    stream << "}\n";
    stream << "try {\n";
    stream << "  Write-UpdateLog 'Waiting for SongBird to exit.'\n";
    stream << "  try { Wait-Process -Id $OldPid -Timeout 30 -ErrorAction SilentlyContinue } catch {}\n";
    stream << "  Start-Sleep -Milliseconds 500\n";
    stream << "  if (!(Test-Path -LiteralPath $NewExe)) { throw 'New executable was not found.' }\n";
    stream << "  if (Test-Path -LiteralPath $BackupExe) { Remove-Item -LiteralPath $BackupExe -Force }\n";
    stream << "  Move-Item -LiteralPath $CurrentExe -Destination $BackupExe -Force\n";
    stream << "  try {\n";
    stream << "    Move-Item -LiteralPath $NewExe -Destination $CurrentExe -Force\n";
    stream << "  } catch {\n";
    stream << "    if (Test-Path -LiteralPath $BackupExe) { Move-Item -LiteralPath $BackupExe -Destination $CurrentExe -Force }\n";
    stream << "    throw\n";
    stream << "  }\n";
    stream << "  Write-UpdateLog 'Starting updated SongBird.'\n";
    stream << "  Start-Process -FilePath $CurrentExe -WorkingDirectory (Split-Path -Parent $CurrentExe)\n";
    stream << "} catch {\n";
    stream << "  Write-UpdateLog ('Update failed: ' + $_.Exception.Message)\n";
    stream << "  if (Test-Path -LiteralPath $CurrentExe) { Start-Process -FilePath $CurrentExe -WorkingDirectory (Split-Path -Parent $CurrentExe) }\n";
    stream << "}\n";
    scriptFile.close();

    const QStringList arguments{
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-File"),
        scriptPath,
        QStringLiteral("-OldPid"),
        QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("-CurrentExe"),
        currentExecutablePath,
        QStringLiteral("-NewExe"),
        packagePath,
        QStringLiteral("-BackupExe"),
        backupExecutablePath,
        QStringLiteral("-LogPath"),
        logPath
    };

    if (isWindowsPlatform() && !isProcessElevated() && !QFileInfo(currentExecutablePath).isWritable()) {
        return elevatedLauncher_ && elevatedLauncher_(QStringLiteral("powershell.exe"), arguments);
    }

    return QProcess::startDetached(QStringLiteral("powershell.exe"), arguments, updateDirectory.absolutePath());
#endif
}
