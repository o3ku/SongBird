#include "app/TunRuntimeService.h"

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

bool TunRuntimeService::isAdapterPresent() const
{
#if defined(Q_OS_WIN)
    ULONG bufferSize = 15 * 1024;
    std::vector<char> buffer(bufferSize);
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
        | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_UNICAST;
    static const wchar_t kTarget[] = L"singbox_tun";
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
            if (cursor->FriendlyName != nullptr && wcscmp(cursor->FriendlyName, kTarget) == 0) {
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
        return OperationResult::ok(QStringLiteral("TUN preflight did not find a removable stale 'singbox_tun' adapter."));
    }
    QProcess remover;
    remover.setProgram(QStringLiteral("powershell"));
    remover.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-Command"),
        QStringLiteral(
            "$ErrorActionPreference='Stop'; "
            "$name = 'singbox_tun'; "
            "$adapter = Get-NetAdapter -Name $name -ErrorAction SilentlyContinue; "
            "if ($adapter) { "
            "  Disable-NetAdapter -Name $name -Confirm:$false -ErrorAction SilentlyContinue | Out-Null; "
            "  Remove-NetAdapter -Name $name -Confirm:$false -ErrorAction Stop; "
            "}; "
            "$deadline = (Get-Date).AddSeconds(30); "
            "while ((Get-Date) -lt $deadline) { "
            "  if (-not (Get-NetAdapter -Name $name -ErrorAction SilentlyContinue)) { "
            "    Start-Sleep -Milliseconds 800; "
            "    if (-not (Get-NetAdapter -Name $name -ErrorAction SilentlyContinue)) { "
            "      Write-Output \"TUN adapter '$name' is clear.\"; "
            "      exit 0; "
            "    } "
            "  }; "
            "  Start-Sleep -Milliseconds 300; "
            "}; "
            "Write-Output \"TUN adapter '$name' still exists after cleanup wait.\"; "
            "exit 2")
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
            QStringLiteral("Timed out while removing the 'singbox_tun' adapter."));
    }
    if (remover.exitStatus() != QProcess::NormalExit) {
        return OperationResult::fail(
            QStringLiteral("Aborted while removing the 'singbox_tun' adapter."));
    }
    const QString removerOutput = QString::fromLocal8Bit(remover.readAll()).trimmed();
    if (remover.exitCode() != 0) {
        return OperationResult::fail(
            removerOutput.isEmpty()
                ? QStringLiteral("Failed to remove the 'singbox_tun' adapter.")
                : QStringLiteral("Failed to remove the 'singbox_tun' adapter: %1").arg(removerOutput));
    }
    if (isAdapterPresent()) {
        return OperationResult::fail(
            QStringLiteral("The 'singbox_tun' adapter is still present after cleanup."));
    }
    return OperationResult::ok(
        removerOutput.isEmpty()
            ? QStringLiteral("Cleaned any stale 'singbox_tun' adapter.")
            : removerOutput);
#endif
}
