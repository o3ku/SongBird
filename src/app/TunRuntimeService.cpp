#include "app/TunRuntimeService.h"

#include "runtime/TunAdapterNames.h"

#include <QProcess>
#include <QString>

#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <cwchar>
#include <vector>
#endif

namespace {

bool isManagedTunAdapterName(const QString& name)
{
    return tunAdapterCleanupNames().contains(name);
}

QString cleanupTargetDescription()
{
    return tunAdapterCleanupNames().join(QStringLiteral("', '")).prepend(QChar('\'')).append(QChar('\''));
}

} // namespace

bool TunRuntimeService::isAdapterPresent() const
{
#if defined(Q_OS_WIN)
    ULONG bufferSize = 15 * 1024;
    std::vector<char> buffer(bufferSize);
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
        | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_UNICAST;
    for (int attempt = 0; attempt < 3; ++attempt) {
        IP_ADAPTER_ADDRESSES* addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        const ULONG status = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addresses, &bufferSize);
        if (status == ERROR_BUFFER_OVERFLOW) {
            buffer.resize(bufferSize);
            continue;
        }
        if (status != NO_ERROR) {
            return true;
        }
        for (IP_ADAPTER_ADDRESSES* cursor = addresses; cursor != nullptr; cursor = cursor->Next) {
            if (cursor->FriendlyName != nullptr
                && isManagedTunAdapterName(QString::fromWCharArray(cursor->FriendlyName))) {
                return true;
            }
        }
        return false;
    }
    return true;
#else
    return false;
#endif
}

OperationResult TunRuntimeService::removeStaleAdapterIfPresent() const
{
#ifndef Q_OS_WIN
    return OperationResult::ok(QStringLiteral("TUN adapter cleanup is only required on Windows."));
#else
    if (!isAdapterPresent()) {
        return OperationResult::ok(QStringLiteral("TUN preflight did not find removable stale SongBird TUN adapters."));
    }
    QProcess remover;
    remover.setProgram(QStringLiteral("powershell"));
    const QString quotedNames = tunAdapterCleanupNames()
        .replaceInStrings(QStringLiteral("'"), QStringLiteral("''"))
        .join(QStringLiteral("','"));
    remover.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-Command"),
        // Each adapter is removed inside its own try/catch so a failure on one name
        // does not abort the loop and leave the remaining adapters untried.
        QStringLiteral(
            "$ErrorActionPreference='Stop'; "
            "$names = @('%1'); "
            "$errors = @(); "
            "foreach ($name in $names) { "
            "  try { "
            "    $adapter = Get-NetAdapter -Name $name -ErrorAction SilentlyContinue; "
            "    if ($adapter) { "
            "      Disable-NetAdapter -Name $name -Confirm:$false -ErrorAction SilentlyContinue | Out-Null; "
            "      Remove-NetAdapter -Name $name -Confirm:$false -ErrorAction Stop; "
            "    }; "
            "  } catch { "
            "    $errors += ($name + ': ' + $_.Exception.Message); "
            "  } "
            "}; "
            "$deadline = (Get-Date).AddSeconds(30); "
            "while ((Get-Date) -lt $deadline) { "
            "  $remaining = @($names | Where-Object { Get-NetAdapter -Name $_ -ErrorAction SilentlyContinue }); "
            "  if ($remaining.Count -eq 0) { "
            "    Start-Sleep -Milliseconds 800; "
            "    $remaining = @($names | Where-Object { Get-NetAdapter -Name $_ -ErrorAction SilentlyContinue }); "
            "    if ($remaining.Count -eq 0) { "
            "      Write-Output \"SongBird TUN adapters are clear.\"; "
            "      exit 0; "
            "    } "
            "  }; "
            "  Start-Sleep -Milliseconds 300; "
            "}; "
            "$remaining = @($names | Where-Object { Get-NetAdapter -Name $_ -ErrorAction SilentlyContinue }); "
            "$report = \"TUN adapters still exist after cleanup wait: \" + ($remaining -join ', '); "
            "if ($errors.Count -gt 0) { $report += ' | removal errors: ' + ($errors -join '; '); }; "
            "Write-Output $report; "
            "exit 2").arg(quotedNames)
    });
    remover.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
    remover.start();
    const bool finished = remover.waitForFinished(32000);
    if (!finished) {
        remover.kill();
        remover.waitForFinished(500);
        return OperationResult::fail(
            QStringLiteral("Timed out while removing SongBird TUN adapters (%1).").arg(cleanupTargetDescription()));
    }
    if (remover.exitStatus() != QProcess::NormalExit) {
        return OperationResult::fail(
            QStringLiteral("Aborted while removing SongBird TUN adapters (%1).").arg(cleanupTargetDescription()));
    }
    const QString removerOutput = QString::fromLocal8Bit(remover.readAll()).trimmed();
    if (remover.exitCode() != 0) {
        return OperationResult::fail(
            removerOutput.isEmpty()
                ? QStringLiteral("Failed to remove SongBird TUN adapters (%1).").arg(cleanupTargetDescription())
                : QStringLiteral("Failed to remove SongBird TUN adapters (%1): %2")
                    .arg(cleanupTargetDescription(), removerOutput));
    }
    if (isAdapterPresent()) {
        return OperationResult::fail(
            QStringLiteral("A SongBird TUN adapter is still present after cleanup (%1).").arg(cleanupTargetDescription()));
    }
    return OperationResult::ok(
        removerOutput.isEmpty()
            ? QStringLiteral("Cleaned any stale SongBird TUN adapters (%1).").arg(cleanupTargetDescription())
            : removerOutput);
#endif
}
