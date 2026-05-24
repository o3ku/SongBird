#include "app/AppUpdateInstallService.h"

#include "common/AppPlatform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

AppUpdateInstallService::AppUpdateInstallService(ElevatedLauncher elevatedLauncher)
    : elevatedLauncher_(std::move(elevatedLauncher))
{
}

bool AppUpdateInstallService::launchUpdateScript(const QString& newExecutablePath) const
{
    const QString currentExecutablePath = QCoreApplication::applicationFilePath();
    const QFileInfo currentExecutableInfo(currentExecutablePath);
    const QFileInfo newExecutableInfo(newExecutablePath);
    if (!currentExecutableInfo.exists() || !newExecutableInfo.exists()) {
        return false;
    }

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
        newExecutablePath,
        QStringLiteral("-BackupExe"),
        backupExecutablePath,
        QStringLiteral("-LogPath"),
        logPath
    };

    if (isWindowsPlatform() && !isProcessElevated() && !QFileInfo(currentExecutablePath).isWritable()) {
        return elevatedLauncher_ && elevatedLauncher_(QStringLiteral("powershell.exe"), arguments);
    }

    return QProcess::startDetached(QStringLiteral("powershell.exe"), arguments, updateDirectory.absolutePath());
}
