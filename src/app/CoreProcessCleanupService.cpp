#include "app/CoreProcessCleanupService.h"

#include <QCoreApplication>
#include <QSet>
#include <QString>

#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <cwchar>
#include <vector>
#endif

#include "common/CorePidFile.h"

namespace {

#if defined(Q_OS_WIN)
bool isManagedCoreProcessName(const wchar_t* processName)
{
    static const wchar_t* kCoreProcessNames[] = {
        L"xray.exe",
        L"sing-box-client.exe",
        L"sing-box.exe",
        L"tuic.exe",
    };

    if (processName == nullptr) {
        return false;
    }

    for (const wchar_t* targetName : kCoreProcessNames) {
        if (_wcsicmp(processName, targetName) == 0) {
            return true;
        }
    }
    return false;
}

QString processNameForPid(DWORD pid)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    QString name;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                name = QString::fromWCharArray(entry.szExeFile);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return name;
}

QSet<DWORD> tcpListeningPidsForPorts(const QSet<quint16>& ports)
{
    QSet<DWORD> pids;
    if (ports.isEmpty()) {
        return pids;
    }

    ULONG tableSize = 0;
    DWORD result = GetExtendedTcpTable(
        nullptr,
        &tableSize,
        FALSE,
        AF_INET,
        TCP_TABLE_OWNER_PID_LISTENER,
        0);
    if (result != ERROR_INSUFFICIENT_BUFFER || tableSize == 0) {
        return pids;
    }

    std::vector<char> buffer(tableSize);
    auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
    result = GetExtendedTcpTable(
        table,
        &tableSize,
        FALSE,
        AF_INET,
        TCP_TABLE_OWNER_PID_LISTENER,
        0);
    if (result != NO_ERROR) {
        return pids;
    }

    for (DWORD index = 0; index < table->dwNumEntries; ++index) {
        const MIB_TCPROW_OWNER_PID& row = table->table[index];
        const quint16 port = ntohs(static_cast<u_short>(row.dwLocalPort));
        if (ports.contains(port) && row.dwOwningPid > 0) {
            pids.insert(row.dwOwningPid);
        }
    }

    return pids;
}

QStringList terminateCorePids(const QSet<qint64>& pids)
{
    QStringList terminatedProcesses;
    for (qint64 pidValue : pids) {
        if (pidValue <= 0 || pidValue == QCoreApplication::applicationPid()) {
            continue;
        }

        const DWORD pid = static_cast<DWORD>(pidValue);
        const QString processName = processNameForPid(pid);
        if (processName.isEmpty()
            || !isManagedCoreProcessName(reinterpret_cast<const wchar_t*>(processName.utf16()))) {
            continue;
        }

        HANDLE processHandle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (processHandle == nullptr) {
            continue;
        }

        if (TerminateProcess(processHandle, 1)) {
            WaitForSingleObject(processHandle, 1500);
            terminatedProcesses.append(QStringLiteral("%1(%2)").arg(processName).arg(pidValue));
        }
        CloseHandle(processHandle);
    }
    return terminatedProcesses;
}
#endif

} // namespace

QStringList CoreProcessCleanupService::cleanupOrphanCoreProcesses() const
{
#if defined(Q_OS_WIN)
    const QSet<qint64> recordedPids = readCorePids();
    QStringList terminatedProcesses = terminateCorePids(recordedPids);
    writeCorePids(QSet<qint64>());
    return terminatedProcesses;
#else
    return {};
#endif
}

QStringList CoreProcessCleanupService::cleanupCoreProcessesUsingConfiguredPorts(
    const Config& config,
    int locationProbePortOffset) const
{
#if defined(Q_OS_WIN)
    QSet<quint16> ports;
    const auto addPort = [&ports](int port) {
        if (port > 0 && port <= 65535) {
            ports.insert(static_cast<quint16>(port));
        }
    };
    addPort(config.localPort);
    addPort(config.localPort + 1);
    addPort(config.localPort + locationProbePortOffset);

    QSet<qint64> pids;
    for (DWORD pid : tcpListeningPidsForPorts(ports)) {
        pids.insert(static_cast<qint64>(pid));
    }

    return terminateCorePids(pids);
#else
    Q_UNUSED(config);
    Q_UNUSED(locationProbePortOffset);
    return {};
#endif
}
