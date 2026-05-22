#include "app/AppBootstrap.h"
#include "app/BackgroundTaskCoordinator.h"
#include "app/ConfigPathResolver.h"
#include "app/CoreStartupCheckpoint.h"
#include "app/CoreStartupChecklistController.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QGuiApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSessionManager>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <memory>
#include <optional>

#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <cwchar>
#include <vector>
#endif

#include <utility>

#include "app/StartupAdminElevation.h"
#include "app/AppBootstrapUiCommandPlan.h"
#include "app/SettingsDialogSession.h"
#include "app/SettingsDialogApplyPlan.h"
#include "app/SingleInstanceBootstrap.h"
#include "app/TunSettingsApplyDecision.h"
#include "app/TunRuntimeState.h"
#include "common/CorePidFile.h"
#include "common/DataSizeFormatter.h"
#include "common/ServerDisplayName.h"
#include "common/SystemProxyMode.h"
#include "common/UserAgent.h"
#include "domain/models/Config.h"
#include "platform/windows/WindowsAutoRunService.h"
#include "platform/windows/WindowsSystemProxyService.h"
#include "persistence/JsonConfigRepository.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/CoreConfigPreflight.h"
#include "runtime/CoreLifecycleService.h"
#include "runtime/ProtocolCoreCompat.h"
#include "runtime/QtCoreProcessHost.h"
#include "runtime/ServerConfigWriter.h"
#include "runtime/TunCompatCoreRequirement.h"
#include "services/ConfigBackupService.h"
#include "services/CoreUpdateService.h"
#include "services/GeoResourceUpdateService.h"
#include "services/ProxyAvailabilityCheckService.h"
#include "services/RoutingService.h"
#include "services/ServerService.h"
#include "services/SpeedTestService.h"
#include "services/SubscriptionService.h"
#include "services/SubscriptionUpdateService.h"
#include "subscription/ConfigFileImportParser.h"
#include "subscription/CustomConfigTextParser.h"
#include "subscription/SubscriptionImportTextParser.h"
#include "ui/dialogs/AddServerDialog.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/CustomServerDialog.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/theme/AppTheme.h"
#include "ui/tray/TrayController.h"

namespace {

constexpr auto DefaultIeProxyExceptions =
    "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;"
    "172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*";
constexpr auto ProjectPageUrl = "https://github.com/o3ku/songbird";
constexpr auto ReleasePageUrl = "https://github.com/o3ku/songbird/releases";
constexpr auto AppReleaseDate = __DATE__;
constexpr auto DocumentationUrl = "https://www.v2fly.org/";
constexpr auto DnsObjectUrl = "https://www.v2fly.org/config/dns.html#dnsobject";
constexpr auto RuleObjectUrl = "https://www.v2fly.org/config/routing.html#ruleobject";
constexpr int LocationProbePortOffset = 103;
constexpr int LocationProbeTimeoutMs = 5000;
constexpr int LocationProbeRetryDelayMs = 300;
constexpr int LocationProbeTotalTimeoutMs = 12000;
constexpr int LocationProbeMaxAttempts = 8;
constexpr int CoreListenProbeConnectTimeoutMs = 500;
constexpr int CoreListenProbeRetryDelayMs = 250;
constexpr int CoreListenProbeTotalTimeoutMs = 15000;
constexpr int TunCoreListenProbeTotalTimeoutMs = 45000;
constexpr int CoreStartupStableRunMs = 3000;
const QString kSingBoxRuleSetDirectoryName = QStringLiteral("rule-set");

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

bool isTunAdapterPresent()
{
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

QString normalizeCoreVersionTag(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("version "))) {
        value = value.mid(QStringLiteral("version ").size()).trimmed();
    }

    if (!value.isEmpty() && value.front().isDigit()) {
        value.prepend(QChar('v'));
    }

    return value;
}

QStringList missingSingBoxRuleSetTags(const QString& configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonArray ruleSets = document.object()
                                    .value(QStringLiteral("route"))
                                    .toObject()
                                    .value(QStringLiteral("rule_set"))
                                    .toArray();
    QStringList missingTags;
    QSet<QString> seenTags;
    const QString ruleSetDirectory = QDir(QCoreApplication::applicationDirPath()).filePath(kSingBoxRuleSetDirectoryName);
    for (const QJsonValue& value : ruleSets) {
        const QJsonObject ruleSet = value.toObject();
        if (ruleSet.value(QStringLiteral("type")).toString() != QStringLiteral("remote")) {
            continue;
        }

        const QString tag = ruleSet.value(QStringLiteral("tag")).toString().trimmed();
        if (tag.isEmpty() || seenTags.contains(tag)) {
            continue;
        }

        const QFileInfo localRuleSet(QDir(ruleSetDirectory).filePath(QStringLiteral("%1.srs").arg(tag)));
        if (localRuleSet.exists() && localRuleSet.isFile() && localRuleSet.size() > 0) {
            continue;
        }

        seenTags.insert(tag);
        missingTags.append(tag);
    }

    return missingTags;
}

QStringList coreVersionCommandArguments(CoreType coreType)
{
    switch (resolveRuntimeCoreType(coreType)) {
    case CoreType::SingBox:
        return QStringList{QStringLiteral("version")};
    case CoreType::Xray:
        return QStringList{QStringLiteral("-version")};
    case CoreType::Unknown:
    default:
        return {};
    }
}

QString extractCoreVersionFromOutput(CoreType coreType, const QString& output)
{
    if (output.trimmed().isEmpty()) {
        return {};
    }

    const QString normalizedOutput = output.trimmed();
    QRegularExpressionMatch match;

    switch (resolveRuntimeCoreType(coreType)) {
    case CoreType::SingBox:
        match = QRegularExpression(QStringLiteral("\\bsing-box\\s+version\\s+([0-9A-Za-z._-]+)"))
                    .match(normalizedOutput);
        break;
    case CoreType::Xray:
        match = QRegularExpression(QStringLiteral("\\bXray\\s+([0-9A-Za-z._-]+)"))
                    .match(normalizedOutput);
        break;
    case CoreType::Unknown:
    default:
        return {};
    }

    return match.hasMatch() ? normalizeCoreVersionTag(match.captured(1)) : QString();
}

bool isRoutineCoreLogLine(const QString& line)
{
    static const QRegularExpression importantLevelPattern(QStringLiteral(
        "(?:\\[(?:Warning|Error|Fatal|Panic)\\]|\\b(?:WARN|WARNING|ERROR|FATAL|PANIC)\\b)"));
    if (importantLevelPattern.match(line).hasMatch()) {
        return false;
    }

    static const QRegularExpression routineLevelPattern(QStringLiteral(
        "(?:\\[(?:Info|Debug)\\]|^\\s*(?:INFO|DEBUG)\\b|\\b(?:INFO|DEBUG)\\[)"));
    return routineLevelPattern.match(line).hasMatch();
}

QString countryCodeToFlag(const QString& countryCode)
{
    const QString code = countryCode.trimmed().toUpper();
    if (code.size() != 2) {
        return {};
    }

    QVector<uint> flagCodePoints;
    flagCodePoints.reserve(2);
    for (const QChar ch : code) {
        if (ch < QLatin1Char('A') || ch > QLatin1Char('Z')) {
            return {};
        }
        flagCodePoints.append(0x1F1E6u + static_cast<uint>(ch.unicode() - QLatin1Char('A').unicode()));
    }
    return QString::fromUcs4(flagCodePoints.constData(), flagCodePoints.size());
}

QString buildLocationSummaryFromPayload(const QByteArray& payload)
{
    const QJsonDocument json = QJsonDocument::fromJson(payload);
    if (!json.isObject()) {
        return {};
    }

    const QJsonObject object = json.object();
    const QString status = object.value(QStringLiteral("status")).toString().trimmed();
    if (status.compare(QStringLiteral("fail"), Qt::CaseInsensitive) == 0) {
        return {};
    }
    if (object.contains(QStringLiteral("success")) && !object.value(QStringLiteral("success")).toBool(true)) {
        return {};
    }

    QString city = object.value(QStringLiteral("city")).toString().trimmed();
    if (city.isEmpty()) {
        city = object.value(QStringLiteral("regionName")).toString().trimmed();
    }
    if (city.isEmpty()) {
        city = object.value(QStringLiteral("region_name")).toString().trimmed();
    }
    if (city.isEmpty()) {
        city = object.value(QStringLiteral("region")).toString().trimmed();
    }

    QString country = object.value(QStringLiteral("country")).toString().trimmed();
    if (country.isEmpty()) {
        country = object.value(QStringLiteral("country_name")).toString().trimmed();
    }
    QString countryCode = object.value(QStringLiteral("countryCode")).toString().trimmed();
    if (countryCode.isEmpty()) {
        countryCode = object.value(QStringLiteral("country_code")).toString().trimmed();
    }
    if (countryCode.isEmpty()) {
        countryCode = object.value(QStringLiteral("country_iso")).toString().trimmed();
    }
    if (countryCode.isEmpty() && country.size() == 2) {
        countryCode = country;
    }

    QStringList parts;
    if (!country.isEmpty()) {
        const QString flag = countryCodeToFlag(countryCode);
        parts.append(flag.isEmpty() ? country : QStringLiteral("%1 %2").arg(flag, country));
    }
    if (!city.isEmpty()) {
        parts.append(city);
    }

    const QString summary = parts.join(QStringLiteral(", "));
    if (!summary.isEmpty()) {
        return summary;
    }

    QString ip = object.value(QStringLiteral("query")).toString().trimmed();
    if (ip.isEmpty()) {
        ip = object.value(QStringLiteral("ip")).toString().trimmed();
    }
    return ip;
}

QStringList locationProbeUrls()
{
    return {
        QStringLiteral("http://ip-api.com/json/?fields=status,message,country,countryCode,regionName,city,query"),
        QStringLiteral("http://ipwho.is/"),
        QStringLiteral("https://ipinfo.io/json"),
        QStringLiteral("https://ifconfig.co/json"),
        QStringLiteral("https://ipapi.co/json/")
    };
}

bool isValidTcpPort(int port)
{
    return port > 0 && port <= 65535;
}

int resolveLocationProbeHttpPort(const Config& config, bool usesDedicatedProbe)
{
    if (usesDedicatedProbe) {
        return isValidTcpPort(config.localLocationProbePort)
            ? config.localLocationProbePort
            : config.localPort + LocationProbePortOffset;
    }

    return isValidTcpPort(config.localHttpPort)
        ? config.localHttpPort
        : config.localPort + 1;
}

bool waitForLocalTcpPortReady(int port, int timeoutMs)
{
    if (!isValidTcpPort(port) || timeoutMs <= 0) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
        if (socket.waitForConnected(CoreListenProbeConnectTimeoutMs)) {
            socket.disconnectFromHost();
            return true;
        }

        if (timer.elapsed() < timeoutMs) {
            QThread::msleep(CoreListenProbeRetryDelayMs);
        }
    }

    return false;
}

QString locationProbeErrorMessage(const QUrl& url, const QString& error)
{
    const QString host = url.host().trimmed();
    if (host.isEmpty()) {
        return error;
    }
    if (error.trimmed().isEmpty()) {
        return QStringLiteral("Outbound location request failed: %1").arg(host);
    }
    return QStringLiteral("%1: %2").arg(host, error.trimmed());
}

QString readCoreVersion(CoreType coreType, const QString& program)
{
    const QStringList arguments = coreVersionCommandArguments(coreType);
    if (program.trimmed().isEmpty() || arguments.isEmpty()) {
        return {};
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(QFileInfo(program).absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted(1500)) {
        return {};
    }

    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1500);
        return {};
    }

    return extractCoreVersionFromOutput(coreType, QString::fromUtf8(process.readAll()));
}

QStringList buildCoreCandidateDirectories(const QString& configPath)
{
    QStringList directories;
    const auto appendDirectory = [&directories](const QString& path) {
        if (path.trimmed().isEmpty()) {
            return;
        }

        const QString directory = QDir(path).absolutePath();
        if (!directory.isEmpty() && !directories.contains(directory, Qt::CaseInsensitive)) {
            directories.append(directory);
        }
    };

    appendDirectory(QDir::currentPath());
    appendDirectory(QCoreApplication::applicationDirPath());
    appendDirectory(QFileInfo(configPath).dir().absolutePath());
    return directories;
}

QStringList coreExecutableCandidateNames(CoreType coreType)
{
    switch (resolveRuntimeCoreType(coreType)) {
    case CoreType::SingBox:
        return QStringList{
            QStringLiteral("sing-box-client.exe"),
            QStringLiteral("sing-box.exe")
        };
    case CoreType::Xray:
    case CoreType::Unknown:
    default:
        return QStringList{
            QStringLiteral("xray.exe")};
    }
}

QStringList resolveCoreCandidatesForConfigPath(CoreType coreType, const QString& configPath)
{
    const QStringList directories = buildCoreCandidateDirectories(configPath);
    const QStringList fileNames = coreExecutableCandidateNames(coreType);

    QStringList candidates;
    for (const QString& directory : directories) {
        for (const QString& fileName : fileNames) {
            candidates.append(QDir(directory).filePath(fileName));
        }
    }
    return candidates;
}

QString locateFirstExistingFileInCandidates(const QStringList& candidates)
{
    for (const QString& candidate : candidates) {
        const QFileInfo candidateInfo(candidate);
        const QString fileName = candidateInfo.fileName();
        if (fileName.contains(QChar('*')) || fileName.contains(QChar('?'))) {
            QDir candidateDir(candidateInfo.path());
            if (!candidateDir.exists()) {
                continue;
            }

            const QFileInfoList matches = candidateDir.entryInfoList(
                QStringList{fileName},
                QDir::Files | QDir::NoSymLinks | QDir::Readable,
                QDir::Name | QDir::IgnoreCase);
            if (!matches.isEmpty()) {
                return QDir::toNativeSeparators(matches.constFirst().absoluteFilePath());
            }
            continue;
        }

        if (QFileInfo::exists(candidate)) {
            return QDir::toNativeSeparators(QFileInfo(candidate).absoluteFilePath());
        }
    }

    return {};
}

QStringList coreCandidateFileNames(const QStringList& candidates)
{
    QStringList candidateNames;
    for (const QString& candidate : candidates) {
        const QString fileName = QFileInfo(candidate).fileName();
        if (!fileName.isEmpty() && !candidateNames.contains(fileName, Qt::CaseInsensitive)) {
            candidateNames.append(fileName);
        }
    }
    return candidateNames;
}

QString expectedCoreFilesText(const QStringList& candidates)
{
    const QStringList candidateNames = coreCandidateFileNames(candidates);
    return candidateNames.isEmpty()
        ? QCoreApplication::translate("AppBootstrap", "(unknown)")
        : candidateNames.join(QStringLiteral(", "));
}

QStringList splitProxyExceptions(const QString& value)
{
    QString normalized = value;
    normalized.replace(QChar(','), QChar(';'));

    QStringList parts;
    for (const QString& item : normalized.split(QChar(';'), Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }
    return parts;
}

void appendUniqueProxyException(QStringList& entries, const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    for (const QString& existing : std::as_const(entries)) {
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    entries.append(trimmed);
}

QStringList collectRouteDerivedProxyExceptions(const Config& config)
{
    QList<RoutingRule> effectiveRules;
    for (const RoutingRule& rule : config.collection().routingCustomRules) {
        if (rule.enabled) {
            effectiveRules.append(rule);
        }
    }

    if (config.collection().enableRoutingAdvanced
        && config.collection().routingIndex >= 0
        && config.collection().routingIndex < config.collection().routingItems.size()) {
        const RoutingItem& selectedRouting = config.collection().routingItems.at(config.collection().routingIndex);
        for (const RoutingRule& rule : selectedRouting.rules) {
            if (rule.enabled) {
                effectiveRules.append(rule);
            }
        }
    }

    QStringList derived;
    for (const RoutingRule& rule : std::as_const(effectiveRules)) {
        if (rule.outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        for (const QString& domainValue : rule.domain) {
            const QString domain = domainValue.trimmed();
            if (domain.startsWith(QStringLiteral("domain:"), Qt::CaseInsensitive)) {
                const QString suffix = domain.mid(QStringLiteral("domain:").size()).trimmed();
                if (suffix.isEmpty()) {
                    continue;
                }
                appendUniqueProxyException(derived, suffix);
                appendUniqueProxyException(derived, QStringLiteral("*.%1").arg(suffix));
                continue;
            }

            if (domain.startsWith(QStringLiteral("full:"), Qt::CaseInsensitive)) {
                appendUniqueProxyException(derived, domain.mid(QStringLiteral("full:").size()).trimmed());
            }
        }
    }

    return derived;
}
constexpr auto XrayReleasePageUrl = "https://github.com/XTLS/Xray-core/releases";
constexpr auto SingBoxReleasePageUrl = "https://github.com/SagerNet/sing-box/releases";
constexpr auto GeoReleasePageUrl = "https://github.com/Loyalsoldier/v2ray-rules-dat/releases";

bool interactivePromptsEnabled()
{
    return !qEnvironmentVariableIsSet("SONGBIRD_NONINTERACTIVE");
}

QString tunAdminRequiredStartMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows. Run SongBird as administrator before starting the core.");
}

QString tunAdminRequiredSaveMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows. The settings were saved and will take effect after restarting SongBird as administrator.");
}

QString tunAdminRestartPromptTitle()
{
    return QCoreApplication::translate("AppBootstrap", "Administrator Permission");
}

QString tunAdminRestartPromptMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows.\nRestart SongBird as administrator now to apply the saved TUN setting?");
}

QString tunAdminRestartFailureMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "Failed to restart SongBird with administrator privileges.");
}

QString sanitizeFileNameSegment(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return QStringLiteral("server");
    }

    for (int index = 0; index < value.size(); ++index) {
        switch (value.at(index).unicode()) {
        case '<':
        case '>':
        case ':':
        case '"':
        case '/':
        case '\\':
        case '|':
        case '?':
        case '*':
            value[index] = QChar('_');
            break;
        default:
            break;
        }
    }

    return value;
}

QString buildSuggestedExportPath(const QString& directoryPath, const VmessItem& server, const QString& suffix)
{
    const QString baseName = sanitizeFileNameSegment(
        !server.remarks.trimmed().isEmpty()
            ? server.remarks
            : (!server.indexId.trimmed().isEmpty() ? server.indexId : QStringLiteral("server")));
    return QDir(directoryPath).filePath(QStringLiteral("%1-%2.json").arg(baseName, suffix));
}

OperationResult importCustomConfigTextWithService(
    const QString& text,
    Config& config,
    ServerService& serverService)
{
    QString extension;
    bool ok = false;
    VmessItem item = CustomConfigTextParser::parse(text, &extension, &ok);
    if (!ok) {
        return OperationResult::fail(QString());
    }

    QString tempDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDirectory.trimmed().isEmpty()) {
        tempDirectory = QDir::tempPath();
    }
    if (!QDir().mkpath(tempDirectory)) {
        return OperationResult::fail(QStringLiteral("Failed to create temporary directory for custom config import."));
    }

    const QString fileName = extension.trimmed().isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : QStringLiteral("%1.%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), extension.trimmed());
    const QString tempFilePath = QDir(tempDirectory).filePath(fileName);

    QFile file(tempFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to create temporary custom config file."));
    }
    if (file.write(text.toUtf8()) < 0) {
        file.close();
        QFile::remove(tempFilePath);
        return OperationResult::fail(QStringLiteral("Failed to write temporary custom config file."));
    }
    file.close();

    item.address = tempFilePath;
    const OperationResult result = serverService.addServer(config, item);
    QFile::remove(tempFilePath);
    return result;
}

template <typename Callback>
void invokeOnUiThread(QObject* context, Callback&& callback)
{
    if (context == nullptr) {
        return;
    }

    if (QThread::currentThread() == context->thread()) {
        callback();
        return;
    }

    QMetaObject::invokeMethod(context, std::forward<Callback>(callback), Qt::QueuedConnection);
}

template <typename Callback>
auto invokeOnUiThreadBlocking(QObject* context, Callback&& callback) -> decltype(callback())
{
    using Result = decltype(callback());
    if (context == nullptr) {
        return Result();
    }

    if (QThread::currentThread() == context->thread()) {
        return callback();
    }

    Result result{};
    QMetaObject::invokeMethod(
        context,
        [&]() {
            result = callback();
        },
        Qt::BlockingQueuedConnection);
    return result;
}

} // namespace

AppBootstrap::AppBootstrap(
    QString configPath,
    bool startHidden,
    bool skipCoreChecks)
    : configPath_(std::move(configPath))
    , startHidden_(startHidden)
    , skipCoreChecks_(skipCoreChecks)
{
}

AppBootstrap::~AppBootstrap()
{
    shuttingDown_.store(true);
    lifetimeGuard_.reset();
    cancelPendingCoreRestarts();
    waitForBackgroundThreads();

    if (coreLifecycleService_) {
        coreLifecycleService_->stop();
    }
    if (auxiliaryCoreLifecycleService_) {
        auxiliaryCoreLifecycleService_->stop();
    }
}

bool AppBootstrap::run()
{
    repository_ = std::make_unique<JsonConfigRepository>(resolveConfigPath());
    serverService_ = std::make_unique<ServerService>(*repository_, resolveCustomConfigDirectory());
    configBackupService_ = std::make_unique<ConfigBackupService>(resolveConfigPath());
    routingService_ = std::make_unique<RoutingService>(*repository_);
    speedTestService_ = std::make_unique<SpeedTestService>(resolveCustomConfigDirectory());
    subscriptionService_ = std::make_unique<SubscriptionService>(*repository_);
    geoResourceUpdateService_ = std::make_unique<GeoResourceUpdateService>(
        QFileInfo(resolveConfigPath()).dir().absolutePath());
    clientConfigWriter_ = std::make_unique<ClientConfigWriter>(resolveCustomConfigDirectory());
    serverConfigWriter_ = std::make_unique<ServerConfigWriter>();
    coreProcessHost_ = std::make_unique<QtCoreProcessHost>();
    coreLifecycleService_ = std::make_unique<CoreLifecycleService>(*coreProcessHost_);
    coreStartupChecklist_ = std::make_unique<CoreStartupChecklistController>();
    backgroundTasks_ = std::make_unique<BackgroundTaskCoordinator>();
    backgroundTasks_->setBlockingPredicate([this]() {
        return isCoreStartupBlockingBackgroundTask();
    });
    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::blockedByCoreStartup,
        backgroundTasks_.get(), [this]() {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Proxy startup is in progress.")));
        });
    auxiliaryCoreProcessHost_ = std::make_unique<QtCoreProcessHost>();
    auxiliaryCoreLifecycleService_ = std::make_unique<CoreLifecycleService>(*auxiliaryCoreProcessHost_);
    autoRunService_ = std::make_unique<WindowsAutoRunService>();
    systemProxyService_ = std::make_unique<WindowsSystemProxyService>();

    mainWindow_ = std::make_unique<MainWindow>();
    QObject::connect(coreLifecycleService_.get(), &CoreLifecycleService::outputReceived, mainWindow_.get(), [this](const QString& line) {
        if (isRoutineCoreLogLine(line)) {
            return;
        }
        mainWindow_->appendLog(line);
    });
    QObject::connect(auxiliaryCoreLifecycleService_.get(), &CoreLifecycleService::outputReceived, mainWindow_.get(), [this](const QString& line) {
        if (isRoutineCoreLogLine(line)) {
            return;
        }
        mainWindow_->appendLog(QStringLiteral("tun-compat | %1").arg(line));
    });
    QObject::connect(coreLifecycleService_.get(), &CoreLifecycleService::startFailed, mainWindow_.get(), [this](const QString& message) {
        handleCoreStartFailed(message);
    });
    QObject::connect(coreLifecycleService_.get(), &CoreLifecycleService::exited, mainWindow_.get(),
        [this](int exitCode, QProcess::ExitStatus status, bool stopRequested) {
            handleCoreExited(exitCode, static_cast<int>(status), stopRequested, false);
        });
    QObject::connect(auxiliaryCoreLifecycleService_.get(), &CoreLifecycleService::exited, mainWindow_.get(),
        [this](int exitCode, QProcess::ExitStatus status, bool stopRequested) {
            handleCoreExited(exitCode, static_cast<int>(status), stopRequested, true);
        });
    QObject::connect(speedTestService_.get(), &SpeedTestService::runningChanged, mainWindow_.get(), [this](bool running) {
        QPointer<MainWindow> mainWindowGuard(mainWindow_.get());
        if (!mainWindowGuard.isNull()) {
            mainWindowGuard->setSpeedTestRunning(running);
        }
        if (!running) {
            backgroundTasks_->resetSpeedTestProgress();
            if (speedTestResultsDirty_ && serverService_ != nullptr && !serverService_->save(config_)) {
                appendResult(OperationResult::fail(QStringLiteral("Failed to save configuration after updating test results.")));
            }
            speedTestResultsDirty_ = false;
            backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::SpeedTest);
        } else {
            speedTestResultsDirty_ = false;
            backgroundTasks_->syncState();
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::logGenerated, mainWindow_.get(), [this](const QString& message) {
        QPointer<MainWindow> mainWindowGuard(mainWindow_.get());
        if (!mainWindowGuard.isNull()) {
            mainWindowGuard->appendLog(message);
            mainWindowGuard->showTransientStatus(
                message,
                3000,
                MainWindow::TransientStatusPriority::Routine);
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::testResultReady, mainWindow_.get(), [this](const QString& indexId, const QString& result) {
        if (serverService_ == nullptr) {
            return;
        }

        const OperationResult updateResult = serverService_->setTestResult(config_, indexId, result);
        if (!updateResult.success) {
            appendResult(updateResult);
            return;
        }
        speedTestResultsDirty_ = true;
        const VmessItem* speedTestServer = findServerById(indexId);
        const QString serverName = speedTestServer == nullptr ? QString() : serverDisplayName(*speedTestServer);
        backgroundTasks_->recordSpeedTestResult(serverName);
        backgroundTasks_->syncState();

        if (mainWindow_ != nullptr) {
            mainWindow_->updateServerTestResult(indexId, result);
        }
        if (trayController_ != nullptr) {
            trayController_->setServers(
                config_.collection().servers,
                config_.currentIndexId);
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::finished, mainWindow_.get(), [this](const QString& summary) {
        if (mainWindow_ != nullptr) {
            mainWindow_->showTransientStatus(summary, 4000);
        }
    });

    trayController_ = std::make_unique<TrayController>(mainWindow_.get());

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, mainWindow_.get(), [this]() {
        shuttingDown_.store(true);
        cancelPendingCoreRestarts();
        cleanupRuntimeForExit(windowsShutdownRequested_);
        if (!shutdownUiStatePersisted_) {
            persistUiState();
            shutdownUiStatePersisted_ = true;
        }
    });
    if (QGuiApplication* guiApplication = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        QObject::connect(guiApplication, &QGuiApplication::commitDataRequest, mainWindow_.get(), [this](QSessionManager&) {
            windowsShutdownRequested_ = true;
            shuttingDown_.store(true);
            cancelPendingCoreRestarts();
            cleanupRuntimeForExit(true);
            if (!shutdownUiStatePersisted_) {
                persistUiState();
                shutdownUiStatePersisted_ = true;
            }
        });
    }

    cleanupOrphanCoreProcesses();
    wireMainWindow();
    mainWindow_->setHideToTrayEnabled(trayController_->initialize());
    refreshExistingCoreTypes();
    if (!reloadConfig()) {
        return false;
    }
    autoBackupCurrentConfig();
    if (systemProxyService_ != nullptr) {
        managedSystemProxyActive_ = shouldAdoptManagedSystemProxyOnStartup(
            normalizeSystemProxyMode(config_.sysProxyType),
            config_.ui().mainProxyEnabled,
            systemProxyService_->isEnabled());
    }
    mainWindow_->show();
    if (startHidden_ || !config_.ui().showMainOnStartup) {
        if (trayController_ != nullptr && trayController_->isAvailable()) {
            mainWindow_->hide();
        } else {
            mainWindow_->showMinimized();
            appendResult(OperationResult::ok(QStringLiteral(
                "Start hidden requested, but the system tray is unavailable. The window was minimized instead.")));
        }
    }
    uiReady_ = true;
    QTimer::singleShot(0, mainWindow_.get(), [this]() {
        startCoreOnStartup();
    });

    return true;
}

void AppBootstrap::wireMainWindow()
{
    QObject::connect(coreStartupChecklist_.get(), &CoreStartupChecklistController::itemsChanged,
                     mainWindow_.get(), &MainWindow::setCoreStartupChecklist);
    QObject::connect(coreStartupChecklist_.get(), &CoreStartupChecklistController::cleared,
                     mainWindow_.get(), &MainWindow::clearCoreStartupChecklist);

    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::runningChanged,
                     mainWindow_.get(), &MainWindow::setBackgroundTaskRunning);
    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::descriptionChanged,
                     mainWindow_.get(), &MainWindow::setBackgroundTaskDescription);
    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::runningChanged,
                     trayController_.get(), &TrayController::setBackgroundTaskRunning);
    QObject::connect(backgroundTasks_.get(), &BackgroundTaskCoordinator::descriptionChanged,
                     trayController_.get(), &TrayController::setBackgroundTaskDescription);

    QObject::connect(mainWindow_.get(), &MainWindow::addServerRequested, mainWindow_.get(), [this]() {
        AddServerDialog dialog(mainWindow_.get());
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        appendResult(serverService_->addServer(config_, dialog.server()));
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::addCustomServerRequested, mainWindow_.get(), [this]() {
        CustomServerDialog dialog(mainWindow_.get());
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        appendResult(serverService_->addServer(config_, dialog.server()));
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::editServerRequested, mainWindow_.get(), [this](const QString& indexId) {
        const std::optional<VmessItem> existing = findServerSnapshotById(indexId);
        QPointer<MainWindow> mainWindowGuard(mainWindow_.get());
        if (!existing.has_value()) {
            if (!mainWindowGuard.isNull()) {
                mainWindowGuard->appendLog(QStringLiteral("The selected server could not be found for editing."));
            }
            return;
        }

        const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
        const QString activeServerId = activeServer.has_value() ? activeServer->indexId : QString();
        const bool shouldRestartCore = isCoreRunning() && activeServerId == indexId;
        OperationResult result;

        if (existing->configType == ConfigType::Custom) {
            CustomServerDialog dialog(mainWindowGuard.data());
            dialog.setWindowTitle(QStringLiteral("Edit Custom Server"));
            dialog.setServer(*existing, serverService_->resolveCustomConfigPath(existing->address));
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            result = serverService_->updateServer(config_, indexId, dialog.server());
        } else {
            AddServerDialog dialog(mainWindowGuard.data());
            dialog.setWindowTitle(QStringLiteral("Edit Server"));
            dialog.setServer(*existing);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            result = serverService_->updateServer(config_, indexId, dialog.server());
        }
        appendResult(result);
        syncWindow();
        if (result.success && shouldRestartCore) {
            restartCoreIfRunning(QStringLiteral("Reloading core after editing the active server."), true);
        }
    });

    QObject::connect(mainWindow_.get(), &MainWindow::duplicateServerRequested, mainWindow_.get(), [this](const QString& indexId) {
        appendResult(serverService_->duplicateServer(config_, indexId));
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::exportClientConfigRequested, mainWindow_.get(), [this](const QString& indexId) {
        exportClientConfig(indexId);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::exportServerConfigRequested, mainWindow_.get(), [this](const QString& indexId) {
        exportServerConfig(indexId);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::importFromClipboardRequested, mainWindow_.get(), [this]() {
        importFromClipboard();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::importClientConfigRequested, mainWindow_.get(), [this]() {
        importClientConfigFromFile();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::importServerConfigRequested, mainWindow_.get(), [this]() {
        importServerConfigFromFile();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::updateSubscriptionsRequested, mainWindow_.get(), [this]() {
        updateAllSubscriptions();
    });
    QObject::connect(
        mainWindow_.get(),
        &MainWindow::updateCurrentSubscriptionRequested,
        mainWindow_.get(),
        [this](const QString& subscriptionId) {
            updateCurrentSubscription(subscriptionId);
        });
    QObject::connect(
        mainWindow_.get(),
        &MainWindow::updateCurrentSubscriptionViaProxyRequested,
        mainWindow_.get(),
        [this](const QString& subscriptionId) {
            updateCurrentSubscriptionViaProxy(subscriptionId);
        });
    QObject::connect(mainWindow_.get(), &MainWindow::hideSubscriptionRequested, mainWindow_.get(), [this](const QString& subscriptionId) {
        hideSubscription(subscriptionId);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::deleteSubscriptionRequested, mainWindow_.get(), [this](const QString& subscriptionId) {
        deleteSubscription(subscriptionId);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::testMeRequested, mainWindow_.get(), [this]() {
        runProxyAvailabilityCheck();
    });
    QObject::connect(mainWindow_.get(), &MainWindow::updateCoreRequested, mainWindow_.get(), [this](int coreTypeValue) {
        updateCore(coreTypeValue);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::updateGeoResourcesRequested, mainWindow_.get(), [this]() {
        updateGeoResources();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openCustomConfigRequested, mainWindow_.get(), [this](const QString& indexId) {
        openCustomConfigFile(indexId);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::enableSystemProxyRequested, mainWindow_.get(), [this]() {
        enableSystemProxy(true);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::disableSystemProxyRequested, mainWindow_.get(), [this]() {
        disableSystemProxy();
    });
    QObject::connect(mainWindow_.get(), &MainWindow::tunEnabledChanged, mainWindow_.get(), [this](bool enabled) {
        setTunEnabled(enabled);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::routingModeSelected, mainWindow_.get(), [this](int mode) {
        const bool previousAdvancedEnabled = config_.collection().enableRoutingAdvanced;
        const int previousRoutingIndex = config_.collection().routingIndex;
        const OperationResult result = routingService_->setRoutingMode(config_, mode >= 0, mode);
        handleRoutingSelectionResult(result, previousAdvancedEnabled, previousRoutingIndex);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::toggleAutoRunRequested, mainWindow_.get(), [this]() {
        toggleAutoRun();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::restoreBackupRequested, mainWindow_.get(), [this]() {
        restoreConfigFromBackup();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::settingsRequested, mainWindow_.get(), [this]() {
        openSettingsDialog();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openSettingsAtSubscriptionsTabRequested, mainWindow_.get(), [this]() {
        openSettingsDialog(1);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openSettingsAtRoutingTabRequested, mainWindow_.get(), [this]() {
        openSettingsDialog(2);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::aboutRequested, mainWindow_.get(), [this]() {
        openAboutDialog();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openProjectPageRequested, mainWindow_.get(), [this]() {
        openExternalUrl(QString::fromUtf8(ProjectPageUrl));
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openReleasePageRequested, mainWindow_.get(), [this]() {
        openExternalUrl(QString::fromUtf8(ReleasePageUrl));
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openDocumentationRequested, mainWindow_.get(), [this]() {
        openExternalUrl(QString::fromUtf8(DocumentationUrl));
    });

    QObject::connect(
        mainWindow_.get(),
        &MainWindow::openDnsObjectDocumentationRequested,
        mainWindow_.get(),
        [this]() {
            openExternalUrl(QString::fromUtf8(DnsObjectUrl));
        });

    QObject::connect(
        mainWindow_.get(),
        &MainWindow::openRuleObjectDocumentationRequested,
        mainWindow_.get(),
        [this]() {
            openExternalUrl(QString::fromUtf8(RuleObjectUrl));
        });

    QObject::connect(mainWindow_.get(), &MainWindow::openLoopbackToolRequested, mainWindow_.get(), [this]() {
        openLoopbackTool();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openXrayReleasePageRequested, mainWindow_.get(), [this]() {
        openExternalUrl(QString::fromUtf8(XrayReleasePageUrl));
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openSingBoxReleasePageRequested, mainWindow_.get(), [this]() {
        openExternalUrl(QString::fromUtf8(SingBoxReleasePageUrl));
    });

    QObject::connect(mainWindow_.get(), &MainWindow::openGeoReleasePageRequested, mainWindow_.get(), [this]() {
        openExternalUrl(QString::fromUtf8(GeoReleasePageUrl));
    });

    QObject::connect(mainWindow_.get(), &MainWindow::removeServersRequested, mainWindow_.get(), [this](const QStringList& indexIds) {
        const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
        const QString activeServerId = activeServer.has_value() ? activeServer->indexId : QString();
        const bool activeServerRemoved = !activeServerId.isEmpty() && indexIds.contains(activeServerId);
        const OperationResult result = serverService_->removeServers(config_, indexIds);
        appendResult(result);
        syncWindow();
        if (!result.success || !activeServerRemoved || !isCoreRunning()) {
            return;
        }

        if (!resolveActiveServerSnapshot().has_value()) {
            appendResult(OperationResult::ok(QStringLiteral("Stopping core because the active server was removed.")));
            stopCore(false);
            return;
        }

        restartCoreIfRunning(QStringLiteral("Reloading core after removing the active server."), true);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::moveServersRequested, mainWindow_.get(), [this](const QStringList& indexIds, int operation) {
        appendResult(serverService_->moveServers(config_, indexIds, static_cast<ServerMoveOperation>(operation)));
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::reorderServersRequested, mainWindow_.get(), [this](const QStringList& orderedIndexIds) {
        appendResult(serverService_->reorderServers(config_, orderedIndexIds));
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::setDefaultServerRequested, mainWindow_.get(), [this](const QString& indexId) {
        if (requestDefaultServerSwitchAfterCoreStop(indexId, false)) {
            return;
        }

        const QString previousIndexId = config_.currentIndexId;
        const OperationResult result = serverService_->setDefaultServer(config_, indexId);
        handleDefaultServerSelectionResult(result, previousIndexId);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::setDefaultServerWithTunRequested, mainWindow_.get(), [this](const QString& indexId) {
        setDefaultServerWithTun(indexId);
    });
    QObject::connect(mainWindow_.get(), &MainWindow::testServersRequested, mainWindow_.get(), [this](const QStringList& indexIds) {
        startSpeedTest(indexIds);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::reloadConfigRequested, mainWindow_.get(), [this]() {
        reloadConfig();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::hiddenToTray, mainWindow_.get(), [this]() {
        mainWindow_->appendLog(QStringLiteral("Main window hidden to tray."));
    });

    QObject::connect(trayController_.get(), &TrayController::defaultServerRequested, mainWindow_.get(), [this](const QString& indexId) {
        if (requestDefaultServerSwitchAfterCoreStop(indexId, false)) {
            return;
        }

        const QString previousIndexId = config_.currentIndexId;
        const OperationResult result = serverService_->setDefaultServer(config_, indexId);
        handleDefaultServerSelectionResult(result, previousIndexId);
    });

    QObject::connect(trayController_.get(), &TrayController::routingRequested, mainWindow_.get(), [this](int index) {
        const int previousRoutingIndex = config_.collection().routingIndex;
        const bool previousAdvancedEnabled = config_.collection().enableRoutingAdvanced;
        const OperationResult result = routingService_->selectRouting(config_, index);
        handleRoutingSelectionResult(result, previousAdvancedEnabled, previousRoutingIndex);
    });

    QObject::connect(trayController_.get(), &TrayController::quitRequested, mainWindow_.get(), [this]() {
        mainWindow_->requestExit();
    });

}

void AppBootstrap::syncWindow()
{
    if (qApp != nullptr) {
        AppTheme::applyApplicationTheme(*qApp, config_.ui().themeName);
    }

    if (mainWindow_) {
        mainWindow_->setConfig(config_);
    }

    if (trayController_ != nullptr) {
        trayController_->setServers(
            config_.collection().servers,
            config_.currentIndexId);
        trayController_->setRoutings(
            config_.collection().routingItems,
            config_.collection().routingIndex,
            config_.collection().enableRoutingAdvanced);
    }

    syncStatusIndicators();
}

bool AppBootstrap::isCoreStartupBlockingBackgroundTask() const
{
    return coreStartPending_
        || resumeCoreStartAfterTunCleanup_
        || tunCleanupActive_;
}

void AppBootstrap::syncStatusIndicators()
{
    systemProxyMode_ = normalizeSystemProxyMode(config_.sysProxyType);
    const bool systemProxyEnabled = systemProxyService_ != nullptr && systemProxyService_->isEnabled();
    const bool autoRunEnabled = autoRunService_ != nullptr && autoRunService_->isEnabled();
    const bool coreRestartPending = coreRestartTimer_ != nullptr && coreRestartTimer_->isActive();
    const bool corePending = coreStartPending_ || coreStopPending_ || coreRestartPending;
    const bool coreProcessRunning = isCoreRunning();
    const bool coreRunning = isCoreReady();
    const VmessItem* currentServer = findServerById(config_.currentIndexId);
    const QString currentServerName = currentServer == nullptr ? QString() : serverDisplayName(*currentServer);
    const QString routingSummary = buildRoutingSummaryText();
    const QString listenSummary = buildListenSummaryText();

    if (mainWindow_ != nullptr) {
        mainWindow_->setExistingCoreTypes(existingCoreTypes_);
        mainWindow_->setCurrentServerName(currentServerName);
        mainWindow_->setCurrentServerLocation(currentServerLocation_);
        mainWindow_->setCoreProcessRunning(coreProcessRunning);
        mainWindow_->setRoutingSummary(routingSummary, listenSummary);
        mainWindow_->setCoreRunning(coreRunning, corePending);
        mainWindow_->setSystemProxyState(toLegacySystemProxyModeValue(systemProxyMode_), systemProxyEnabled);
        mainWindow_->setAutoRunEnabled(autoRunEnabled);
    }

    if (trayController_ != nullptr) {
        trayController_->setCurrentServerName(currentServerName);
        trayController_->setCoreProcessRunning(coreProcessRunning);
        trayController_->setCoreRunning(coreRunning, corePending);
        trayController_->setSystemProxyState(toLegacySystemProxyModeValue(systemProxyMode_), systemProxyEnabled);
        trayController_->setAutoRunEnabled(autoRunEnabled);
        trayController_->setRoutingSummary(routingSummary, config_.collection().enableRoutingAdvanced);
    }

    if (backgroundTasks_ != nullptr) {
        backgroundTasks_->syncState();
    }
}

void AppBootstrap::clearCurrentServerLocation()
{
    currentServerLocation_.clear();
    if (mainWindow_ != nullptr) {
        mainWindow_->setCurrentServerLocation({});
    }
}

void AppBootstrap::queryCurrentServerLocation(
    const QString& serverIndexId,
    CoreType runtimeCore,
    std::optional<bool> startupProxyEnabled)
{
    Q_UNUSED(runtimeCore);

    const QString locationStep = QCoreApplication::translate("AppBootstrap", "Check outbound location");
    if (!coreReady_ || serverIndexId.trimmed().isEmpty()) {
        const QString message = QStringLiteral("Core is not ready for outbound location detection.");
        coreStartupChecklist_->setCheckpointStatus(CoreStartupCheckpointStatus::Failed, locationStep, message);
        failCoreStartup(OperationResult::fail(message));
        return;
    }

    clearCurrentServerLocation();
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Started,
        locationStep,
        QStringLiteral("Detecting outbound IP location."));

    const VmessItem* activeServer = findServerById(serverIndexId);
    const bool usesDedicatedProbe = activeServer != nullptr
        && activeServer->configType != ConfigType::Custom;

    const int httpPort = resolveLocationProbeHttpPort(config_, usesDedicatedProbe);
    if (!isValidTcpPort(httpPort)) {
        const QString message = QStringLiteral("Location probe port is unavailable.");
        coreStartupChecklist_->setCheckpointStatus(CoreStartupCheckpointStatus::Failed, locationStep, message);
        failCoreStartup(OperationResult::fail(message));
        return;
    }

    QPointer<QObject> uiContext(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    const QString activeServerId = serverIndexId.trimmed();
    QThread* thread = QThread::create([this,
                                       uiContext,
                                       lifetimeGuard,
                                       activeServerId,
                                       httpPort,
                                       locationStep,
                                       startupProxyEnabled]() {
        QString location;
        QString lastError;
        QElapsedTimer probeTimer;
        probeTimer.start();
        const QStringList probeUrls = locationProbeUrls();
        int attempt = 0;

        while (location.isEmpty()
            && probeTimer.elapsed() < LocationProbeTotalTimeoutMs
            && attempt < LocationProbeMaxAttempts) {
            const QUrl probeUrl(probeUrls.at(attempt % probeUrls.size()));
            ++attempt;
            QNetworkAccessManager manager;
            manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), httpPort));

            QNetworkRequest request(probeUrl);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            request.setHeader(QNetworkRequest::UserAgentHeader, fallbackUserAgent());

            QEventLoop loop;
            QNetworkReply* reply = manager.get(request);
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QTimer::singleShot(LocationProbeTimeoutMs, &loop, &QEventLoop::quit);
            loop.exec();

            if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
                location = buildLocationSummaryFromPayload(reply->readAll());
                if (location.isEmpty()) {
                    lastError = locationProbeErrorMessage(
                        probeUrl,
                        QStringLiteral("Outbound location response was empty."));
                }
            } else if (reply->isFinished()) {
                lastError = locationProbeErrorMessage(probeUrl, reply->errorString());
            } else {
                lastError = locationProbeErrorMessage(
                    probeUrl,
                    QStringLiteral("Outbound location request timed out."));
                reply->abort();
            }
            reply->deleteLater();

            if (location.isEmpty()
                && probeTimer.elapsed() < LocationProbeTotalTimeoutMs
                && attempt < LocationProbeMaxAttempts) {
                QThread::msleep(LocationProbeRetryDelayMs);
            }
        }

        QObject* target = uiContext.isNull()
            ? static_cast<QObject*>(QCoreApplication::instance())
            : uiContext.data();
        invokeOnUiThread(target, [this,
                                  lifetimeGuard,
                                  activeServerId,
                                  location,
                                  lastError,
                                  locationStep,
                                  startupProxyEnabled]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (!coreReady_ || config_.currentIndexId.trimmed() != activeServerId) {
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Failed,
                    locationStep,
                    QStringLiteral("Outbound location detection result is stale."));
                setCurrentActivationPending_ = false;
                return;
            }
            currentServerLocation_ = location.trimmed();
            QPointer<MainWindow> mainWindowGuard(mainWindow_.get());
            if (!mainWindowGuard.isNull()) {
                mainWindowGuard->setCurrentServerLocation(currentServerLocation_);
            }
            if (currentServerLocation_.isEmpty()) {
                const QString message = lastError.trimmed().isEmpty()
                    ? QStringLiteral("Outbound location unavailable.")
                    : lastError;
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Failed,
                    locationStep,
                    message);
                setCurrentActivationPending_ = false;
                failCoreStartup(OperationResult::fail(message));
                return;
            }
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Passed,
                locationStep,
                currentServerLocation_);
            if (applySystemProxyAfterOutboundLocation(startupProxyEnabled)) {
                clearCoreStartupChecklistAfterStableRun(activeServerId, coreStartTime_);
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::clearCoreStartupChecklistAfterStableRun(const QString& serverIndexId, qint64 coreStartTime)
{
    const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - coreStartTime;
    const int remainingMs = static_cast<int>(qMax<qint64>(0, CoreStartupStableRunMs - elapsedMs));
    coreStartupChecklist_->clearAfterStableRun(remainingMs, [this, serverIndexId, coreStartTime]() {
        return coreReady_
            && coreStartTime_ == coreStartTime
            && config_.currentIndexId.trimmed() == serverIndexId.trimmed();
    });
}

bool AppBootstrap::reloadConfig()
{
    refreshExistingCoreTypes();
    const Config loadedConfig = repository_->load();
    const QString loadError = repository_->lastLoadError().trimmed();
    if (!loadError.isEmpty()) {
        appendResult(OperationResult::fail(loadError));
        DialogUtils::showCritical(
            mainWindow_.get(),
            QStringLiteral("Failed to Load Configuration"),
            loadError);
        return false;
    }

    config_ = loadedConfig;
    syncWindow();
    if (!uiStateRestored_ && mainWindow_ != nullptr) {
        mainWindow_->restoreUiState(config_);
        uiStateRestored_ = true;
    }
    appendResult(OperationResult::ok(QStringLiteral("Configuration reloaded from disk.")));
    appendStartupResourceCheckResults();
    if (uiReady_ && isCoreRunning()) {
        restartCoreIfRunning(
            QCoreApplication::translate("AppBootstrap", "Reloading core after reloading configuration."),
            true);
    }
    return true;
}

namespace {

VmessItem runtimeServerForLaunchCore(const VmessItem& server, CoreType launchCore)
{
    VmessItem runtimeServer = server;
    runtimeServer.coreType = launchCore;
    return runtimeServer;
}

} // namespace

void AppBootstrap::startCoreOnStartup()
{
    if (config_.ui().mainProxyEnabled) {
        if (skipCoreChecks_) {
            appendResult(OperationResult::ok(QStringLiteral("Startup core checks skipped by command line.")));
        }
        enableSystemProxy();
        return;
    }

    disableSystemProxy();
}

void AppBootstrap::persistUiState()
{
    if (mainWindow_ == nullptr || serverService_ == nullptr) {
        return;
    }

    mainWindow_->captureUiState(config_);
    serverService_->save(config_);
}

void AppBootstrap::appendStartupResourceCheckResults()
{
    QStringList lines;

    if (!config_.currentIndexId.trimmed().isEmpty()
        && findServerById(config_.currentIndexId) == nullptr
        && !config_.collection().servers.isEmpty()) {
        lines.append(QStringLiteral(
            "Startup check: The configured default server does not exist. The first available server will be used."));
    }

    int missingCustomConfigCount = 0;
    for (const VmessItem& item : config_.collection().servers) {
        if (item.configType != ConfigType::Custom) {
            continue;
        }

        const QString customConfigPath = resolveCustomConfigPath(item.address);
        if (customConfigPath.trimmed().isEmpty() || !QFileInfo::exists(customConfigPath)) {
            ++missingCustomConfigCount;
        }
    }

    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    if (activeServer.has_value()) {
        if (activeServer->configType == ConfigType::Custom) {
            const QString customConfigPath = resolveCustomConfigPath(activeServer->address);
            if (customConfigPath.trimmed().isEmpty() || !QFileInfo::exists(customConfigPath)) {
                lines.append(QStringLiteral("Startup check: Custom config file is missing for default server: %1")
                                 .arg(QDir::toNativeSeparators(customConfigPath)));
            }
        }

        if (!skipCoreChecks_) {
            const CoreInfo coreInfo = resolveCoreInfo(*activeServer);
            if (coreInfo.program.trimmed().isEmpty()) {
                const CoreType runtimeCore = resolveLaunchCoreType(*activeServer);
                const QStringList candidates = resolveCoreCandidates(runtimeCore);
                lines.append(QStringLiteral("Startup check: No compatible core executable was found for default server \"%1\". Expected one of: %2.")
                                 .arg(elidedServerDisplayName(*activeServer, 64))
                                 .arg(expectedCoreFilesText(candidates)));
            }

            for (const CoreType auxiliaryCoreType : resolveAuxiliaryTunCompatCoreTypes(config_, *activeServer, existingCoreTypes_)) {
                const QStringList candidates = resolveCoreCandidates(auxiliaryCoreType);
                if (locateFirstExistingFile(candidates).isEmpty()) {
                    lines.append(QStringLiteral("Startup check: Default server \"%1\" also needs the %2 core for TUN compatibility. Expected one of: %3.")
                                     .arg(elidedServerDisplayName(*activeServer, 64))
                                     .arg(coreTypeDisplayName(auxiliaryCoreType))
                                     .arg(expectedCoreFilesText(candidates)));
                }
            }
        }
    }

    if (missingCustomConfigCount > 0) {
        lines.append(QStringLiteral("Startup check: %1 custom server config file(s) are missing.").arg(missingCustomConfigCount));
    }

    for (const QString& line : lines) {
        appendResult(OperationResult::ok(line));
    }
}

void AppBootstrap::appendResult(const OperationResult& result)
{
    if (result.message.trimmed().isEmpty()) {
        return;
    }

    QStringList lines;
    const QStringList rawLines = result.message.split(QChar('\n'));
    for (const QString& rawLine : rawLines) {
        const QString line = rawLine.trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }

    if (lines.isEmpty()) {
        return;
    }

    if (mainWindow_ != nullptr) {
        for (const QString& line : lines) {
            mainWindow_->appendLog(line);
        }
        mainWindow_->showTransientStatus(lines.constFirst(), result.success ? 4000 : 8000);
    }
}

void AppBootstrap::prepareCoreStartupChecklist(bool showOverlay)
{
    QStringList startupSteps{
        QCoreApplication::translate("AppBootstrap", "Environment cleanup"),
        QCoreApplication::translate("AppBootstrap", "Validate core application"),
        QCoreApplication::translate("AppBootstrap", "Validate core config"),
        QCoreApplication::translate("AppBootstrap", "Validate geo files")
    };
    if (config_.tun().tunModeItem.enableTun) {
        startupSteps.append(QCoreApplication::translate("AppBootstrap", "Start tun device"));
    }
    startupSteps.append({
        QCoreApplication::translate("AppBootstrap", "Start core process"),
        QCoreApplication::translate("AppBootstrap", "Check outbound location"),
        QCoreApplication::translate("AppBootstrap", "Apply system proxy")
    });
    coreStartupChecklist_->reset(startupSteps, showOverlay);
    if (showOverlay && mainWindow_ != nullptr) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
}

void AppBootstrap::keepCoreStartupChecklistForUserDismissal(const QString& step, const QString& detail)
{
    const QString trimmedStep = step.trimmed().isEmpty()
        ? QCoreApplication::translate("AppBootstrap", "Core startup")
        : step;
    const QString trimmedDetail = detail.trimmed();
    appendResult(OperationResult::fail(
        trimmedDetail.isEmpty()
            ? QCoreApplication::translate("AppBootstrap", "%1 failed.").arg(trimmedStep)
            : QCoreApplication::translate("AppBootstrap", "%1 failed: %2").arg(trimmedStep, trimmedDetail)));
    coreStartupChecklist_->setCheckpointStatus(CoreStartupCheckpointStatus::Failed, trimmedStep, detail);
}

void AppBootstrap::failCoreStartup(const OperationResult& result)
{
    cleanupAfterFailedCoreStartup();
    appendResult(result);
    syncStatusIndicators();
}

void AppBootstrap::cleanupAfterFailedCoreStartup()
{
    cancelPendingCoreRestarts();
    disconnectPendingCoreStartConnection();

    const bool proxyConfigured =
        normalizeSystemProxyMode(config_.sysProxyType) != SystemProxyMode::ForcedClear
        || config_.ui().mainProxyEnabled;
    const bool proxyApplied = systemProxyService_ != nullptr && systemProxyService_->isEnabled();
    if (proxyConfigured || proxyApplied || managedSystemProxyActive_) {
        const bool proxyCleared = systemProxyService_ == nullptr
            ? true
            : updateSystemProxyMode(SystemProxyMode::ForcedClear);
        appendResult(proxyCleared
            ? OperationResult::ok(QStringLiteral("System proxy disabled because core startup failed."))
            : OperationResult::fail(QStringLiteral("Failed to disable system proxy after core startup failed.")));
        if (proxyCleared) {
            managedSystemProxyActive_ = false;
        }
    }
    clearProxyStateAfterCoreStopped();

    if (coreLifecycleService_ != nullptr && coreLifecycleService_->isRunning()) {
        coreStartupChecklist_->setKeepOnNextStop(true);
        coreStopPending_ = true;
        appendResult(coreLifecycleService_->stop(false));
    } else {
        coreStopPending_ = false;
    }
    stopAuxiliaryCore(false);

    coreStartPending_ = false;
    coreReady_ = false;
    coreTunEnabledAtStart_ = false;
    clearCurrentServerLocation();
}

void AppBootstrap::downloadMissingGeoFilesAndResumeStartup(
    const CoreInfo& coreInfo,
    const QString& geoValidationMessage,
    const QString& checkGeoStep,
    bool skipTunCleanup,
    std::optional<bool> startupProxyEnabled,
    bool showStartupOverlay)
{
    const QString targetDirectory = coreInfo.workingDirectory.trimmed().isEmpty()
        ? QFileInfo(coreInfo.program).absolutePath()
        : coreInfo.workingDirectory;
    if (targetDirectory.trimmed().isEmpty()) {
        failCoreStartup(OperationResult::fail(QStringLiteral("Core working directory is empty.")));
        return;
    }

    const QString startMessage = QCoreApplication::translate(
        "AppBootstrap",
        "Missing legacy geo files detected. Downloading to %1.")
            .arg(QDir::toNativeSeparators(targetDirectory));
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Started,
        checkGeoStep,
        startMessage);
    appendResult(OperationResult::ok(geoValidationMessage));

    coreStartPending_ = true;
    coreReady_ = false;
    cancelCoreStartAfterGeoUpdate_ = false;
    syncStatusIndicators();

    QObject* uiContext = mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this,
                                       targetDirectory,
                                       checkGeoStep,
                                       skipTunCleanup,
                                       startupProxyEnabled,
                                       showStartupOverlay,
                                       lifetimeGuard,
                                       uiContext]() {
        const auto reportProgress = [this, checkGeoStep, lifetimeGuard, uiContext](const QString& message) {
            if (message.trimmed().isEmpty()) {
                return;
            }

            invokeOnUiThread(uiContext, [this, checkGeoStep, message, lifetimeGuard]() {
                if (lifetimeGuard.expired()) {
                    return;
                }
                if (cancelCoreStartAfterGeoUpdate_) {
                    return;
                }

                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Started,
                    checkGeoStep,
                    message);
            });
        };

        GeoResourceUpdateService geoResourceUpdateService(targetDirectory, {}, reportProgress);
        const QList<OperationResult> results{
            geoResourceUpdateService.update(QStringLiteral("geosite")),
            geoResourceUpdateService.update(QStringLiteral("geoip"))};

        bool success = true;
        QStringList messages;
        for (const OperationResult& result : results) {
            success = success && result.success;
            if (!result.message.trimmed().isEmpty()) {
                messages.append(result.message.trimmed());
            }
        }

        const OperationResult result = success
            ? OperationResult::ok(messages.join(QChar('\n')))
            : OperationResult::fail(messages.join(QChar('\n')));

        invokeOnUiThread(uiContext, [this,
                                     result,
                                     checkGeoStep,
                                     skipTunCleanup,
                                     startupProxyEnabled,
                                     showStartupOverlay,
                                     lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }

            coreStartPending_ = false;
            syncStatusIndicators();

            if (shuttingDown_.load()) {
                return;
            }

            const bool startupCanceled = cancelCoreStartAfterGeoUpdate_;
            cancelCoreStartAfterGeoUpdate_ = false;

            if (startupCanceled) {
                keepCoreStartupChecklistForUserDismissal(
                    checkGeoStep,
                    QCoreApplication::translate(
                        "AppBootstrap",
                        "Proxy startup was canceled while Geo files were updating."));
                syncStatusIndicators();
                return;
            }

            if (!result.success) {
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Failed,
                    checkGeoStep,
                    result.message);
                failCoreStartup(result);
                return;
            }

            if (startupProxyEnabled.value_or(true)
                && normalizeSystemProxyMode(config_.sysProxyType) != SystemProxyMode::ForcedChange) {
                keepCoreStartupChecklistForUserDismissal(
                    checkGeoStep,
                    QCoreApplication::translate(
                        "AppBootstrap",
                        "Geo files were updated, but proxy startup was canceled."));
                syncStatusIndicators();
                return;
            }

            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Passed,
                checkGeoStep,
                result.message);
            startCoreInternal(skipTunCleanup, startupProxyEnabled, true, showStartupOverlay);
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::downloadMissingSingBoxRuleSetsAndResumeStartup(
    const QStringList& missingRuleSetTags,
    const QString& checkGeoStep,
    bool skipTunCleanup,
    std::optional<bool> startupProxyEnabled,
    bool showStartupOverlay)
{
    QStringList tags;
    QSet<QString> seenTags;
    for (const QString& tag : missingRuleSetTags) {
        const QString normalizedTag = tag.trimmed().toLower();
        if (normalizedTag.isEmpty() || seenTags.contains(normalizedTag)) {
            continue;
        }
        seenTags.insert(normalizedTag);
        tags.append(normalizedTag);
    }

    if (tags.isEmpty()) {
        startCoreInternal(skipTunCleanup, startupProxyEnabled, true, showStartupOverlay);
        return;
    }

    const QString targetDirectory = QCoreApplication::applicationDirPath();
    const QString startMessage = QCoreApplication::translate(
        "AppBootstrap",
        "Missing sing-box rule sets detected. Downloading %1 file(s) to %2.")
            .arg(tags.size())
            .arg(QDir::toNativeSeparators(QDir(targetDirectory).filePath(kSingBoxRuleSetDirectoryName)));
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Started,
        checkGeoStep,
        startMessage);

    coreStartPending_ = true;
    coreReady_ = false;
    cancelCoreStartAfterGeoUpdate_ = false;
    syncStatusIndicators();

    QObject* uiContext = mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this,
                                       targetDirectory,
                                       tags,
                                       checkGeoStep,
                                       skipTunCleanup,
                                       startupProxyEnabled,
                                       showStartupOverlay,
                                       lifetimeGuard,
                                       uiContext]() {
        const auto reportProgress = [this, checkGeoStep, lifetimeGuard, uiContext](const QString& message) {
            if (message.trimmed().isEmpty()) {
                return;
            }

            invokeOnUiThread(uiContext, [this, checkGeoStep, message, lifetimeGuard]() {
                if (lifetimeGuard.expired() || cancelCoreStartAfterGeoUpdate_) {
                    return;
                }

                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Started,
                    checkGeoStep,
                    message);
            });
        };

        GeoResourceUpdateService geoResourceUpdateService(targetDirectory, {}, reportProgress);
        QList<OperationResult> results;
        for (const QString& tag : tags) {
            results.append(geoResourceUpdateService.updateSingBoxRuleSet(tag));
        }

        bool success = true;
        QStringList messages;
        for (const OperationResult& result : results) {
            success = success && result.success;
            if (!result.message.trimmed().isEmpty()) {
                messages.append(result.message.trimmed());
            }
        }

        const OperationResult result = success
            ? OperationResult::ok(messages.join(QChar('\n')))
            : OperationResult::fail(messages.join(QChar('\n')));

        invokeOnUiThread(uiContext, [this,
                                     result,
                                     checkGeoStep,
                                     skipTunCleanup,
                                     startupProxyEnabled,
                                     showStartupOverlay,
                                     lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }

            coreStartPending_ = false;
            syncStatusIndicators();

            if (shuttingDown_.load()) {
                return;
            }

            const bool startupCanceled = cancelCoreStartAfterGeoUpdate_;
            cancelCoreStartAfterGeoUpdate_ = false;

            if (startupCanceled) {
                keepCoreStartupChecklistForUserDismissal(
                    checkGeoStep,
                    QCoreApplication::translate(
                        "AppBootstrap",
                        "Proxy startup was canceled while sing-box rule sets were updating."));
                syncStatusIndicators();
                return;
            }

            if (!result.success) {
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Failed,
                    checkGeoStep,
                    result.message);
                failCoreStartup(result);
                return;
            }

            if (startupProxyEnabled.value_or(true)
                && normalizeSystemProxyMode(config_.sysProxyType) != SystemProxyMode::ForcedChange) {
                keepCoreStartupChecklistForUserDismissal(
                    checkGeoStep,
                    QCoreApplication::translate(
                        "AppBootstrap",
                        "sing-box rule sets were updated, but proxy startup was canceled."));
                syncStatusIndicators();
                return;
            }

            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Passed,
                checkGeoStep,
                result.message);
            startCoreInternal(skipTunCleanup, startupProxyEnabled, true, showStartupOverlay);
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::downloadMissingCoreAndResumeStartup(
    CoreType coreType,
    const OperationResult& missingCoreResult,
    const QString& downloadCoreStep,
    bool skipTunCleanup,
    std::optional<bool> startupProxyEnabled,
    bool showStartupOverlay)
{
    const CoreType runtimeCore = resolveRuntimeCoreType(coreType);
    const QString installDirectory = resolveCoreInstallDirectory(runtimeCore);
    if (runtimeCore == CoreType::Unknown || installDirectory.trimmed().isEmpty()) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            downloadCoreStep,
            missingCoreResult.message);
        failCoreStartup(missingCoreResult);
        return;
    }

    const QString startMessage = QCoreApplication::translate(
        "AppBootstrap",
        "Missing %1 core detected. Downloading to %2.")
            .arg(coreTypeDisplayName(runtimeCore))
            .arg(QDir::toNativeSeparators(installDirectory));
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Started,
        downloadCoreStep,
        startMessage);
    appendResult(OperationResult::ok(missingCoreResult.message));

    if (!backgroundTasks_->tryBegin(BackgroundTaskCoordinator::Kind::CoreUpdate)) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            downloadCoreStep,
            QCoreApplication::translate("AppBootstrap", "Another background task is already running."));
        failCoreStartup(OperationResult::fail(QCoreApplication::translate(
            "AppBootstrap",
            "Cannot download %1 while another background task is running.")
                .arg(coreTypeDisplayName(runtimeCore))));
        return;
    }

    coreStartPending_ = true;
    coreReady_ = false;
    syncStatusIndicators();

    const CoreUpdateConfig workerConfig{
        config_.checkPreReleaseUpdate,
        config_.ignoreGeoUpdateCore};
    QObject* uiContext = mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this,
                                       runtimeCore,
                                       workerConfig,
                                       installDirectory,
                                       downloadCoreStep,
                                       skipTunCleanup,
                                       startupProxyEnabled,
                                       showStartupOverlay,
                                       lifetimeGuard,
                                       uiContext]() {
        runCoreUpdateTask(
            runtimeCore,
            workerConfig,
            installDirectory,
            QPointer<QObject>(uiContext),
            true,
            [this, downloadCoreStep, lifetimeGuard](const QString& message) {
                if (lifetimeGuard.expired()) {
                    return;
                }
                if (!coreStartPending_) {
                    return;
                }
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Started,
                    downloadCoreStep,
                    message);
            },
            [this,
             runtimeCore,
             downloadCoreStep,
             skipTunCleanup,
             startupProxyEnabled,
             showStartupOverlay,
             lifetimeGuard](const OperationResult& result) {
                if (lifetimeGuard.expired()) {
                    return;
                }

                backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::CoreUpdate);
                const bool startupCanceled = !coreStartPending_;
                coreStartPending_ = false;
                syncStatusIndicators();

                if (shuttingDown_.load()) {
                    return;
                }

                if (startupCanceled) {
                    keepCoreStartupChecklistForUserDismissal(
                        downloadCoreStep,
                        QCoreApplication::translate(
                            "AppBootstrap",
                            "Proxy startup was canceled while the core was downloading."));
                    syncStatusIndicators();
                    return;
                }

                if (!result.success) {
                    coreStartupChecklist_->setCheckpointStatus(
                        CoreStartupCheckpointStatus::Failed,
                        downloadCoreStep,
                        result.message);
                    failCoreStartup(result);
                    return;
                }

                if (startupProxyEnabled.value_or(true)
                    && normalizeSystemProxyMode(config_.sysProxyType) != SystemProxyMode::ForcedChange) {
                    keepCoreStartupChecklistForUserDismissal(
                        downloadCoreStep,
                        QCoreApplication::translate(
                            "AppBootstrap",
                            "The core was downloaded, but proxy startup was canceled."));
                    syncStatusIndicators();
                    return;
                }

                refreshExistingCoreTypes();
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Passed,
                    downloadCoreStep,
                    result.message);
                startCoreInternal(skipTunCleanup, startupProxyEnabled, true, showStartupOverlay);
            });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::showOperationMessage(
    const QString& title,
    const OperationResult& result,
    QWidget* parent,
    bool showDialog)
{
    appendResult(result);
    if (!showDialog || result.message.trimmed().isEmpty() || mainWindow_ == nullptr) {
        return;
    }

    QWidget* messageParent = parent != nullptr ? parent : mainWindow_.get();
    if (result.success) {
        DialogUtils::showInformation(messageParent, title, result.message);
        return;
    }

    DialogUtils::showWarning(messageParent, title, result.message);
}

void AppBootstrap::startCore(bool showStartupOverlay)
{
    enableSystemProxy(showStartupOverlay);
}

void AppBootstrap::startCore(bool skipTunCleanup, bool showStartupOverlay)
{
    setSystemProxyMode(SystemProxyMode::ForcedChange, skipTunCleanup, false, showStartupOverlay);
}

void AppBootstrap::startCoreInternal(
    bool skipTunCleanup,
    std::optional<bool> startupProxyEnabled,
    bool environmentAlreadyChecked,
    bool showStartupOverlay)
{
    const QString environmentStep = QCoreApplication::translate("AppBootstrap", "Environment cleanup");
    const QString downloadCoreStep = QCoreApplication::translate("AppBootstrap", "Validate core application");
    const QString checkGeoStep = QCoreApplication::translate("AppBootstrap", "Validate geo files");
    const QString validateConfigStep = QCoreApplication::translate("AppBootstrap", "Validate core config");
    const QString startTunDeviceStep = QCoreApplication::translate("AppBootstrap", "Start tun device");
    const QString startCoreStep = QCoreApplication::translate("AppBootstrap", "Start core process");
    if (!skipTunCleanup && !environmentAlreadyChecked) {
        prepareCoreStartupChecklist(showStartupOverlay);
    } else if (!coreStartupChecklist_->hasSteps()) {
        prepareCoreStartupChecklist(showStartupOverlay);
    } else if (showStartupOverlay && !coreStartupChecklist_->overlayRequested()) {
        coreStartupChecklist_->requestOverlay();
    }

    if (coreStartPending_ || coreStopPending_) {
        keepCoreStartupChecklistForUserDismissal(
            QCoreApplication::translate("AppBootstrap", "Start core process"),
            coreStopPending_
                ? QStringLiteral("Core stop is already in progress.")
                : QStringLiteral("Core start is already in progress."));
        return;
    }
    cancelPendingCoreRestarts();
    clearCurrentServerLocation();
    cleanupCoreProcessesUsingConfiguredPorts();
    if (isTunRuntimeBlocked(config_, isWindowsPlatform(), isProcessElevated())) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            environmentStep,
            QStringLiteral("Administrator permission is required for TUN."));
        failCoreStartup(OperationResult::fail(tunAdminRequiredStartMessage()));
        return;
    }
    const std::optional<VmessItem> server = resolveActiveServerSnapshot();
    if (!server.has_value()) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            validateConfigStep,
            QStringLiteral("No active server."));
        failCoreStartup(OperationResult::fail(QStringLiteral("No active server is available for runtime config generation.")));
        return;
    }

    if (config_.tun().tunModeItem.enableTun && !skipTunCleanup && !environmentAlreadyChecked) {
        if (tunCleanupActive_) {
            keepCoreStartupChecklistForUserDismissal(
                environmentStep,
                QStringLiteral("TUN adapter cleanup is already in progress."));
            return;
        }

        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Started,
            environmentStep,
            QStringLiteral("Removing stale TUN adapter if needed."));
        tunCleanupActive_ = true;
        resumeCoreStartAfterTunCleanup_ = true;
        syncStatusIndicators();
        removeStaleTunAdapterAsync([this, startupProxyEnabled, environmentStep](const OperationResult& result) {
            tunCleanupActive_ = false;
            const bool resumeStart = resumeCoreStartAfterTunCleanup_;
            resumeCoreStartAfterTunCleanup_ = false;
            syncStatusIndicators();
            const bool shouldResume = shouldResumeCoreStartAfterTunCleanup(
                    result.success,
                    resumeStart,
                    shuttingDown_.load(),
                    coreStopPending_);
            if (!shouldResume) {
                coreStartupChecklist_->setCheckpointStatus(
                    result.success ? CoreStartupCheckpointStatus::Passed : CoreStartupCheckpointStatus::Failed,
                    environmentStep,
                    result.message);
                if (!result.success) {
                    failCoreStartup(result);
                } else {
                    keepCoreStartupChecklistForUserDismissal(
                        environmentStep,
                        QStringLiteral("Core startup was canceled before TUN cleanup finished."));
                }
                return;
            }

            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Passed,
                environmentStep,
                result.message);
            if (shouldResume) {
                startCoreInternal(true, startupProxyEnabled, true, coreStartupChecklist_->overlayRequested());
            }
        });
        return;
    }

    if (!environmentAlreadyChecked) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Passed,
            environmentStep,
            skipTunCleanup
                ? QStringLiteral("TUN cleanup completed.")
                : QStringLiteral("No TUN cleanup required."));
    }

    const CoreType launchCore = resolveLaunchCoreType(*server);
    const VmessItem runtimeServer = runtimeServerForLaunchCore(*server, launchCore);
    Config runtimeConfig = config_;
    const CoreInfo coreInfo = resolveCoreInfo(runtimeServer);
    if (coreInfo.program.isEmpty()) {
        const CoreType runtimeCore = launchCore;
        const QStringList candidates = resolveCoreCandidates(runtimeCore);
        const QString expectedFiles = expectedCoreFilesText(candidates);
        const OperationResult missingCoreResult = OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "No compatible %1 core executable was found for the active server \"%2\". Expected one of: %3.")
                .arg(coreTypeDisplayName(runtimeCore))
                .arg(elidedServerDisplayName(*server, 64))
                .arg(expectedFiles));
        downloadMissingCoreAndResumeStartup(
            runtimeCore,
            missingCoreResult,
            downloadCoreStep,
            skipTunCleanup,
            startupProxyEnabled,
            showStartupOverlay);

        return;
    }
    if (coreStartupChecklist_->statusOf(downloadCoreStep) == CoreStartupCheckpointStatus::Pending) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Passed,
            downloadCoreStep,
            QStringLiteral("Required core executable is available."));
    }

    for (const CoreType auxiliaryCoreType : resolveAuxiliaryTunCompatCoreTypes(runtimeConfig, runtimeServer, existingCoreTypes_)) {
        const QStringList auxiliaryCandidates = resolveCoreCandidates(auxiliaryCoreType);
        const QString auxiliaryProgram = locateFirstExistingFile(auxiliaryCandidates);
        if (!auxiliaryProgram.isEmpty()) {
            continue;
        }

        const QString expectedFiles = expectedCoreFilesText(auxiliaryCandidates);
        const OperationResult missingAuxiliaryResult = OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "The active server \"%1\" needs the %2 core for TUN compatibility, but no compatible executable was found.\nExpected one of: %3.")
                .arg(elidedServerDisplayName(*server, 64))
                .arg(coreTypeDisplayName(auxiliaryCoreType))
                .arg(expectedFiles));
        downloadMissingCoreAndResumeStartup(
            auxiliaryCoreType,
            missingAuxiliaryResult,
            downloadCoreStep,
            skipTunCleanup,
            startupProxyEnabled,
            showStartupOverlay);

        return;
    }

    const QString coreConfigPath = resolveRuntimeConfigPath(*server);
    if (coreConfigPath.isEmpty()) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            validateConfigStep,
            QStringLiteral("Runtime config path is empty."));
        failCoreStartup(OperationResult::fail(QStringLiteral("Failed to resolve the runtime config output path.")));
        return;
    }

    const CoreType runtimeCore = launchCore;

    QStringList auxiliaryConfigPaths;
    removeStaleSingBoxCache();
    clientConfigWriter_->setExistingCoreTypes(existingCoreTypes_);
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Started,
        validateConfigStep,
        QStringLiteral("Generating runtime config: %1").arg(QDir::toNativeSeparators(coreConfigPath)));
    const OperationResult writeResult = clientConfigWriter_->writeClientConfigs(
        runtimeConfig,
        runtimeServer,
        coreConfigPath,
        &auxiliaryConfigPaths);
    if (!writeResult.success) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            validateConfigStep,
            writeResult.message);
        failCoreStartup(writeResult);
        return;
    }

    if (runtimeCore == CoreType::SingBox) {
        const QStringList missingRuleSets = missingSingBoxRuleSetTags(coreConfigPath);
        if (!missingRuleSets.isEmpty()) {
            downloadMissingSingBoxRuleSetsAndResumeStartup(
                missingRuleSets,
                checkGeoStep,
                skipTunCleanup,
                startupProxyEnabled,
                showStartupOverlay);
            return;
        }
    }

    const OperationResult geoResult = validateCoreGeoFilesBeforeStart(coreInfo);
    if (!geoResult.success) {
        if (!coreUsesLegacyGeoFiles(coreInfo)) {
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Failed,
                checkGeoStep,
                geoResult.message);
            failCoreStartup(geoResult);
            return;
        }

        downloadMissingGeoFilesAndResumeStartup(
            coreInfo,
            geoResult.message,
            checkGeoStep,
            skipTunCleanup,
            startupProxyEnabled,
            showStartupOverlay);
        return;
    }
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Passed,
        checkGeoStep,
        geoResult.message);

    if (!skipCoreChecks_) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Started,
            validateConfigStep,
            QFileInfo(coreInfo.program).fileName());
        const OperationResult preflightResult = validateCoreConfigBeforeStart(coreInfo, coreConfigPath);
        if (!preflightResult.success) {
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Failed,
                validateConfigStep,
                preflightResult.message);
            failCoreStartup(preflightResult);
            return;
        }
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Passed,
            validateConfigStep,
            preflightResult.message);
    } else {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Skipped,
            validateConfigStep,
            QStringLiteral("Startup checks are disabled."));
    }

    if (!auxiliaryConfigPaths.isEmpty()) {
        const QString auxiliaryProgram = locateFirstExistingFile(resolveCoreCandidates(CoreType::SingBox));
        if (auxiliaryProgram.isEmpty()) {
            const OperationResult missingAuxiliaryResult = OperationResult::fail(
                QStringLiteral("TUN compatibility mode requires sing-box, but no compatible sing-box executable was found."));
            downloadMissingCoreAndResumeStartup(
                CoreType::SingBox,
                missingAuxiliaryResult,
                downloadCoreStep,
                skipTunCleanup,
                startupProxyEnabled,
                showStartupOverlay);
            return;
        }

        CoreInfo auxiliaryCoreInfo;
        auxiliaryCoreInfo.program = auxiliaryProgram;
        auxiliaryCoreInfo.arguments = QStringList{QStringLiteral("run"), QStringLiteral("-c"), auxiliaryCoreInfo.configPlaceholder};
        auxiliaryCoreInfo.appendConfigArgument = false;
        auxiliaryCoreInfo.workingDirectory = QFileInfo(auxiliaryProgram).absolutePath();

        if (!skipCoreChecks_) {
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Started,
                startTunDeviceStep,
                QStringLiteral("Validating TUN compatibility relay."));
            const OperationResult auxiliaryPreflightResult = validateCoreConfigBeforeStart(
                auxiliaryCoreInfo,
                auxiliaryConfigPaths.constFirst());
            if (!auxiliaryPreflightResult.success) {
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Failed,
                    startTunDeviceStep,
                    auxiliaryPreflightResult.message);
                failCoreStartup(auxiliaryPreflightResult);
                return;
            }
        }

        stopAuxiliaryCore();
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Started,
            startTunDeviceStep,
            QStringLiteral("Starting TUN compatibility relay core."));
        const OperationResult auxiliaryStartResult = auxiliaryCoreLifecycleService_->start(
            auxiliaryCoreInfo,
            auxiliaryConfigPaths.constFirst());
        if (!auxiliaryStartResult.success) {
            stopAuxiliaryCore();
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Failed,
                startTunDeviceStep,
                auxiliaryStartResult.message);
            failCoreStartup(auxiliaryStartResult);
            return;
        }
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Passed,
            startTunDeviceStep,
            auxiliaryStartResult.message);
    } else {
        if (runtimeConfig.tun().tunModeItem.enableTun) {
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Started,
                startTunDeviceStep,
                QStringLiteral("TUN device will be started by the core process."));
        }
        stopAuxiliaryCore();
    }

    CoreInfo launchCoreInfo = coreInfo;
    launchCoreInfo.asyncStart = true;
    disconnectPendingCoreStartConnection();
    coreStartedConnection_ = QObject::connect(
        coreLifecycleService_.get(),
        &CoreLifecycleService::started,
        mainWindow_.get(),
        [this,
         serverIndexId = server->indexId,
         runtimeCore,
         customServer = (server->configType == ConfigType::Custom),
         startupProxyEnabled](const QString& message) mutable {
            disconnectPendingCoreStartConnection();
            coreStartTime_ = QDateTime::currentMSecsSinceEpoch();
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Started,
                QCoreApplication::translate("AppBootstrap", "Start core process"),
                message);
            if (coreTunEnabledAtStart_
                && coreStartupChecklist_->statusOf(QCoreApplication::translate("AppBootstrap", "Start tun device"))
                    == CoreStartupCheckpointStatus::Started) {
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Passed,
                    QCoreApplication::translate("AppBootstrap", "Start tun device"),
                    QStringLiteral("TUN device startup is handled by the core process."));
            }
            validateCoreListeningAndHandleStarted(serverIndexId, runtimeCore, customServer, startupProxyEnabled);
        });

    coreStartPending_ = true;
    coreReady_ = false;
    coreTunEnabledAtStart_ = runtimeConfig.tun().tunModeItem.enableTun;
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Started,
        startCoreStep,
        QFileInfo(launchCoreInfo.program).fileName());
    const OperationResult startResult = coreLifecycleService_->start(launchCoreInfo, coreConfigPath);
    if (!startResult.success) {
        coreStartPending_ = false;
        coreReady_ = false;
        const bool tunEnabledAtFailedStart = coreTunEnabledAtStart_;
        coreTunEnabledAtStart_ = false;
        disconnectPendingCoreStartConnection();
        stopAuxiliaryCore();
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            startCoreStep,
            startResult.message);
        if (tunEnabledAtFailedStart
            && coreStartupChecklist_->statusOf(startTunDeviceStep) == CoreStartupCheckpointStatus::Started) {
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Failed,
                startTunDeviceStep,
                startResult.message);
        }
        failCoreStartup(startResult);
        return;
    }
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Started,
        startCoreStep,
        startResult.message);

    syncStatusIndicators();
}

void AppBootstrap::handleCoreStarted(
    const QString& serverIndexId,
    CoreType runtimeCore,
    bool customServer,
    std::optional<bool> startupProxyEnabled)
{
    Q_UNUSED(runtimeCore);
    Q_UNUSED(customServer);

    if (mainWindow_ != nullptr) {
        mainWindow_->setCurrentServerWarning({});
    }
    coreStartPending_ = false;
    coreReady_ = true;
    disconnectPendingCoreStartConnection();

    syncStatusIndicators();
    queryCurrentServerLocation(serverIndexId, runtimeCore, startupProxyEnabled);
}

bool AppBootstrap::applySystemProxyAfterOutboundLocation(std::optional<bool> startupProxyEnabled)
{
    const bool shouldRestoreProxy = !startupProxyEnabled.has_value() || startupProxyEnabled.value();
    if (systemProxyService_ != nullptr
        && normalizeSystemProxyMode(config_.sysProxyType) == SystemProxyMode::ForcedChange
        && shouldRestoreProxy) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Started,
            QCoreApplication::translate("AppBootstrap", "Apply system proxy"),
            QStringLiteral("Applying Global system proxy."));
        const bool wasApplied = systemProxyService_->isEnabled();
        const bool proxyUpdated = updateSystemProxyMode(SystemProxyMode::ForcedChange);
        if (!proxyUpdated) {
            const QString message = QStringLiteral("Failed to apply the configured Global system proxy after starting the core.");
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Failed,
                QCoreApplication::translate("AppBootstrap", "Apply system proxy"),
                QStringLiteral("Failed to apply the configured Global system proxy."));
            failCoreStartup(OperationResult::fail(message));
            if (setCurrentActivationPending_) {
                if (mainWindow_ != nullptr) {
                    mainWindow_->setCurrentServerWarning(
                        QCoreApplication::translate("AppBootstrap", "Please verify manually"));
                }
                appendResult(OperationResult::fail(
                    QCoreApplication::translate(
                        "AppBootstrap",
                        "Node set successfully. It may be inaccessible. Please verify manually.")));
                setCurrentActivationPending_ = false;
            }
            syncStatusIndicators();
            return false;
        } else if (!wasApplied) {
            appendResult(OperationResult::ok(QStringLiteral(
                "Applied the configured Global system proxy after outbound location check.")));
        }
        if (proxyUpdated) {
            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Passed,
                QCoreApplication::translate("AppBootstrap", "Apply system proxy"),
                QStringLiteral("Global system proxy is active."));
            managedSystemProxyActive_ = true;
        }
    } else {
        const QString message = QStringLiteral("System proxy service is unavailable.");
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            QCoreApplication::translate("AppBootstrap", "Apply system proxy"),
            message);
        failCoreStartup(OperationResult::fail(message));
        setCurrentActivationPending_ = false;
        return false;
    }
    setCurrentActivationPending_ = false;
    syncStatusIndicators();
    return true;
}

void AppBootstrap::validateCoreListeningAndHandleStarted(
    const QString& serverIndexId,
    CoreType runtimeCore,
    bool customServer,
    std::optional<bool> startupProxyEnabled)
{
    const QString startCoreStep = QCoreApplication::translate("AppBootstrap", "Start core process");
    const bool usesDedicatedProbe = !customServer;
    const int listenPort = resolveLocationProbeHttpPort(config_, usesDedicatedProbe);
    if (!isValidTcpPort(listenPort)) {
        const QString message = QStringLiteral("Local proxy listen port is unavailable.");
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            startCoreStep,
            message);
        failCoreStartup(OperationResult::fail(message));
        return;
    }

    QPointer<QObject> uiContext(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    const QString activeServerId = serverIndexId.trimmed();
    const qint64 startTime = coreStartTime_;
    const bool tunEnabledAtStart = coreTunEnabledAtStart_;
    const int listenProbeTimeoutMs = tunEnabledAtStart
        ? TunCoreListenProbeTotalTimeoutMs
        : CoreListenProbeTotalTimeoutMs;
    QThread* thread = QThread::create([this,
                                       uiContext,
                                       lifetimeGuard,
                                       activeServerId,
                                       runtimeCore,
                                       customServer,
                                       startupProxyEnabled,
                                       startCoreStep,
                                       startTime,
                                       listenPort,
                                       listenProbeTimeoutMs]() {
        const bool portReady = waitForLocalTcpPortReady(listenPort, listenProbeTimeoutMs);

        QObject* target = uiContext.isNull()
            ? static_cast<QObject*>(QCoreApplication::instance())
            : uiContext.data();
        invokeOnUiThread(target, [this,
                                  lifetimeGuard,
                                  activeServerId,
                                  runtimeCore,
                                  customServer,
                                  startupProxyEnabled,
                                  startCoreStep,
                                  startTime,
                                  listenPort,
                                  listenProbeTimeoutMs,
                                  portReady]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (coreStartTime_ != startTime
                || config_.currentIndexId.trimmed() != activeServerId) {
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Skipped,
                    startCoreStep,
                    QStringLiteral("Core listening validation result is stale."));
                // Defensive: currentIndexId drifted but no fresh startCoreInternal was
                // issued (coreStartTime_ unchanged). The launched core is orphaned —
                // stop it so coreReady_/managedSystemProxyActive_ stay consistent.
                if (coreStartTime_ == startTime && isCoreRunning() && !coreStopPending_) {
                    stopCore(false);
                }
                return;
            }

            if (!portReady) {
                const QString message = QStringLiteral(
                    "Core did not open the local proxy port %1 within %2 seconds.")
                    .arg(listenPort)
                    .arg((listenProbeTimeoutMs + 999) / 1000);
                coreStartupChecklist_->setCheckpointStatus(
                    CoreStartupCheckpointStatus::Failed,
                    startCoreStep,
                    message);
                failCoreStartup(OperationResult::fail(message));
                return;
            }

            coreStartupChecklist_->setCheckpointStatus(
                CoreStartupCheckpointStatus::Passed,
                startCoreStep,
                QStringLiteral("Core local proxy is listening on 127.0.0.1:%1.").arg(listenPort));
            handleCoreStarted(activeServerId, runtimeCore, customServer, startupProxyEnabled);
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::handleCoreStartFailed(const QString& message)
{
    const bool tunEnabledAtFailedStart = coreTunEnabledAtStart_;
    coreStartupChecklist_->setCheckpointStatus(
        CoreStartupCheckpointStatus::Failed,
        QCoreApplication::translate("AppBootstrap", "Start core process"),
        message);
    if (tunEnabledAtFailedStart
        && coreStartupChecklist_->statusOf(QCoreApplication::translate("AppBootstrap", "Start tun device"))
            == CoreStartupCheckpointStatus::Started) {
        coreStartupChecklist_->setCheckpointStatus(
            CoreStartupCheckpointStatus::Failed,
            QCoreApplication::translate("AppBootstrap", "Start tun device"),
            message);
    }
    coreStartPending_ = false;
    coreStopPending_ = false;
    coreReady_ = false;
    coreTunEnabledAtStart_ = false;
    disconnectPendingCoreStartConnection();
    failCoreStartup(OperationResult::fail(message));
    if (setCurrentActivationPending_) {
        if (mainWindow_ != nullptr) {
            mainWindow_->setCurrentServerWarning(
                QCoreApplication::translate("AppBootstrap", "Please verify manually"));
        }
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "Node set successfully. It may be inaccessible. Please verify manually.")));
        setCurrentActivationPending_ = false;
    }
    stopAuxiliaryCore();
    clearCurrentServerLocation();
}

void AppBootstrap::disconnectPendingCoreStartConnection()
{
    if (coreStartedConnection_) {
        QObject::disconnect(coreStartedConnection_);
        coreStartedConnection_ = {};
    }
}

void AppBootstrap::stopCore(bool immediate)
{
    setSystemProxyMode(SystemProxyMode::ForcedClear, false, immediate);
}

void AppBootstrap::stopCoreInternal(bool immediate, bool clearRestartAfterStop)
{
    const bool keepStartupChecklist = coreStartupChecklist_->consumeKeepOnNextStop();
    const bool startupChecklistVisible = coreStartupChecklist_->overlayActive();
    const bool resourceStartupPending = (backgroundTasks_->isKindActive(BackgroundTaskCoordinator::Kind::GeoUpdate)
        || backgroundTasks_->isKindActive(BackgroundTaskCoordinator::Kind::CoreUpdate))
        && coreStartPending_ && !isCoreRunning();
    if (resourceStartupPending) {
        cancelCoreStartAfterGeoUpdate_ = true;
    }
    coreStartPending_ = false;
    coreReady_ = false;
    if (!startupChecklistVisible
        && !restartAfterStopPending_
        && pendingDefaultServerAfterStopId_.trimmed().isEmpty()
        && !keepStartupChecklist) {
        coreStartupChecklist_->clear();
    }
    clearCurrentServerLocation();
    disconnectPendingCoreStartConnection();
    cancelPendingCoreRestarts();
    resumeCoreStartAfterTunCleanup_ = false;
    if (clearRestartAfterStop) {
        restartAfterStopPending_ = false;
        restartAfterStopShowStartupOverlay_ = false;
    }
    clearAppliedSystemProxyAfterCoreStopped();
    if (clearRestartAfterStop) {
        clearProxyStateAfterCoreStopped();
    }
    if (!immediate && coreLifecycleService_ != nullptr && coreLifecycleService_->isRunning()) {
        coreStopPending_ = true;
    } else {
        coreStopPending_ = false;
    }
    skipTunCleanupOnNextCoreExit_ = immediate
        && shouldCleanupTunAfterCoreStop(isWindowsPlatform(), coreTunEnabledAtStart_);
    const bool cleanupTunSynchronously = skipTunCleanupOnNextCoreExit_;
    OperationResult stopResult = resourceStartupPending
        ? OperationResult::ok(QStringLiteral("Core startup will stop after the pending resource download finishes."))
        : coreLifecycleService_->stop(immediate);
    appendResult(stopResult);
    stopAuxiliaryCore(true);
    if (cleanupTunSynchronously && stopResult.success) {
        tunCleanupActive_ = false;
        removeStaleTunAdapter();
    }
    syncStatusIndicators();
}

void AppBootstrap::stopAuxiliaryCore(bool immediate)
{
    if (auxiliaryCoreLifecycleService_ != nullptr && auxiliaryCoreLifecycleService_->isRunning()) {
        appendResult(auxiliaryCoreLifecycleService_->stop(immediate));
    }
}

OperationResult AppBootstrap::removeStaleTunAdapterIfPresent() const
{
#if defined(Q_OS_WIN)
    if (!isTunAdapterPresent()) {
        return OperationResult::ok(QString());
    }
#endif
    QProcess remover;
    remover.setProgram(QStringLiteral("powershell"));
    remover.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-Command"),
        QStringLiteral("$ErrorActionPreference='Stop'; Remove-NetAdapter -Name 'singbox_tun' -Confirm:$false -ErrorAction Stop")
    });
#if defined(Q_OS_WIN)
    remover.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    remover.start();
    const bool finished = remover.waitForFinished(3000);
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
    if (remover.exitCode() != 0) {
        return OperationResult::fail(
            QStringLiteral("Failed to remove the 'singbox_tun' adapter."));
    }
#if defined(Q_OS_WIN)
    if (isTunAdapterPresent()) {
        return OperationResult::fail(
            QStringLiteral("The 'singbox_tun' adapter is still present after cleanup."));
    }
#endif
    return OperationResult::ok(
        QStringLiteral("Cleaned any stale 'singbox_tun' adapter."));
}

void AppBootstrap::removeStaleTunAdapterAsync(const std::function<void(const OperationResult&)>& completion)
{
    QPointer<QObject> mainWindowGuard(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, mainWindowGuard, completion, lifetimeGuard]() {
        const OperationResult result = removeStaleTunAdapterIfPresent();
        QObject* uiContext = mainWindowGuard.isNull()
            ? static_cast<QObject*>(QCoreApplication::instance())
            : mainWindowGuard.data();
        invokeOnUiThread(uiContext, [completion, result, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (completion) {
                completion(result);
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::removeStaleTunAdapter()
{
    appendResult(removeStaleTunAdapterIfPresent());
}

void AppBootstrap::removeStaleSingBoxCache()
{
    // Keep the sing-box cache file across restarts so remote rule-set downloads
    // can be reused when the upstream proxy is unstable.
}

void AppBootstrap::cleanupOrphanCoreProcesses()
{
#if defined(Q_OS_WIN)
    const QSet<qint64> recordedPids = readCorePids();
    QStringList terminatedProcesses = terminateCorePids(recordedPids);
    writeCorePids(QSet<qint64>());

    if (!terminatedProcesses.isEmpty()) {
        appendResult(OperationResult::ok(
            QStringLiteral("Cleaned up orphan core processes: %1").arg(terminatedProcesses.join(QStringLiteral(", ")))));
    }
#endif
}

void AppBootstrap::cleanupCoreProcessesUsingConfiguredPorts()
{
#if defined(Q_OS_WIN)
    QSet<quint16> ports;
    const auto addPort = [&ports](int port) {
        if (port > 0 && port <= 65535) {
            ports.insert(static_cast<quint16>(port));
        }
    };
    addPort(config_.localPort);
    addPort(config_.localPort + 1);
    addPort(config_.localPort + LocationProbePortOffset);

    QSet<qint64> pids;
    for (DWORD pid : tcpListeningPidsForPorts(ports)) {
        pids.insert(static_cast<qint64>(pid));
    }

    const QStringList terminatedProcesses = terminateCorePids(pids);
    if (!terminatedProcesses.isEmpty()) {
        appendResult(OperationResult::ok(
            QStringLiteral("Cleaned up core processes using configured ports: %1")
                .arg(terminatedProcesses.join(QStringLiteral(", ")))));
    }
#endif
}

void AppBootstrap::handleCoreExited(int exitCode, int status, bool stopRequested, bool auxiliary)
{
    if (!auxiliary) {
        coreStartPending_ = false;
        coreReady_ = false;
        disconnectPendingCoreStartConnection();
        if (stopRequested && !coreStartupChecklist_->overlayActive()) {
            coreStartupChecklist_->clear();
        }
    }

    if (!auxiliary) {
        coreStopPending_ = false;
    }

    const bool cleanupTunAfterStop = !auxiliary && shouldCleanupTunAfterCoreStop(
        isWindowsPlatform(),
        coreTunEnabledAtStart_);
    const bool cleanupAlreadyHandledOnImmediateStop = !auxiliary && skipTunCleanupOnNextCoreExit_;
    if (!auxiliary) {
        coreTunEnabledAtStart_ = false;
        skipTunCleanupOnNextCoreExit_ = false;
    }

    const bool switchDefaultServerAfterStop = !auxiliary && stopRequested && !pendingDefaultServerAfterStopId_.trimmed().isEmpty();
    const bool restartAfterStop = !auxiliary && stopRequested && restartAfterStopPending_ && !switchDefaultServerAfterStop;
    const bool restartAfterStopShowStartupOverlay = restartAfterStopShowStartupOverlay_;
    if (restartAfterStop) {
        restartAfterStopPending_ = false;
        restartAfterStopShowStartupOverlay_ = false;
    }
    if (!auxiliary) {
        clearAppliedSystemProxyAfterCoreStopped();
    }

    syncStatusIndicators();
    if (cleanupAlreadyHandledOnImmediateStop) {
        return;
    }

    if (cleanupTunAfterStop && stopRequested) {
        if (tunCleanupActive_) {
            return;
        }

        tunCleanupActive_ = true;
        resumeCoreStartAfterTunCleanup_ = false;
        syncStatusIndicators();
        removeStaleTunAdapterAsync([this, switchDefaultServerAfterStop, restartAfterStop, restartAfterStopShowStartupOverlay](const OperationResult& result) {
            tunCleanupActive_ = false;
            appendResult(result);
            syncStatusIndicators();
            if (shouldRunPostStopActionAfterTunCleanup(
                    result.success,
                    coreUpdatePendingAfterStop_,
                    shuttingDown_.load())) {
                continuePendingCoreUpdate();
                return;
            }
            if (shouldRunPostStopActionAfterTunCleanup(
                    result.success,
                    switchDefaultServerAfterStop,
                    shuttingDown_.load())) {
                scheduleDefaultServerSwitchAfterCoreStopped();
                return;
            }
            if (shouldRunPostStopActionAfterTunCleanup(
                    result.success,
                    restartAfterStop,
                    shuttingDown_.load())) {
                scheduleCoreStartAfterCoreStopped(restartAfterStopShowStartupOverlay);
            }
        });
        if (shuttingDown_.load() || stopRequested) {
            return;
        }
    } else if (coreUpdatePendingAfterStop_ && !shuttingDown_.load()) {
        continuePendingCoreUpdate();
    } else if (switchDefaultServerAfterStop && !shuttingDown_.load()) {
        scheduleDefaultServerSwitchAfterCoreStopped();
    } else if (restartAfterStop && !shuttingDown_.load()) {
        scheduleCoreStartAfterCoreStopped(restartAfterStopShowStartupOverlay);
    }
    if (shuttingDown_.load() || stopRequested) {
        return;
    }

    const QProcess::ExitStatus exitStatus = static_cast<QProcess::ExitStatus>(status);
    const QString core = auxiliary ? QStringLiteral("Auxiliary core") : QStringLiteral("Core");
    const QString kind = exitStatus == QProcess::CrashExit
        ? QStringLiteral("crash")
        : QStringLiteral("exit");

    if (stopRequested) {
        return;
    }

    int& crashCount = auxiliary ? auxiliaryCrashRestartCount_ : coreCrashRestartCount_;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 runDuration = coreStartTime_ > 0 ? (now - coreStartTime_) : 0;

    constexpr qint64 kStableRunThresholdMs = 60000;
    if (runDuration > kStableRunThresholdMs) {
        crashCount = 0;
    }
    ++crashCount;

    constexpr int kMaxCrashRestarts = 5;
    if (crashCount > kMaxCrashRestarts) {
        appendResult(OperationResult::ok(
            QStringLiteral("%1 %2 detected (code=%3). Auto-restart disabled after %4 consecutive failures.")
                .arg(core, kind).arg(exitCode).arg(kMaxCrashRestarts)));
        if (!auxiliary && mainWindow_) {
            mainWindow_->showTransientStatus(
                QCoreApplication::translate(
                    "AppBootstrap",
                    "Core crashed %1 times. Auto-restart disabled. Check your configuration.")
                    .arg(kMaxCrashRestarts),
                0);
        }
        if (!auxiliary) {
            clearProxyStateAfterCoreStopped();
            syncStatusIndicators();
        }
        return;
    }

    const int delayMs = std::min(3000 * crashCount, 30000);
    scheduleCoreRestart(
        QStringLiteral("%1 %2 detected (code=%3). Restarting in %4s... (attempt %5/%6)")
            .arg(core, kind).arg(exitCode).arg(delayMs / 1000).arg(crashCount).arg(kMaxCrashRestarts),
        auxiliary,
        delayMs);
}

void AppBootstrap::scheduleCoreRestart(const QString& reason, bool auxiliary, int delayMs)
{
    if (shuttingDown_.load()) {
        return;
    }
    if (!reason.isEmpty()) {
        appendResult(OperationResult::ok(reason));
    }
    QTimer*& slot = auxiliary ? auxiliaryRestartTimer_ : coreRestartTimer_;
    if (slot == nullptr) {
        slot = new QTimer(mainWindow_.get());
        slot->setSingleShot(true);
        QObject::connect(slot, &QTimer::timeout, mainWindow_.get(), [this, auxiliary]() {
            if (shuttingDown_.load()) {
                return;
            }

            if (!auxiliary && config_.tun().tunModeItem.enableTun) {
                if (tunCleanupActive_) {
                    return;
                }

                tunCleanupActive_ = true;
                resumeCoreStartAfterTunCleanup_ = false;
                syncStatusIndicators();
                removeStaleTunAdapterAsync([this](const OperationResult& result) {
                    tunCleanupActive_ = false;
                    appendResult(result);
                    syncStatusIndicators();
                    if (shouldResumeCoreStartAfterTunCleanup(
                            result.success,
                            true,
                            shuttingDown_.load(),
                            coreStopPending_)) {
                        startCoreInternal(true, true, false, false);
                    }
                });
                return;
            }

            startCoreInternal(false, true, false, false);
        });
    }
    slot->start(delayMs);
}

void AppBootstrap::cancelPendingCoreRestarts()
{
    if (coreRestartTimer_ != nullptr) {
        coreRestartTimer_->stop();
    }
    if (auxiliaryRestartTimer_ != nullptr) {
        auxiliaryRestartTimer_->stop();
    }
}

void AppBootstrap::restartCoreIfRunning(const QString& reason, bool showStartupOverlay)
{
    if (!isCoreRunning() || coreStopPending_) {
        setCurrentActivationPending_ = false;
        return;
    }

    if (!reason.isEmpty()) {
        appendResult(OperationResult::ok(reason));
    }
    restartAfterStopPending_ = true;
    restartAfterStopShowStartupOverlay_ = showStartupOverlay;
    prepareCoreStartupChecklist(showStartupOverlay);
    stopCoreInternal(false, false);
}

void AppBootstrap::scheduleCoreStartAfterCoreStopped(bool showStartupOverlay)
{
    QPointer<QObject> mainWindowGuard(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QTimer::singleShot(0, mainWindowGuard.isNull() ? QCoreApplication::instance() : mainWindowGuard.data(), [this, showStartupOverlay, lifetimeGuard]() {
        if (lifetimeGuard.expired() || shuttingDown_.load()) {
            return;
        }

        startCoreInternal(false, true, false, showStartupOverlay);
    });
}

bool AppBootstrap::saveSystemProxyMode(SystemProxyMode mode)
{
    if (serverService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("System proxy mode cannot be changed before the configuration service is ready.")));
        return false;
    }

    const int previousValue = config_.sysProxyType;
    const bool previousMainProxyEnabled = config_.ui().mainProxyEnabled;
    config_.sysProxyType = toLegacySystemProxyModeValue(mode);
    config_.ui().mainProxyEnabled = mode == SystemProxyMode::ForcedChange;
    if (!serverService_->save(config_)) {
        config_.sysProxyType = previousValue;
        config_.ui().mainProxyEnabled = previousMainProxyEnabled;
        appendResult(OperationResult::fail(QStringLiteral("Failed to save the selected system proxy mode.")));
        syncStatusIndicators();
        return false;
    }

    return true;
}

void AppBootstrap::setSystemProxyMode(
    SystemProxyMode mode,
    bool skipTunCleanup,
    bool immediateStop,
    bool showStartupOverlay)
{
    const bool coreProcessRunning = isCoreRunning();
    const bool coreReady = isCoreReady();
    const bool coreActivationPending = coreStartPending_ || resumeCoreStartAfterTunCleanup_;
    if (mode == SystemProxyMode::ForcedChange
        && !coreProcessRunning
        && coreActivationPending) {
        if (showStartupOverlay && coreStartupChecklist_->hasSteps()) {
            coreStartupChecklist_->requestOverlay();
        }
        appendResult(OperationResult::ok(QStringLiteral("Core start is already in progress.")));
        syncStatusIndicators();
        return;
    }

    if (!saveSystemProxyMode(mode)) {
        return;
    }

    const bool deferManagedApply = mode == SystemProxyMode::ForcedChange && !coreReady;
    const bool success = deferManagedApply
        || updateSystemProxyMode(mode);
    appendResult(success
        ? OperationResult::ok(
              deferManagedApply
                  ? QStringLiteral("System proxy mode set to %1. It will be applied after the core starts.")
                        .arg(systemProxyModeDisplayName(mode))
                  : QStringLiteral("System proxy mode set to %1.").arg(systemProxyModeDisplayName(mode)))
        : OperationResult::fail(QStringLiteral("Failed to apply system proxy mode %1.").arg(systemProxyModeDisplayName(mode))));
    if (success) {
        managedSystemProxyActive_ = deferManagedApply
            ? false
            : expectedSystemProxyEnabled(mode);
    }

    if (mode == SystemProxyMode::ForcedChange && !coreProcessRunning) {
        appendResult(OperationResult::ok(QStringLiteral("Starting the active core because Global system proxy mode was enabled.")));
        startCoreInternal(skipTunCleanup, true, false, showStartupOverlay);
        return;
    }

    if (mode == SystemProxyMode::ForcedClear && (coreProcessRunning || coreActivationPending)) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping the active core because system proxy was cleared.")));
        cancelPendingCoreRestarts();
        stopCoreInternal(immediateStop, true);
        return;
    }

    syncStatusIndicators();
}

void AppBootstrap::clearAppliedSystemProxyAfterCoreStopped()
{
    if (systemProxyService_ == nullptr
        || !shouldClearManagedSystemProxy(managedSystemProxyActive_, systemProxyService_->isEnabled())) {
        return;
    }

    const bool proxyCleared = updateSystemProxyMode(SystemProxyMode::ForcedClear);
    appendResult(proxyCleared
        ? OperationResult::ok(QStringLiteral("System proxy disabled because the core stopped."))
        : OperationResult::fail(QStringLiteral("Failed to disable system proxy after the core stopped.")));
    if (proxyCleared) {
        managedSystemProxyActive_ = false;
    }
}

void AppBootstrap::clearProxyStateAfterCoreStopped()
{
    if (config_.sysProxyType == toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear)
        && !config_.ui().mainProxyEnabled) {
        return;
    }

    saveSystemProxyMode(SystemProxyMode::ForcedClear);
}

void AppBootstrap::applySystemProxyModeOnExit(bool windowsShutdown)
{
    if (exitProxyStateApplied_ || systemProxyService_ == nullptr) {
        return;
    }

    exitProxyStateApplied_ = true;
    if (windowsShutdown) {
        if (!managedSystemProxyActive_) {
            return;
        }
        systemProxyService_->resetOnShutdown();
        managedSystemProxyActive_ = false;
        return;
    }

    if (!managedSystemProxyActive_) {
        return;
    }

    const SystemProxyMode mode = resolveSystemProxyModeOnExit(
        normalizeSystemProxyMode(config_.sysProxyType),
        windowsShutdown);
    updateSystemProxyMode(mode);
    managedSystemProxyActive_ = expectedSystemProxyEnabled(mode);
}

void AppBootstrap::cleanupRuntimeForExit(bool windowsShutdown)
{
    applySystemProxyModeOnExit(windowsShutdown);
    if (coreLifecycleService_ != nullptr && coreLifecycleService_->isRunning()) {
        coreLifecycleService_->stop(true);
    }
    stopAuxiliaryCore(true);
    coreStartPending_ = false;
    coreStopPending_ = false;
    coreReady_ = false;
}

void AppBootstrap::enableSystemProxy(bool showStartupOverlay)
{
    setSystemProxyMode(SystemProxyMode::ForcedChange, false, false, showStartupOverlay);
}

void AppBootstrap::disableSystemProxy()
{
    setSystemProxyMode(SystemProxyMode::ForcedClear);
}

void AppBootstrap::toggleAutoRun()
{
    if (autoRunService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("Auto run service is unavailable.")));
        return;
    }

    const bool nextState = !autoRunService_->isEnabled();
    const bool success = autoRunService_->setEnabled(nextState);
    if (success) {
        appendResult(OperationResult::ok(nextState
            ? QStringLiteral("Auto run enabled.")
            : QStringLiteral("Auto run disabled.")));
    } else {
        appendResult(OperationResult::fail(QStringLiteral("Failed to update auto run setting.")));
    }

    syncStatusIndicators();
}

void AppBootstrap::setTunEnabled(bool enabled)
{
    if (serverService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("TUN mode cannot be changed before the configuration service is ready.")));
        syncStatusIndicators();
        return;
    }

    if (config_.tun().tunModeItem.enableTun == enabled) {
        syncStatusIndicators();
        return;
    }

    if (enabled && !resolveActiveServerSnapshot().has_value()) {
        appendResult(OperationResult::fail(QCoreApplication::translate("AppBootstrap", "Please select a server first.")));
        syncWindow();
        return;
    }

    const Config previousConfig = config_;
    const Config updatedConfig = prepareTunToggleConfigForSave(
        config_,
        enabled,
        isWindowsPlatform(),
        isProcessElevated());
    const bool shouldEnableProxyWithTun =
        enabled
        && !previousConfig.tun().tunModeItem.enableTun
        && !isCoreRunning();

    const TunSettingsSaveBehavior tunSaveBehavior = evaluateTunSettingsSaveBehavior(
        previousConfig,
        updatedConfig,
        isWindowsPlatform(),
        isProcessElevated(),
        isCoreRunning());
    const TunSettingsApplyDecision& tunDecision = tunSaveBehavior.applyDecision;

    if (tunSaveBehavior.shouldPromptForAdminRestart && !askRestartAsAdministratorForTun()) {
        syncWindow();
        return;
    }

    config_ = tunSaveBehavior.configToPersist;
    if (!serverService_->save(config_)) {
        config_ = previousConfig;
        appendResult(OperationResult::fail(QStringLiteral("Failed to save the TUN mode setting.")));
        syncWindow();
        return;
    }

    appendResult(OperationResult::ok(
        enabled
            ? QStringLiteral("TUN mode enabled.")
            : QStringLiteral("TUN mode disabled.")));

    if (tunSaveBehavior.shouldPromptForAdminRestart) {
        appendResult(OperationResult::fail(tunAdminRequiredSaveMessage()));
    }

    syncWindow();

    if (tunDecision.shouldRestartRunningCore) {
        restartCoreIfRunning(QCoreApplication::translate(
            "AppBootstrap",
            "Reloading core after applying TUN mode changes."),
            true);
    } else if (shouldEnableProxyWithTun && !tunSaveBehavior.shouldPromptForAdminRestart) {
        enableSystemProxy(true);
    } else {
        syncStatusIndicators();
    }

    if (tunSaveBehavior.shouldPromptForAdminRestart) {
        persistUiState();
        config_.ui().mainProxyEnabled = true;
        config_.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange);
        serverService_->save(config_);
        if (!restartApplication(true)) {
            config_ = previousConfig;
            serverService_->save(config_);
            appendResult(OperationResult::fail(tunAdminRestartFailureMessage()));
            syncWindow();
        }
    }
}

void AppBootstrap::importFromClipboard()
{
    const QString text = QApplication::clipboard() == nullptr
        ? QString()
        : QApplication::clipboard()->text().trimmed();
    if (text.isEmpty()) {
        appendResult(OperationResult::fail(QStringLiteral("Clipboard text is empty.")));
        return;
    }

    for (const QString& line : SubscriptionImportTextParser::nonEmptyLines(text)) {
        if (SubscriptionImportTextParser::isSubscriptionUrl(line)) {
            importSubscriptionUrlsFromTextAsync(text);
            return;
        }
    }

    importClipboardTextAsync(text);
}

void AppBootstrap::importClipboardTextAsync(const QString& text)
{
    if (subscriptionService_ == nullptr || serverService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("Clipboard import service is unavailable.")));
        return;
    }

    if (!backgroundTasks_->tryBegin(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)) {
        return;
    }

    const QString startupMessage = QCoreApplication::translate(
        "AppBootstrap",
        "Importing clipboard content in the background...");
    if (mainWindow_ != nullptr) {
        mainWindow_->appendLog(startupMessage);
        mainWindow_->showTransientStatus(
            startupMessage,
            3000,
            MainWindow::TransientStatusPriority::Routine);
        mainWindow_->setSubscriptionUpdateRunning(true);
    }

    const QString configPath = resolveConfigPath();
    const QString customConfigDirectory = resolveCustomConfigDirectory();
    QObject* uiContext = mainWindow_ == nullptr
        ? static_cast<QObject*>(QCoreApplication::instance())
        : mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, text, configPath, customConfigDirectory, uiContext, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        ServerService serverService(repository, customConfigDirectory);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();

        OperationResult result = subscriptionUpdateService.importFromText(workerConfig, text);
        if (!result.success) {
            const OperationResult customImportResult =
                importCustomConfigTextWithService(text, workerConfig, serverService);
            if (customImportResult.success) {
                result = customImportResult;
            }
        }

        invokeOnUiThread(uiContext, [this, result, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::SubscriptionUpdate);
            if (mainWindow_ != nullptr) {
                mainWindow_->setSubscriptionUpdateRunning(false);
            }
            if (repository_ != nullptr && result.success) {
                config_ = repository_->load();
            }
            appendResult(result);
            if (result.success) {
                syncWindow();
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::importSubscriptionUrlsFromTextAsync(const QString& text)
{
    if (subscriptionService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("Subscription import service is unavailable.")));
        return;
    }

    if (!backgroundTasks_->tryBegin(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)) {
        return;
    }

    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    const QString activeSubscriptionId = activeServer.has_value() ? activeServer->subId.trimmed() : QString();
    if (mainWindow_ != nullptr) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Importing subscription URLs from the clipboard in the background...");
        mainWindow_->appendLog(message);
        mainWindow_->showTransientStatus(
            message,
            3000,
            MainWindow::TransientStatusPriority::Routine);
    }

    for (auto& server : config_.collection().servers) {
        server.testResult = QStringLiteral("...");
    }
    syncWindow();

    if (mainWindow_ != nullptr) {
        mainWindow_->setSubscriptionUpdateRunning(true);
    }
    const QString configPath = resolveConfigPath();
    QObject* uiContext = mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, text, configPath, activeSubscriptionId, uiContext, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();
        QList<SubItem> subscriptions = subscriptionService.list(workerConfig);
        QStringList subscriptionIds;
        QStringList visitedUrls;
        QString lastSubscriptionId;

        for (const QString& line : SubscriptionImportTextParser::nonEmptyLines(text)) {
            if (!SubscriptionImportTextParser::isSubscriptionUrl(line)) {
                continue;
            }

            const QString normalizedUrl = line.trimmed();
            if (normalizedUrl.isEmpty()) {
                continue;
            }

            bool alreadyVisited = false;
            for (const QString& visitedUrl : visitedUrls) {
                if (visitedUrl.compare(normalizedUrl, Qt::CaseInsensitive) == 0) {
                    alreadyVisited = true;
                    break;
                }
            }
            if (alreadyVisited) {
                continue;
            }
            visitedUrls.append(normalizedUrl);

            QString subscriptionId;
            for (SubItem& item : subscriptions) {
                if (item.url.trimmed().compare(normalizedUrl, Qt::CaseInsensitive) != 0) {
                    continue;
                }

                if (item.id.trimmed().isEmpty()) {
                    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                }
                if (item.remarks.trimmed().isEmpty()) {
                    item.remarks = SubscriptionImportTextParser::extractHostOrIpFromUrl(normalizedUrl);
                }
                item.enabled = true;
                subscriptionId = item.id.trimmed();
                break;
            }

            if (subscriptionId.isEmpty()) {
                SubItem item;
                item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                item.remarks = SubscriptionImportTextParser::extractHostOrIpFromUrl(normalizedUrl);
                if (item.remarks.trimmed().isEmpty()) {
                    item.remarks = QStringLiteral("import sub");
                }
                item.url = normalizedUrl;
                item.enabled = true;
                subscriptions.append(item);
                subscriptionId = item.id;
            }

            if (!subscriptionId.isEmpty() && !subscriptionIds.contains(subscriptionId)) {
                subscriptionIds.append(subscriptionId);
                lastSubscriptionId = subscriptionId;
            }
        }

        if (subscriptionIds.isEmpty()) {
            invokeOnUiThread(uiContext, [this, lifetimeGuard]() {
                if (lifetimeGuard.expired()) {
                    return;
                }
                backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::SubscriptionUpdate);
                if (mainWindow_ != nullptr) {
                    mainWindow_->setSubscriptionUpdateRunning(false);
                }
                appendResult(OperationResult::fail(QStringLiteral("No supported share URL or subscription payload was detected.")));
            });
            return;
        }

        const OperationResult saveResult = subscriptionService.saveSubscriptions(workerConfig, subscriptions);
        OperationResult result = saveResult;
        if (saveResult.success) {
            workerConfig.ui().mainSelectedSubId = lastSubscriptionId;
            repository.save(workerConfig);
            const OperationResult updateResult = subscriptionUpdateService.updateByIds(workerConfig, subscriptionIds);

            QStringList lines;
            lines.append(QStringLiteral("Imported %1 subscription URL(s).").arg(subscriptionIds.size()));
            if (!updateResult.message.trimmed().isEmpty()) {
                lines.append(updateResult.message);
            }

            result = updateResult.success
                ? OperationResult::ok(lines.join(QStringLiteral("\n")))
                : OperationResult::fail(lines.join(QStringLiteral("\n")));
        }

        invokeOnUiThread(uiContext, [this, result, subscriptionIds, activeSubscriptionId, lastSubscriptionId, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::SubscriptionUpdate);
            if (mainWindow_ != nullptr) {
                mainWindow_->setSubscriptionUpdateRunning(false);
            }
            if (repository_ != nullptr) {
                config_ = repository_->load();
            }
            appendResult(result);
            syncWindow();
            if (mainWindow_ != nullptr && !lastSubscriptionId.trimmed().isEmpty()) {
                mainWindow_->selectSubscriptionTab(lastSubscriptionId);
            }
            if (result.success
                && !activeSubscriptionId.isEmpty()
                && subscriptionIds.contains(activeSubscriptionId)) {
                restartCoreIfRunning(QStringLiteral("Reloading core after updating subscriptions."), true);
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::exportClientConfig(const QString& indexId)
{
    const VmessItem* server = findServerById(indexId);
    if (server == nullptr || clientConfigWriter_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("The selected server could not be found for client config export.")));
        return;
    }

    const QString exportDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    const QString filePath = DialogUtils::getSaveFileName(
        mainWindow_.get(),
        QStringLiteral("Export Client Config"),
        buildSuggestedExportPath(exportDirectory, *server, QStringLiteral("client")),
        QStringLiteral("Config Files (*.json);;All Files (*.*)"));
    if (filePath.trimmed().isEmpty()) {
        return;
    }

    const OperationResult writeResult = clientConfigWriter_->writeClientConfig(config_, *server, filePath);
    if (!writeResult.success) {
        appendResult(writeResult);
        return;
    }

    appendResult(OperationResult::ok(
        QStringLiteral("Client config exported: %1").arg(QDir::toNativeSeparators(filePath))));
}

void AppBootstrap::exportServerConfig(const QString& indexId)
{
    const VmessItem* server = findServerById(indexId);
    if (server == nullptr || serverConfigWriter_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("The selected server could not be found for server config export.")));
        return;
    }

    const QString exportDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    const QString filePath = DialogUtils::getSaveFileName(
        mainWindow_.get(),
        QStringLiteral("Export Server Config"),
        buildSuggestedExportPath(exportDirectory, *server, QStringLiteral("server")),
        QStringLiteral("Config Files (*.json);;All Files (*.*)"));
    if (filePath.trimmed().isEmpty()) {
        return;
    }

    const OperationResult writeResult = serverConfigWriter_->writeServerConfig(*server, filePath);
    if (!writeResult.success) {
        appendResult(writeResult);
        return;
    }

    appendResult(OperationResult::ok(
        QStringLiteral("Server config exported: %1").arg(QDir::toNativeSeparators(filePath))));
}

void AppBootstrap::importClientConfigFromFile()
{
    const QString filePath = DialogUtils::getOpenFileName(
        mainWindow_.get(),
        QStringLiteral("Import Client Config"),
        QFileInfo(resolveConfigPath()).dir().absolutePath(),
        QStringLiteral("Config Files (*.json *.txt);;All Files (*.*)"));
    if (filePath.trimmed().isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendResult(OperationResult::fail(QStringLiteral("Failed to open the selected client config file.")));
        return;
    }

    VmessItem imported;
    const OperationResult parseResult = ConfigFileImportParser::parseClientConfig(QString::fromUtf8(file.readAll()), &imported);
    if (!parseResult.success) {
        appendResult(parseResult);
        return;
    }

    AddServerDialog dialog(mainWindow_.get());
    dialog.setWindowTitle(QStringLiteral("Import Client Config"));
    dialog.setServer(imported);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    appendResult(serverService_->addServer(config_, dialog.server()));
    syncWindow();
}

void AppBootstrap::importServerConfigFromFile()
{
    const QString filePath = DialogUtils::getOpenFileName(
        mainWindow_.get(),
        QStringLiteral("Import Server Config"),
        QFileInfo(resolveConfigPath()).dir().absolutePath(),
        QStringLiteral("Config Files (*.json *.txt);;All Files (*.*)"));
    if (filePath.trimmed().isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendResult(OperationResult::fail(QStringLiteral("Failed to open the selected server config file.")));
        return;
    }

    VmessItem imported;
    const OperationResult parseResult = ConfigFileImportParser::parseServerConfig(QString::fromUtf8(file.readAll()), &imported);
    if (!parseResult.success) {
        appendResult(parseResult);
        return;
    }

    AddServerDialog dialog(mainWindow_.get());
    dialog.setWindowTitle(QStringLiteral("Import Server Config"));
    dialog.setServer(imported);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    appendResult(serverService_->addServer(config_, dialog.server()));
    syncWindow();
}

void AppBootstrap::updateAllSubscriptions()
{
    if (backgroundTasks_->isKindActive(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)) {
        appendResult(OperationResult::fail(QStringLiteral("A subscription update is already running in the background.")));
        return;
    }

    updateSubscriptionsByIds(
        QStringList(),
        false,
        QCoreApplication::translate("AppBootstrap", "Updating subscriptions in the background..."));
}

void AppBootstrap::updateCurrentSubscription(const QString& subscriptionId)
{
    const QString trimmedSubscriptionId = subscriptionId.trimmed();
    if (trimmedSubscriptionId.isEmpty()) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "Select a specific subscription tab before updating the current subscription.")));
        return;
    }

    QList<int> rowIndexes;
    for (int index = 0; index < config_.collection().subscriptions.size(); ++index) {
        if (config_.collection().subscriptions.at(index).id.trimmed() == trimmedSubscriptionId) {
            rowIndexes.append(index);
        }
    }

    if (rowIndexes.isEmpty()) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "The selected subscription could not be found.")));
        return;
    }

    updateSelectedSubscriptions(rowIndexes);
}

void AppBootstrap::updateCurrentSubscriptionViaProxy(const QString& subscriptionId)
{
    const QString trimmedSubscriptionId = subscriptionId.trimmed();
    if (trimmedSubscriptionId.isEmpty()) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "Select a specific subscription tab before updating the current subscription.")));
        return;
    }

    updateSubscriptionsByIds(
        QStringList{trimmedSubscriptionId},
        true,
        QCoreApplication::translate("AppBootstrap", "Updating the current subscription through the local proxy..."));
}

void AppBootstrap::hideSubscription(const QString& subscriptionId)
{
    if (subscriptionService_ == nullptr) {
        return;
    }

    appendResult(subscriptionService_->setSubscriptionEnabled(config_, subscriptionId, false));
    syncWindow();
}

void AppBootstrap::deleteSubscription(const QString& subscriptionId)
{
    if (subscriptionService_ == nullptr) {
        return;
    }

    const QString normalizedId = subscriptionId.trimmed();
    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    const QString activeSubscriptionId = activeServer.has_value() ? activeServer->subId.trimmed() : QString();
    const bool activeSubscriptionRemoved = !activeSubscriptionId.isEmpty() && activeSubscriptionId == normalizedId;

    const OperationResult result = subscriptionService_->removeSubscription(config_, normalizedId);
    appendResult(result);
    syncWindow();
    if (!result.success || !activeSubscriptionRemoved || !isCoreRunning()) {
        return;
    }

    if (!resolveActiveServerSnapshot().has_value()) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping core because the active subscription was deleted.")));
        stopCore(false);
        return;
    }

    restartCoreIfRunning(QStringLiteral("Reloading core after deleting the active subscription."), true);
}

void AppBootstrap::openCustomConfigFile(const QString& indexId)
{
    if (serverService_ == nullptr) {
        return;
    }

    const VmessItem* existing = findServerById(indexId);
    if (existing == nullptr || existing->configType != ConfigType::Custom) {
        appendResult(OperationResult::fail(QStringLiteral("The selected custom server could not be found.")));
        return;
    }

    const QString filePath = serverService_->resolveCustomConfigPath(existing->address);
    if (filePath.trimmed().isEmpty() || !QFileInfo::exists(filePath)) {
        appendResult(OperationResult::fail(QStringLiteral("Custom config file does not exist.")));
        return;
    }

    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    appendResult(opened
        ? OperationResult::ok(QStringLiteral("Opened custom config: %1").arg(QDir::toNativeSeparators(filePath)))
        : OperationResult::fail(QStringLiteral("Failed to open custom config file.")));
}

void AppBootstrap::handleRoutingSelectionResult(
    const OperationResult& result,
    bool previousAdvancedEnabled,
    int previousRoutingIndex)
{
    appendResult(result);
    syncWindow();

    const RoutingSelectionPlan plan = evaluateRoutingSelectionPlan(
        result.success,
        previousAdvancedEnabled,
        config_.collection().enableRoutingAdvanced,
        previousRoutingIndex,
        config_.collection().routingIndex,
        isCoreRunning());
    if (plan.shouldRestartRunningCore) {
        restartCoreIfRunning(QStringLiteral("Reloading core to apply routing changes."), true);
    }
}

void AppBootstrap::handleDefaultServerSelectionResult(const OperationResult& result, const QString& previousIndexId)
{
    appendResult(result);
    syncWindow();

    const DefaultServerSelectionPlan plan = evaluateDefaultServerSelectionPlan(
        result.success,
        previousIndexId,
        config_.currentIndexId,
        isCoreRunning());
    if (!result.success) {
        return;
    }

    if (plan.shouldClearCurrentServerWarning && mainWindow_ != nullptr) {
        mainWindow_->setCurrentServerWarning({});
    }
    if (plan.shouldMarkCurrentActivationPending) {
        setCurrentActivationPending_ = true;
    }
    if (plan.shouldRestartRunningCore) {
        restartCoreIfRunning(QStringLiteral("Reloading core after switching the default server."), true);
    }
    if (plan.shouldClearCurrentActivationPending) {
        setCurrentActivationPending_ = false;
    }
    if (plan.shouldEnableProxy) {
        enableSystemProxy(true);
    }
}

bool AppBootstrap::requestDefaultServerSwitchAfterCoreStop(const QString& indexId, bool enableTun)
{
    const QString trimmedId = indexId.trimmed();
    if (trimmedId.isEmpty() || !isCoreRunning() || config_.currentIndexId == trimmedId) {
        return false;
    }

    pendingDefaultServerAfterStopId_ = trimmedId;
    pendingDefaultServerAfterStopEnableTun_ = enableTun;
    pendingDefaultServerAfterStopShowStartupOverlay_ = true;
    setCurrentActivationPending_ = true;
    appendResult(OperationResult::ok(QStringLiteral("Stopping current core before switching the default server.")));
    prepareCoreStartupChecklist(true);
    stopCoreInternal(false, false);
    return true;
}

void AppBootstrap::scheduleDefaultServerSwitchAfterCoreStopped()
{
    QPointer<QObject> mainWindowGuard(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QTimer::singleShot(0, mainWindowGuard.isNull() ? QCoreApplication::instance() : mainWindowGuard.data(), [this, lifetimeGuard]() {
        if (lifetimeGuard.expired() || shuttingDown_.load()) {
            return;
        }

        switchDefaultServerAfterCoreStopped();
    });
}

void AppBootstrap::switchDefaultServerAfterCoreStopped()
{
    const QString indexId = pendingDefaultServerAfterStopId_.trimmed();
    const bool enableTun = pendingDefaultServerAfterStopEnableTun_;
    const bool showStartupOverlay = pendingDefaultServerAfterStopShowStartupOverlay_;
    pendingDefaultServerAfterStopId_.clear();
    pendingDefaultServerAfterStopEnableTun_ = false;
    pendingDefaultServerAfterStopShowStartupOverlay_ = false;

    if (indexId.isEmpty() || serverService_ == nullptr || shuttingDown_.load()) {
        setCurrentActivationPending_ = false;
        return;
    }

    const QString previousIndexId = config_.currentIndexId;
    const OperationResult result = serverService_->setDefaultServer(config_, indexId);
    appendResult(result);
    syncWindow();
    if (!result.success) {
        setCurrentActivationPending_ = false;
        return;
    }

    if (mainWindow_ != nullptr) {
        mainWindow_->setCurrentServerWarning({});
    }

    if (enableTun && !config_.tun().tunModeItem.enableTun) {
        setTunEnabled(true);
        return;
    }

    if (previousIndexId != config_.currentIndexId || enableTun) {
        appendResult(OperationResult::ok(QStringLiteral("Starting core after switching the default server.")));
        startCoreInternal(false, true, false, showStartupOverlay);
        return;
    }

    setCurrentActivationPending_ = false;
}

void AppBootstrap::setDefaultServerWithTun(const QString& indexId)
{
    if (serverService_ == nullptr) {
        return;
    }

    if (requestDefaultServerSwitchAfterCoreStop(indexId, true)) {
        return;
    }

    const QString previousIndexId = config_.currentIndexId;
    const OperationResult result = serverService_->setDefaultServer(config_, indexId);
    appendResult(result);
    syncWindow();
    if (!result.success) {
        return;
    }

    if (mainWindow_ != nullptr) {
        mainWindow_->setCurrentServerWarning({});
    }
    setCurrentActivationPending_ = true;

    if (config_.tun().tunModeItem.enableTun) {
        if (isCoreRunning() && previousIndexId != config_.currentIndexId) {
            restartCoreIfRunning(QStringLiteral("Reloading core after switching the default server."), true);
            return;
        }
        enableSystemProxy(true);
        return;
    }

    setTunEnabled(true);
}

void AppBootstrap::runProxyAvailabilityCheck()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    if (!backgroundTasks_->tryBegin(BackgroundTaskCoordinator::Kind::ProxyAvailabilityCheck)) {
        return;
    }

    const ProxyAvailabilityCheckConfig workerConfig{
        config_.localPort,
        config_.tun().tunModeItem.enableTun,
        config_.defaults().speedPingTestUrl};
    QObject* uiContext = mainWindow_.get();
    mainWindow_->showTransientStatus(
        QCoreApplication::translate("AppBootstrap", "Running availability check in the background..."),
        3000,
        MainWindow::TransientStatusPriority::Routine);

    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, workerConfig, uiContext, lifetimeGuard]() {
        ProxyAvailabilityCheckService proxyAvailabilityCheckService;
        const OperationResult result = proxyAvailabilityCheckService.check(workerConfig);

        invokeOnUiThread(uiContext, [this, result, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::ProxyAvailabilityCheck);
            showOperationMessage(
                QCoreApplication::translate("AppBootstrap", "TestMe"),
                result,
                mainWindow_.get());
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::updateCore(int coreTypeValue)
{
    updateCore(coreTypeValue, false);
}

void AppBootstrap::updateCore(int coreTypeValue, bool startAfterSuccess)
{
    updateCore(coreTypeValue, startAfterSuccess, mainWindow_.get(), mainWindow_.get(), false, false, {}, {});
}

void AppBootstrap::updateCore(
    int coreTypeValue,
    bool startAfterSuccess,
    QObject* progressContext,
    QWidget* dialogParent,
    bool skipConfirmation,
    bool skipLocalVersionCheck,
    const std::function<void(const QString&)>& progressObserver,
    const std::function<void(const OperationResult&)>& completionObserver)
{
    if (mainWindow_ == nullptr) {
        return;
    }

    const auto notifyCompletion = [&completionObserver](const OperationResult& result) {
        if (completionObserver) {
            completionObserver(result);
        }
    };

    if (!backgroundTasks_->tryBegin(BackgroundTaskCoordinator::Kind::CoreUpdate)) {
        return;
    }

    const CoreType coreType = resolveRuntimeCoreType(static_cast<CoreType>(coreTypeValue));
    const QString title = QCoreApplication::translate("AppBootstrap", "Install / Update %1 Core")
                              .arg(coreTypeDisplayName(coreType));
    const QString installDirectory = resolveCoreInstallDirectory(coreType);
    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    const bool shouldRestartRunningCore = isCoreRunning()
        && activeServer.has_value()
        && resolveRuntimeCoreType(activeServer->coreType) == coreType;
    const CoreUpdateConfig workerConfig{
        config_.checkPreReleaseUpdate,
        config_.ignoreGeoUpdateCore};
    QPointer<QObject> progressContextGuard(progressContext);
    QPointer<QWidget> dialogParentGuard(dialogParent);
    bool stoppedForInstall = false;

    QWidget* messageParent = dialogParentGuard.isNull() ? mainWindow_.get() : dialogParentGuard.data();
    const QString prompt = QCoreApplication::translate("CoreUpdateService", "Install / Update %1?\nThe running core will be stopped before installation if needed.")
                               .arg(coreTypeDisplayName(coreType));
    const bool confirmed = skipConfirmation || (messageParent != nullptr
        && DialogUtils::askYesNoQuestion(
               messageParent,
               title,
               prompt,
               QMessageBox::Yes)
            == QMessageBox::Yes);
    if (!confirmed) {
        backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::CoreUpdate);
        const OperationResult result = OperationResult::ok(
            QCoreApplication::translate("AppBootstrap", "%1 update was canceled.")
                .arg(coreTypeDisplayName(coreType)));
        appendResult(result);
        notifyCompletion(result);
        return;
    }

    if (shouldRestartRunningCore) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Stopping the running core before installing the update.");
        appendResult(OperationResult::ok(message));
        coreUpdatePendingAfterStop_ = true;
        pendingCoreUpdateType_ = coreType;
        pendingCoreUpdateStartAfterSuccess_ = startAfterSuccess;
        pendingCoreUpdateSkipLocalVersionCheck_ = skipLocalVersionCheck;
        pendingCoreUpdateProgressContext_ = progressContextGuard;
        pendingCoreUpdateDialogParent_ = dialogParentGuard;
        pendingCoreUpdateProgressObserver_ = progressObserver;
        pendingCoreUpdateCompletionObserver_ = completionObserver;
        stopCoreInternal(false, false);
        return;
    }

    const QString startupMessage = QCoreApplication::translate("AppBootstrap", "Starting %1 update...")
                                       .arg(coreTypeDisplayName(coreType));
    mainWindow_->appendLog(startupMessage);
    mainWindow_->showTransientStatus(
        startupMessage,
        3000,
        MainWindow::TransientStatusPriority::Routine);

    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this,
                                       coreType,
                                       workerConfig,
                                       installDirectory,
                                       title,
                                       stoppedForInstall,
                                       startAfterSuccess,
                                       skipLocalVersionCheck,
                                       progressContextGuard,
                                       dialogParentGuard,
                                       progressObserver,
                                       completionObserver,
                                       lifetimeGuard]() {
        runCoreUpdateTask(
            coreType,
            workerConfig,
            installDirectory,
            progressContextGuard,
            skipLocalVersionCheck,
            progressObserver,
            [this, title, stoppedForInstall, startAfterSuccess, dialogParentGuard, completionObserver, lifetimeGuard](const OperationResult& result) {
                if (lifetimeGuard.expired()) {
                    return;
                }
                finalizeCoreUpdate(
                    title,
                    result,
                    stoppedForInstall,
                    startAfterSuccess,
                    dialogParentGuard,
                    completionObserver);
            });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::runCoreUpdateTask(
    CoreType coreType,
    CoreUpdateConfig workerConfig,
    QString installDirectory,
    QPointer<QObject> progressContextGuard,
    bool skipLocalVersionCheck,
    std::function<void(const QString&)> progressObserver,
    std::function<void(const OperationResult&)> completionObserver)
{
    CoreUpdateService coreUpdateService;
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;

    const auto reportProgress = [this, progressContextGuard, progressObserver, lifetimeGuard](const QString& message) {
        if (message.trimmed().isEmpty()) {
            return;
        }

        QObject* uiTarget = progressContextGuard.isNull() ? mainWindow_.get() : progressContextGuard.data();
        invokeOnUiThread(uiTarget, [this, message, progressObserver, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (mainWindow_ == nullptr) {
                return;
            }

            mainWindow_->appendLog(message);
            mainWindow_->showTransientStatus(
                message,
                0,
                MainWindow::TransientStatusPriority::Routine);
            if (progressObserver) {
                progressObserver(message);
            }
        });
    };

    CoreUpdateService::UpdateOptions updateOptions;
    updateOptions.progressHandler = reportProgress;
    updateOptions.cancelCheck = [this]() {
        QThread* currentThread = QThread::currentThread();
        return shuttingDown_.load()
            || (currentThread != nullptr && currentThread->isInterruptionRequested());
    };
    updateOptions.skipLocalVersionCheck = skipLocalVersionCheck;

    const OperationResult result = coreUpdateService.update(
        coreType,
        workerConfig,
        installDirectory,
        updateOptions);

    QObject* uiTarget = progressContextGuard.isNull() ? mainWindow_.get() : progressContextGuard.data();
    invokeOnUiThread(uiTarget, [completionObserver, result, lifetimeGuard]() {
        if (lifetimeGuard.expired()) {
            return;
        }
        if (completionObserver) {
            completionObserver(result);
        }
    });
}

void AppBootstrap::finalizeCoreUpdate(
    const QString& title,
    const OperationResult& result,
    bool stoppedForInstall,
    bool startAfterSuccess,
    QPointer<QWidget> dialogParentGuard,
    const std::function<void(const OperationResult&)>& completionObserver)
{
    backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::CoreUpdate);
    if (shuttingDown_.load()) {
        return;
    }

    if (mainWindow_ != nullptr && !result.message.trimmed().isEmpty()) {
        mainWindow_->showTransientStatus(result.message, 5000);
    }
    if (result.success) {
        refreshExistingCoreTypes();
    }
    if (completionObserver) {
        completionObserver(result);
    }

    if (stoppedForInstall) {
        if (result.success && result.requiresRestart && startAfterSuccess) {
            const QString message = QCoreApplication::translate(
                "AppBootstrap",
                "Restarting the updated core...");
            if (mainWindow_ != nullptr) {
                mainWindow_->appendLog(message);
                mainWindow_->showTransientStatus(
                    message,
                    0,
                    MainWindow::TransientStatusPriority::Routine);
            }
            enableSystemProxy(true);
        } else if (!result.success) {
            const QString message = QCoreApplication::translate(
                "AppBootstrap",
                "Update failed. Restoring the previous running core...");
            if (mainWindow_ != nullptr) {
                mainWindow_->appendLog(message);
                mainWindow_->showTransientStatus(message, 0);
            }
            enableSystemProxy(false);
        } else {
            clearProxyStateAfterCoreStopped();
            syncStatusIndicators();
        }
    }

    if (!stoppedForInstall && startAfterSuccess && result.success) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Starting the downloaded core...");
        if (mainWindow_ != nullptr) {
            mainWindow_->appendLog(message);
            mainWindow_->showTransientStatus(
                message,
                0,
                MainWindow::TransientStatusPriority::Routine);
        }
        enableSystemProxy(true);
    }

    if (mainWindow_ == nullptr || result.message.trimmed().isEmpty()) {
        return;
    }

    QWidget* messageParent = dialogParentGuard.isNull() ? mainWindow_.get() : dialogParentGuard.data();
    showOperationMessage(title, result, messageParent);
}

void AppBootstrap::continuePendingCoreUpdate()
{
    if (!coreUpdatePendingAfterStop_ || mainWindow_ == nullptr) {
        return;
    }

    const CoreType coreType = pendingCoreUpdateType_;
    const bool startAfterSuccess = pendingCoreUpdateStartAfterSuccess_;
    const bool skipLocalVersionCheck = pendingCoreUpdateSkipLocalVersionCheck_;
    QPointer<QObject> progressContextGuard = pendingCoreUpdateProgressContext_;
    QPointer<QWidget> dialogParentGuard = pendingCoreUpdateDialogParent_;
    std::function<void(const QString&)> progressObserver = pendingCoreUpdateProgressObserver_;
    std::function<void(const OperationResult&)> completionObserver = pendingCoreUpdateCompletionObserver_;

    coreUpdatePendingAfterStop_ = false;
    pendingCoreUpdateType_ = CoreType::Unknown;
    pendingCoreUpdateStartAfterSuccess_ = false;
    pendingCoreUpdateSkipLocalVersionCheck_ = false;
    pendingCoreUpdateProgressContext_.clear();
    pendingCoreUpdateDialogParent_.clear();
    pendingCoreUpdateProgressObserver_ = {};
    pendingCoreUpdateCompletionObserver_ = {};

    const QString title = QCoreApplication::translate("AppBootstrap", "Install / Update %1 Core")
                              .arg(coreTypeDisplayName(coreType));
    const QString installDirectory = resolveCoreInstallDirectory(coreType);
    const CoreUpdateConfig workerConfig{
        config_.checkPreReleaseUpdate,
        config_.ignoreGeoUpdateCore};
    const QString startupMessage = QCoreApplication::translate("AppBootstrap", "Starting %1 update...")
                                       .arg(coreTypeDisplayName(coreType));
    mainWindow_->appendLog(startupMessage);
    mainWindow_->showTransientStatus(
        startupMessage,
        3000,
        MainWindow::TransientStatusPriority::Routine);

    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this,
                                       coreType,
                                       workerConfig,
                                       installDirectory,
                                       title,
                                       startAfterSuccess,
                                       skipLocalVersionCheck,
                                       progressContextGuard,
                                       dialogParentGuard,
                                       progressObserver,
                                       completionObserver,
                                       lifetimeGuard]() {
        runCoreUpdateTask(
            coreType,
            workerConfig,
            installDirectory,
            progressContextGuard,
            skipLocalVersionCheck,
            progressObserver,
            [this, title, startAfterSuccess, dialogParentGuard, completionObserver, lifetimeGuard](const OperationResult& result) {
                if (lifetimeGuard.expired()) {
                    return;
                }
                finalizeCoreUpdate(
                    title,
                    result,
                    true,
                    startAfterSuccess,
                    dialogParentGuard,
                    completionObserver);
            });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::updateGeoResources()
{
    if (geoResourceUpdateService_ == nullptr || mainWindow_ == nullptr) {
        return;
    }

    if (!backgroundTasks_->tryBegin(BackgroundTaskCoordinator::Kind::GeoUpdate)) {
        return;
    }

    const QString targetDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    QObject* uiContext = mainWindow_.get();
    const QString title = QCoreApplication::translate("AppBootstrap", "Update Geo Files");
    const QString message = QCoreApplication::translate("AppBootstrap", "Updating Geo resources in the background...");
    mainWindow_->appendLog(message);
    mainWindow_->showTransientStatus(
        message,
        3000,
        MainWindow::TransientStatusPriority::Routine);

    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, targetDirectory, title, uiContext, lifetimeGuard]() {
        GeoResourceUpdateService geoResourceUpdateService(targetDirectory);
        const QList<OperationResult> results{
            geoResourceUpdateService.update(QStringLiteral("geosite")),
            geoResourceUpdateService.update(QStringLiteral("geoip"))};

        bool hasSuccess = false;
        QStringList failureLines;
        QStringList successLines;
        for (const OperationResult& result : results) {
            if (result.success) {
                hasSuccess = true;
                if (!result.message.trimmed().isEmpty()) {
                    successLines.append(result.message);
                }
            } else if (!result.message.trimmed().isEmpty()) {
                failureLines.append(result.message);
            }
        }

        invokeOnUiThread(uiContext, [this, title, results, hasSuccess, failureLines, successLines, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::GeoUpdate);
            for (const OperationResult& result : results) {
                appendResult(result);
            }

            if (mainWindow_ != nullptr) {
                if (!failureLines.isEmpty()) {
                    DialogUtils::showWarning(mainWindow_.get(), title, failureLines.join(QChar('\n')));
                } else if (!successLines.isEmpty()) {
                    DialogUtils::showInformation(mainWindow_.get(), title, successLines.join(QChar('\n')));
                }
            }

            if (hasSuccess) {
                restartCoreIfRunning(QCoreApplication::translate(
                    "AppBootstrap",
                    "Reloading core after updating Geo resources."));
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::updateSelectedSubscriptions(const QList<int>& rowIndexes)
{
    QStringList subscriptionIds;
    for (int rowIndex : rowIndexes) {
        if (rowIndex < 0 || rowIndex >= config_.collection().subscriptions.size()) {
            continue;
        }

        const QString subscriptionId = config_.collection().subscriptions.at(rowIndex).id.trimmed();
        if (!subscriptionId.isEmpty() && !subscriptionIds.contains(subscriptionId)) {
            subscriptionIds.append(subscriptionId);
        }
    }

    updateSubscriptionsByIds(
        subscriptionIds,
        false,
        QCoreApplication::translate("AppBootstrap", "Updating selected subscriptions in the background..."));
}

void AppBootstrap::updateSubscriptionsByIds(
    const QStringList& subscriptionIds,
    bool useProxy,
    const QString& startupMessage)
{
    if (!backgroundTasks_->tryBegin(BackgroundTaskCoordinator::Kind::SubscriptionUpdate)) {
        return;
    }

    const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
    const QString activeSubscriptionId = activeServer.has_value() ? activeServer->subId.trimmed() : QString();
    if (mainWindow_ != nullptr) {
        mainWindow_->appendLog(startupMessage);
        mainWindow_->showTransientStatus(
            startupMessage,
            3000,
            MainWindow::TransientStatusPriority::Routine);
    }

    // Clear test results as loading indicator.
    for (auto& server : config_.collection().servers) {
        server.testResult = QStringLiteral("...");
    }
    syncWindow();

    if (mainWindow_ != nullptr) {
        mainWindow_->setSubscriptionUpdateRunning(true);
    }
    const QString configPath = resolveConfigPath();
    QObject* uiContext = mainWindow_.get();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, configPath, subscriptionIds, activeSubscriptionId, uiContext, useProxy, lifetimeGuard]() {
        JsonConfigRepository repository(configPath);
        SubscriptionService subscriptionService(repository);
        QNetworkAccessManager networkAccessManager;
        SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);
        Config workerConfig = repository.load();
        const OperationResult result = subscriptionIds.isEmpty()
            ? subscriptionUpdateService.updateAll(workerConfig, useProxy)
            : subscriptionUpdateService.updateByIds(workerConfig, subscriptionIds, useProxy);

        invokeOnUiThread(uiContext, [this, result, subscriptionIds, activeSubscriptionId, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::SubscriptionUpdate);
            if (mainWindow_ != nullptr) {
                mainWindow_->setSubscriptionUpdateRunning(false);
            }
            if (repository_ != nullptr) {
                config_ = repository_->load();
            }
            appendResult(result);
            syncWindow();
            if (result.success
                && !activeSubscriptionId.isEmpty()
                && (subscriptionIds.isEmpty() || subscriptionIds.contains(activeSubscriptionId))) {
                restartCoreIfRunning(QStringLiteral("Reloading core after updating subscriptions."), true);
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
}

void AppBootstrap::autoBackupCurrentConfig()
{
    if (configBackupService_ == nullptr) {
        return;
    }

    const OperationResult result = configBackupService_->backupCurrentConfig(config_);
    if (!result.success) {
        appendResult(result);
    }
}

void AppBootstrap::restoreConfigFromBackup()
{
    if (mainWindow_ == nullptr || configBackupService_ == nullptr) {
        return;
    }

    QString initialDirectory = configBackupService_->backupDirectoryPath();
    if (initialDirectory.trimmed().isEmpty() || !QDir(initialDirectory).exists()) {
        initialDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    }

    const QString backupPath = DialogUtils::getOpenFileName(
        mainWindow_.get(),
        QCoreApplication::translate("AppBootstrap", "Restore GUI Config"),
        initialDirectory,
        QCoreApplication::translate("AppBootstrap", "Config Files (*.json);;All Files (*.*)"));
    if (backupPath.trimmed().isEmpty()) {
        return;
    }

    JsonConfigRepository validationRepository(backupPath);
    validationRepository.load();
    const QString validationError = validationRepository.lastLoadError().trimmed();
    if (!validationError.isEmpty()) {
        appendResult(OperationResult::fail(validationError));
        DialogUtils::showWarning(
            mainWindow_.get(),
            QCoreApplication::translate("AppBootstrap", "Restore GUI Config"),
            validationError);
        return;
    }

    const bool wasCoreRunning = isCoreRunning();
    if (wasCoreRunning) {
        stopCoreInternal(true, false);
    }

    const OperationResult backupResult = configBackupService_->backupCurrentConfig(config_);
    if (!backupResult.success) {
        appendResult(backupResult);
        return;
    }

    const OperationResult restoreResult = configBackupService_->restoreFromPath(backupPath);
    if (!restoreResult.success) {
        appendResult(restoreResult);
        return;
    }

    uiStateRestored_ = false;
    if (!reloadConfig()) {
        return;
    }

    if (wasCoreRunning && resolveActiveServer() != nullptr) {
        enableSystemProxy(true);
    } else {
        clearProxyStateAfterCoreStopped();
        syncStatusIndicators();
    }

    appendResult(OperationResult::ok(
        QCoreApplication::translate("AppBootstrap", "Configuration restored from backup.")));
}

void AppBootstrap::openSettingsDialog(int initialTab)
{
    if (mainWindow_ == nullptr || serverService_ == nullptr) {
        return;
    }

    refreshExistingCoreTypes();
    SettingsDialogSession session(
        mainWindow_.get(),
        [this](QThread* thread) { trackBackgroundThread(thread); },
        [this](CoreType coreType) { return detectCoreVersion(coreType); },
        [this](CoreType requestedCoreType, QPointer<SettingsDialog> dialogGuard) {
            const auto updateStarted = std::make_shared<bool>(false);
            updateCore(
                static_cast<int>(requestedCoreType),
                false,
                dialogGuard.data(),
                dialogGuard.data(),
                false,
                false,
                [dialogGuard, requestedCoreType, updateStarted](const QString& message) {
                    if (!dialogGuard.isNull()) {
                        if (!*updateStarted) {
                            dialogGuard->beginCoreUpdate(requestedCoreType);
                            *updateStarted = true;
                        }
                        dialogGuard->setCoreUpdateProgress(requestedCoreType, message);
                    }
                },
                [this, dialogGuard, requestedCoreType](const OperationResult& result) {
                    if (dialogGuard.isNull()) {
                        return;
                    }

                    if (result.success) {
                        dialogGuard->setExistingCoreTypes(existingCoreTypes_);
                        dialogGuard->setCoreVersion(requestedCoreType, detectCoreVersion(requestedCoreType));
                    }
                    dialogGuard->finishCoreUpdate(requestedCoreType, result.success, result.message);
                });
        });

    const SettingsDialogSession::Result sessionResult = session.exec(
        config_,
        initialTab,
        existingCoreTypes_,
        lifetimeGuard_);
    if (sessionResult.outcome == SettingsDialogSession::Outcome::Cancelled) {
        return;
    }

    if (sessionResult.outcome == SettingsDialogSession::Outcome::RestoreBackup) {
        restoreConfigFromBackup();
        return;
    }

    if (sessionResult.outcome == SettingsDialogSession::Outcome::UpdateSubscriptions) {
        const OperationResult saveResult =
            subscriptionService_->saveSubscriptions(config_, sessionResult.config.collection().subscriptions);
        appendResult(saveResult);
        if (saveResult.success) {
            syncWindow();
            updateSelectedSubscriptions(sessionResult.selectedSubscriptionRows);
        }
        return;
    }

    applySettingsDialogResult(sessionResult.config);
}

void AppBootstrap::applySettingsDialogResult(const Config& updatedConfig)
{
    const Config previousConfig = config_;

    QSet<QString> previousSubIds;
    for (const SubItem& item : previousConfig.collection().subscriptions) {
        const QString id = item.id.trimmed();
        if (!id.isEmpty()) {
            previousSubIds.insert(id);
        }
    }

    const SettingsDialogApplyPlan plan = evaluateSettingsDialogApplyPlan(
        previousConfig,
        updatedConfig,
        isWindowsPlatform(),
        isProcessElevated(),
        isCoreRunning());

    config_ = plan.tunSaveBehavior.configToPersist;
    SubscriptionService::normalizeSubscriptionIds(config_.collection().subscriptions);

    QStringList newSubIds;
    for (const SubItem& item : config_.collection().subscriptions) {
        if (item.enabled) {
            const QString id = item.id.trimmed();
            if (!id.isEmpty() && !previousSubIds.contains(id)) {
                newSubIds.append(id);
            }
        }
    }
    if (!serverService_->save(config_)) {
        config_ = previousConfig;
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to save settings.")));
        syncWindow();
        return;
    }

    appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Settings saved.")));
    if (plan.tunSaveBehavior.shouldPromptForAdminRestart) {
        appendResult(OperationResult::fail(tunAdminRequiredSaveMessage()));
    }
    syncWindow();
    if (mainWindow_ != nullptr && !newSubIds.isEmpty()) {
        mainWindow_->selectSubscriptionTab(newSubIds.constLast());
    }

    if (!newSubIds.isEmpty()) {
        updateSubscriptionsByIds(
            newSubIds,
            false,
            QCoreApplication::translate("AppBootstrap", "Updating new subscriptions in the background..."));
    }

    if (plan.autoRunChanged && autoRunService_ != nullptr) {
        const bool success = autoRunService_->setEnabled(updatedConfig.ui().autoRunEnabled);
        if (!success) {
            appendResult(OperationResult::fail(QStringLiteral("Failed to update auto run setting.")));
        }
    }

    if (plan.systemProxySettingsChanged
        && systemProxyService_ != nullptr
        && isCoreReady()) {
        const bool proxyUpdated = updateSystemProxyMode(normalizeSystemProxyMode(config_.sysProxyType));
        if (!proxyUpdated) {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Failed to reapply system proxy settings.")));
        } else {
            managedSystemProxyActive_ = expectedSystemProxyEnabled(normalizeSystemProxyMode(config_.sysProxyType));
        }
    }

    if (plan.runtimeSettingsChanged) {
        restartCoreIfRunning(QCoreApplication::translate(
            "AppBootstrap",
            "Reloading core after applying settings changes."),
            true);
    } else {
        syncStatusIndicators();
    }

    const bool adminRestartInitiated =
        plan.tunSaveBehavior.shouldPromptForAdminRestart && promptRestartAsAdministratorForTun();

    if (shouldPromptForLanguageRestartAfterSettingsSave(plan.languageChanged, adminRestartInitiated)) {
        promptRestartForLanguageChange();
    }
}

void AppBootstrap::openAboutDialog()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    AboutDialog dialog(mainWindow_.get());
    dialog.setRepoUrl(QString::fromUtf8(ProjectPageUrl));
    dialog.setVersion(QCoreApplication::applicationVersion());
    dialog.setReleaseDate(QString::fromLatin1(AppReleaseDate));
    dialog.setConfigPath(QDir::toNativeSeparators(resolveConfigPath()));
    dialog.setProxyActive(isCoreReady());
    dialog.exec();
}

void AppBootstrap::openExternalUrl(const QString& url)
{
    if (url.trimmed().isEmpty()) {
        return;
    }

    if (!QDesktopServices::openUrl(QUrl(url))) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to open the requested link.")));
    }
}

void AppBootstrap::openLoopbackTool()
{
    const QString loopbackToolPath = locateFirstExistingFile({
        QDir::current().filePath(QStringLiteral("EnableLoopback.exe")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("EnableLoopback.exe"))
    });
    if (loopbackToolPath.trimmed().isEmpty()) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "EnableLoopback.exe was not found.")));
        return;
    }

    if (!QProcess::startDetached(loopbackToolPath, QStringList())) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to launch EnableLoopback.exe.")));
    }
}

bool AppBootstrap::askRestartAsAdministratorForTun() const
{
    if (mainWindow_ == nullptr || !isWindowsPlatform() || isProcessElevated()) {
        return false;
    }

    return DialogUtils::askYesNoQuestion(
            mainWindow_.get(),
            tunAdminRestartPromptTitle(),
            tunAdminRestartPromptMessage(),
            QMessageBox::Yes)
        == QMessageBox::Yes;
}

bool AppBootstrap::promptRestartAsAdministratorForTun()
{
    if (!askRestartAsAdministratorForTun()) {
        return false;
    }

    persistUiState();
    if (!restartApplication(true)) {
        appendResult(OperationResult::fail(tunAdminRestartFailureMessage()));
        return false;
    }

    return true;
}

void AppBootstrap::promptRestartForLanguageChange()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    const auto title = QCoreApplication::translate("AppBootstrap", "Restart Required");
    const auto message =
        QCoreApplication::translate("AppBootstrap", "Language changes take effect after restart. Restart now?");
    if (DialogUtils::askYesNoQuestion(
            mainWindow_.get(),
            title,
            message,
            QMessageBox::Yes)
        != QMessageBox::Yes) {
        return;
    }

    persistUiState();
    const bool requiresAdminRestart =
        isWindowsPlatform() && !isProcessElevated() && config_.tun().tunModeItem.enableTun;
    if (!restartApplication(requiresAdminRestart)) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to restart the application.")));
    }
}

bool AppBootstrap::restartApplication(bool requireAdministrator)
{
    const QStringList arguments = startupRelaunchArgumentsForRunningInstance(
        QCoreApplication::arguments(),
        requireAdministrator,
        QCoreApplication::applicationPid());

    SingleInstanceBootstrap::releaseCurrentInstance();

    bool restarted = false;
    if (requireAdministrator) {
        restarted = restartProcessAsAdministrator(QCoreApplication::applicationFilePath(), arguments);
    } else {
        restarted = QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
    }

    if (!restarted) {
        SingleInstanceBootstrap::reacquireCurrentInstance();
        return false;
    }

    shutdownUiStatePersisted_ = true;
    cleanupRuntimeForExit(false);
    if (mainWindow_ != nullptr) {
        mainWindow_->setAllowClose(true);
    }
    QCoreApplication::quit();
    return true;
}

void AppBootstrap::startSpeedTest(const QStringList& indexIds)
{
    if (speedTestService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("Speed test service is unavailable.")));
        return;
    }

    backgroundTasks_->resetSpeedTestProgress();
    if (!backgroundTasks_->tryBegin(BackgroundTaskCoordinator::Kind::SpeedTest)) {
        return;
    }

    QStringList uniqueIds;
    for (const QString& indexId : indexIds) {
        const QString trimmed = indexId.trimmed();
        if (!trimmed.isEmpty() && !uniqueIds.contains(trimmed)) {
            uniqueIds.append(trimmed);
        }
    }

    QList<SpeedTestRequestItem> items;
    items.reserve(uniqueIds.size());
    for (const QString& indexId : uniqueIds) {
        const VmessItem* server = findServerById(indexId);
        if (server == nullptr) {
            continue;
        }

        const CoreType launchCore = resolveLaunchCoreType(*server);
        const VmessItem runtimeServer = runtimeServerForLaunchCore(*server, launchCore);

        items.append(SpeedTestRequestItem{
            *server,
            runtimeServer,
            resolveCoreInfo(runtimeServer)});
    }

    const OperationResult startResult = speedTestService_->start(config_, items);
    if (!startResult.success) {
        appendResult(startResult);
    }

    if (!startResult.success) {
        backgroundTasks_->resetSpeedTestProgress();
        backgroundTasks_->finish(BackgroundTaskCoordinator::Kind::SpeedTest);
    }

    if (startResult.success) {
        backgroundTasks_->setSpeedTestTotalCount(items.size());
        backgroundTasks_->syncState();
        static const QString pending = QStringLiteral("...");
        QStringList pendingIds;
        for (const auto& item : items) {
            auto it = std::find_if(config_.collection().servers.begin(), config_.collection().servers.end(),
                [&item](const VmessItem& s) { return s.indexId == item.server.indexId; });
            if (it != config_.collection().servers.end()) {
                it->testResult = pending;
                pendingIds.append(it->indexId);
            }
        }
        if (mainWindow_ != nullptr) {
            mainWindow_->updateServerTestResults(pendingIds, pending);
        }
        if (trayController_ != nullptr) {
            trayController_->setServers(
                config_.collection().servers,
                config_.currentIndexId);
        }
    }
}

void AppBootstrap::trackBackgroundThread(QThread* thread)
{
    if (thread == nullptr) {
        return;
    }

    for (int index = backgroundThreads_.size() - 1; index >= 0; --index) {
        const QPointer<QThread>& trackedThread = backgroundThreads_.at(index);
        if (trackedThread.isNull() || !trackedThread->isRunning()) {
            backgroundThreads_.removeAt(index);
        }
    }

    backgroundThreads_.append(thread);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
}

void AppBootstrap::waitForBackgroundThreads()
{
    // Workers cooperatively check QThread::isInterruptionRequested() at their
    // iteration boundaries (subscription update, geo update) or via service-level
    // cancel flags. Keep waiting during destruction: these workers capture
    // AppBootstrap members, so continuing teardown while one is still running
    // would leave a use-after-free behind.
    constexpr unsigned long kShutdownWaitMs = 15000;
    const QList<QPointer<QThread>> threads = backgroundThreads_;
    for (const QPointer<QThread>& threadGuard : threads) {
        QThread* thread = threadGuard.data();
        if (thread == nullptr) {
            continue;
        }

        thread->requestInterruption();
        while (!thread->wait(kShutdownWaitMs)) {
            qWarning("AppBootstrap: background thread did not honor interruption within %lums; still waiting",
                kShutdownWaitMs);
            thread->requestInterruption();
        }
    }
    backgroundThreads_.clear();
}

bool AppBootstrap::isCoreRunning() const
{
    return coreLifecycleService_ != nullptr && coreLifecycleService_->isRunning();
}

bool AppBootstrap::isCoreReady() const
{
    return isCoreRunning() && coreReady_;
}

QString AppBootstrap::resolveConfigPath() const
{
    return configPath_;
}

const VmessItem* AppBootstrap::resolveActiveServer() const
{
    const QString currentIndexId = config_.currentIndexId.trimmed();
    if (currentIndexId.isEmpty()) {
        return nullptr;
    }

    for (const VmessItem& item : config_.collection().servers) {
        if (item.indexId == currentIndexId) {
            return &item;
        }
    }

    return nullptr;
}

std::optional<VmessItem> AppBootstrap::resolveActiveServerSnapshot() const
{
    const VmessItem* server = resolveActiveServer();
    if (server == nullptr) {
        return std::nullopt;
    }

    return *server;
}

const VmessItem* AppBootstrap::findServerById(const QString& indexId) const
{
    if (indexId.trimmed().isEmpty()) {
        return nullptr;
    }

    for (const VmessItem& item : config_.collection().servers) {
        if (item.indexId == indexId) {
            return &item;
        }
    }

    return nullptr;
}

std::optional<VmessItem> AppBootstrap::findServerSnapshotById(const QString& indexId) const
{
    const VmessItem* server = findServerById(indexId);
    if (server == nullptr) {
        return std::nullopt;
    }

    return *server;
}

QString AppBootstrap::resolveCustomConfigDirectory() const
{
    return QFileInfo(resolveConfigPath()).dir().filePath(QStringLiteral("guiConfigs"));
}

QString AppBootstrap::resolveCustomConfigPath(const QString& address) const
{
    const QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (QFileInfo::exists(trimmed)) {
        return QFileInfo(trimmed).absoluteFilePath();
    }

    const QString managedPath = QDir(resolveCustomConfigDirectory()).filePath(trimmed);
    if (QFileInfo::exists(managedPath)) {
        return QFileInfo(managedPath).absoluteFilePath();
    }

    return trimmed;
}

QString AppBootstrap::resolveRuntimeConfigPath(const VmessItem& server) const
{
    QString runtimeDirectory = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("runtime"));

    if (!QDir().mkpath(runtimeDirectory)) {
        return {};
    }

    QString extension = QStringLiteral("json");
    if (server.configType == ConfigType::Custom) {
        const QString sourcePath = resolveCustomConfigPath(server.address);
        const QString sourceExtension = QFileInfo(sourcePath).suffix().trimmed();
        if (!sourceExtension.isEmpty()) {
            extension = sourceExtension;
        }
    }
    const CoreType runtimeCore = resolveEffectiveCoreType(server);
    return QDir(runtimeDirectory).filePath(QStringLiteral("config.generated.%1").arg(extension));
}

QString AppBootstrap::buildRoutingSummaryText() const
{
    if (!config_.collection().enableRoutingAdvanced || config_.collection().routingItems.isEmpty()) {
        return QCoreApplication::translate("AppBootstrap", "Basic");
    }

    if (config_.collection().routingIndex >= 0
        && config_.collection().routingIndex < config_.collection().routingItems.size()) {
        const QString remarks = config_.collection().routingItems.at(config_.collection().routingIndex).remarks.trimmed();
        if (!remarks.isEmpty()) {
            return QCoreApplication::translate("AppBootstrap", "%1 (Advanced)").arg(remarks);
        }

        return QCoreApplication::translate("AppBootstrap", "Routing %1 (Advanced)")
            .arg(config_.collection().routingIndex + 1);
    }

    return QCoreApplication::translate("AppBootstrap", "Advanced");
}

QString AppBootstrap::buildListenSummaryText() const
{
    if (config_.localPort <= 0) {
        return QCoreApplication::translate("AppBootstrap", "Unavailable");
    }

    return QCoreApplication::translate("AppBootstrap", "SOCKS %1 | HTTP %2")
        .arg(config_.localPort)
        .arg(config_.localPort + 1);
}

QString AppBootstrap::buildSystemProxyExceptions() const
{
    QStringList entries = splitProxyExceptions(
        config_.defaults().defIeProxyExceptions.trimmed().isEmpty()
            ? QString::fromUtf8(DefaultIeProxyExceptions)
            : config_.defaults().defIeProxyExceptions.trimmed());

    for (const QString& item : collectRouteDerivedProxyExceptions(config_)) {
        appendUniqueProxyException(entries, item);
    }

    return entries.join(QStringLiteral(";"));
}

bool AppBootstrap::updateSystemProxyMode(SystemProxyMode mode) const
{
    return systemProxyService_ == nullptr
        || systemProxyService_->update(
            mode,
            config_.localPort + 1,
            config_.localPort,
            buildSystemProxyExceptions(),
            config_.systemProxyAdvancedProtocol);
}

CoreType AppBootstrap::resolveEffectiveCoreType(const VmessItem& server) const
{
    return resolveSelectedCoreType(config_, server, existingCoreTypes_);
}

CoreType AppBootstrap::resolveLaunchCoreType(const VmessItem& server) const
{
    return resolveEffectiveCoreType(server);
}

CoreInfo AppBootstrap::resolveCoreInfo(const VmessItem& server) const
{
    CoreInfo info;
    const CoreType launchCore = resolveLaunchCoreType(server);
    const QStringList candidates = resolveCoreCandidates(launchCore);

    switch (launchCore) {
    case CoreType::SingBox:
        info.arguments = QStringList{QStringLiteral("run"), QStringLiteral("-c"), info.configPlaceholder};
        info.appendConfigArgument = false;
        break;
    case CoreType::Xray:
    case CoreType::Unknown:
    default:
        info.arguments = QStringList();
        info.appendConfigArgument = true;
        break;
    }

    const QString program = locateFirstExistingFile(candidates);

    if (program.isEmpty()) {
        return {};
    }

    info.program = program;
    info.workingDirectory = QFileInfo(program).absolutePath();

    const QString fileName = QFileInfo(program).fileName();
    if (fileName.contains(QStringLiteral("v2ray"), Qt::CaseInsensitive)
        || fileName.contains(QStringLiteral("xray"), Qt::CaseInsensitive)) {
        info.arguments = QStringList();
        info.appendConfigArgument = true;
    }

    return info;
}

QStringList AppBootstrap::resolveCoreCandidates(CoreType coreType) const
{
    return resolveCoreCandidatesForConfigPath(coreType, resolveConfigPath());
}

QString AppBootstrap::locateFirstExistingFile(const QStringList& candidates) const
{
    return locateFirstExistingFileInCandidates(candidates);
}

void AppBootstrap::refreshExistingCoreTypes()
{
    existingCoreTypes_ = detectExistingCoreTypes();
}

QList<CoreType> AppBootstrap::detectExistingCoreTypes() const
{
    QList<CoreType> existingCoreTypes;
    for (const CoreType coreType : availableCoreTypes()) {
        if (!locateFirstExistingFile(resolveCoreCandidates(coreType)).isEmpty()) {
            existingCoreTypes.append(coreType);
        }
    }
    return existingCoreTypes;
}

QString AppBootstrap::detectCoreVersion(CoreType coreType) const
{
    const QString program = locateFirstExistingFile(resolveCoreCandidates(coreType));
    return readCoreVersion(coreType, program);
}

QString AppBootstrap::resolveCoreInstallDirectory(CoreType coreType) const
{
    const QString existingCorePath = locateFirstExistingFile(resolveCoreCandidates(coreType));
    if (!existingCorePath.trimmed().isEmpty()) {
        return QFileInfo(existingCorePath).absolutePath();
    }

    const QString configDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    if (!configDirectory.trimmed().isEmpty()) {
        return configDirectory;
    }

    return QCoreApplication::applicationDirPath();
}


