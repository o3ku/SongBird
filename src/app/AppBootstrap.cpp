#include "app/AppBootstrap.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
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
#include "app/SingleInstanceBootstrap.h"
#include "app/TunSettingsApplyDecision.h"
#include "app/TunRuntimeState.h"
#include "common/CorePidFile.h"
#include "common/DataSizeFormatter.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "platform/windows/WindowsAutoRunService.h"
#include "platform/windows/WindowsGlobalHotkeyService.h"
#include "platform/windows/WindowsSystemProxyService.h"
#include "persistence/JsonConfigRepository.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/CoreLifecycleService.h"
#include "runtime/ProtocolCoreCompat.h"
#include "runtime/QtCoreProcessHost.h"
#include "runtime/ServerConfigWriter.h"
#include "runtime/TunCompatCoreRequirement.h"
#include "services/ConfigBackupService.h"
#include "services/CoreUpdateService.h"
#include "services/GeoResourceUpdateService.h"
#include "services/GrpcStatisticsBackend.h"
#include "services/ProxyAvailabilityCheckService.h"
#include "services/RoutingService.h"
#include "services/ServerService.h"
#include "services/SpeedTestService.h"
#include "services/StatisticsService.h"
#include "services/SubscriptionService.h"
#include "services/SubscriptionUpdateService.h"
#include "subscription/ConfigFileImportParser.h"
#include "subscription/CustomConfigTextParser.h"
#include "subscription/SubscriptionImportTextParser.h"
#include "ui/dialogs/AddServerDialog.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/CustomServerDialog.h"
#include "ui/dialogs/DnsSettingsDialog.h"
#include "ui/dialogs/GlobalHotkeyDialog.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/tray/TrayController.h"

namespace {

constexpr auto DefaultIeProxyExceptions =
    "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;"
    "172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*";
constexpr auto ProjectPageUrl = "https://github.com/o3ku/v2rayq";
constexpr auto ReleasePageUrl = "https://github.com/o3ku/v2rayq/releases";
constexpr auto AppReleaseDate = __DATE__;
constexpr auto DocumentationUrl = "https://www.v2fly.org/";
constexpr auto DnsObjectUrl = "https://www.v2fly.org/config/dns.html#dnsobject";
constexpr auto RuleObjectUrl = "https://www.v2fly.org/config/routing.html#ruleobject";
constexpr int LocationProbePortOffset = 103;

#if defined(Q_OS_WIN)
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
    const QString city = object.value(QStringLiteral("city")).toString().trimmed();
    QString country = object.value(QStringLiteral("country")).toString().trimmed();
    if (country.isEmpty()) {
        country = object.value(QStringLiteral("country_name")).toString().trimmed();
    }
    QString countryCode = object.value(QStringLiteral("countryCode")).toString().trimmed();
    if (countryCode.isEmpty()) {
        countryCode = object.value(QStringLiteral("country_code")).toString().trimmed();
    }

    QStringList parts;
    if (!country.isEmpty()) {
        const QString flag = countryCodeToFlag(countryCode);
        parts.append(flag.isEmpty() ? country : QStringLiteral("%1 %2").arg(flag, country));
    }
    if (!city.isEmpty()) {
        parts.append(city);
    }

    return parts.join(QStringLiteral(", "));
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
    for (const RoutingRule& rule : config.routingCustomRules) {
        if (rule.enabled) {
            effectiveRules.append(rule);
        }
    }

    if (config.enableRoutingAdvanced
        && config.routingIndex >= 0
        && config.routingIndex < config.routingItems.size()) {
        const RoutingItem& selectedRouting = config.routingItems.at(config.routingIndex);
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
    return !qEnvironmentVariableIsSet("QT_V2RAYN_NONINTERACTIVE");
}

QString tunAdminRequiredStartMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows. Run v2rayq as administrator before starting the core.");
}

QString tunAdminRequiredSaveMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows. The settings were saved and will take effect after restarting v2rayq as administrator.");
}

QString tunAdminRestartPromptTitle()
{
    return QCoreApplication::translate("AppBootstrap", "Administrator Permission");
}

QString tunAdminRestartPromptMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows.\nRestart v2rayq as administrator now to apply the saved TUN setting?");
}

QString tunAdminRestartFailureMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "Failed to restart v2rayq with administrator privileges.");
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
    bool autoStartCore,
    bool startHidden,
    bool registerGlobalHotkeys,
    bool skipCoreChecks)
    : configPath_(std::move(configPath))
    , autoStartCore_(autoStartCore)
    , startHidden_(startHidden)
    , registerGlobalHotkeys_(registerGlobalHotkeys)
    , skipCoreChecks_(skipCoreChecks)
{
}

AppBootstrap::~AppBootstrap()
{
    shuttingDown_.store(true);
    lifetimeGuard_.reset();
    cancelPendingCoreRestarts();
    waitForBackgroundThreads();

    stopStatisticsBackends();
    if (coreLifecycleService_) {
        coreLifecycleService_->stop();
    }
    if (auxiliaryCoreLifecycleService_) {
        auxiliaryCoreLifecycleService_->stop();
    }
    stopStatisticsSession();
}

bool AppBootstrap::run()
{
    repository_ = std::make_unique<JsonConfigRepository>(resolveConfigPath());
    serverService_ = std::make_unique<ServerService>(*repository_, resolveCustomConfigDirectory());
    configBackupService_ = std::make_unique<ConfigBackupService>(resolveConfigPath());
    routingService_ = std::make_unique<RoutingService>(*repository_);
    speedTestService_ = std::make_unique<SpeedTestService>(resolveCustomConfigDirectory());
    statisticsService_ = std::make_unique<StatisticsService>(resolveStatisticsFilePath());
    grpcStatisticsBackend_ = std::make_unique<GrpcStatisticsBackend>();
    subscriptionService_ = std::make_unique<SubscriptionService>(*repository_);
    geoResourceUpdateService_ = std::make_unique<GeoResourceUpdateService>(
        QFileInfo(resolveConfigPath()).dir().absolutePath());
    clientConfigWriter_ = std::make_unique<ClientConfigWriter>(resolveCustomConfigDirectory());
    serverConfigWriter_ = std::make_unique<ServerConfigWriter>();
    coreProcessHost_ = std::make_unique<QtCoreProcessHost>();
    coreLifecycleService_ = std::make_unique<CoreLifecycleService>(*coreProcessHost_);
    auxiliaryCoreProcessHost_ = std::make_unique<QtCoreProcessHost>();
    auxiliaryCoreLifecycleService_ = std::make_unique<CoreLifecycleService>(*auxiliaryCoreProcessHost_);
    autoRunService_ = std::make_unique<WindowsAutoRunService>();
    globalHotkeyService_ = std::make_unique<WindowsGlobalHotkeyService>();
    systemProxyService_ = std::make_unique<WindowsSystemProxyService>();

    mainWindow_ = std::make_unique<MainWindow>();
    QObject::connect(coreLifecycleService_.get(), &CoreLifecycleService::outputReceived, mainWindow_.get(), [this](const QString& line) {
        mainWindow_->appendLog(line);
    });
    QObject::connect(auxiliaryCoreLifecycleService_.get(), &CoreLifecycleService::outputReceived, mainWindow_.get(), [this](const QString& line) {
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
    QObject::connect(statisticsService_.get(), &StatisticsService::statisticsChanged, mainWindow_.get(), [this]() {
        syncStatusIndicators();
    });
    QObject::connect(statisticsService_.get(), &StatisticsService::sessionChanged, mainWindow_.get(), [this]() {
        syncStatusIndicators();
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::runningChanged, mainWindow_.get(), [this](bool running) {
        if (mainWindow_ != nullptr) {
            mainWindow_->setSpeedTestRunning(running);
        }
        if (!running) {
            finishBackgroundTask(BackgroundTaskKind::SpeedTest);
        } else {
            syncBackgroundTaskState();
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::logGenerated, mainWindow_.get(), [this](const QString& message) {
        if (mainWindow_ != nullptr) {
            mainWindow_->appendLog(message);
            mainWindow_->showTransientStatus(message, 3000);
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::testResultReady, mainWindow_.get(), [this](const QString& indexId, const QString& result) {
        if (serverService_ == nullptr) {
            return;
        }

        const OperationResult updateResult = serverService_->updateTestResult(config_, indexId, result);
        if (!updateResult.success) {
            appendResult(updateResult);
            return;
        }

        if (mainWindow_ != nullptr) {
            mainWindow_->updateServerTestResult(indexId, result);
        }
    });
    QObject::connect(speedTestService_.get(), &SpeedTestService::finished, mainWindow_.get(), [this](const QString& summary) {
        if (mainWindow_ != nullptr) {
            mainWindow_->showTransientStatus(summary, 4000);
        }
    });
    QObject::connect(grpcStatisticsBackend_.get(), &GrpcStatisticsBackend::trafficDeltaReceived, mainWindow_.get(), [this](quint64 up, quint64 down) {
        if (statisticsService_ == nullptr) {
            return;
        }

        const QString activeServerId = statisticsService_->sessionState().activeServerId.trimmed();
        if (activeServerId.isEmpty()) {
            return;
        }

        statisticsService_->applyTrafficDelta(activeServerId, up, down);
    });
    QObject::connect(
        grpcStatisticsBackend_.get(),
        &GrpcStatisticsBackend::pollingAvailabilityChanged,
        mainWindow_.get(),
        [this](bool available) {
            if (statisticsService_ != nullptr) {
                statisticsService_->setPollingAvailable(available);
            }
        });

    trayController_ = std::make_unique<TrayController>(mainWindow_.get());

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, mainWindow_.get(), [this]() {
        shuttingDown_.store(true);
        cancelPendingCoreRestarts();
        if (!shutdownUiStatePersisted_) {
            persistUiState();
            shutdownUiStatePersisted_ = true;
        }
        if (isCoreRunning()) {
            stopCore(true);
        } else {
            stopAuxiliaryCore();
        }
        applySystemProxyModeOnExit(windowsShutdownRequested_);
    });
    if (QGuiApplication* guiApplication = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        QObject::connect(guiApplication, &QGuiApplication::commitDataRequest, mainWindow_.get(), [this](QSessionManager&) {
            windowsShutdownRequested_ = true;
            shuttingDown_.store(true);
            cancelPendingCoreRestarts();
            if (!shutdownUiStatePersisted_) {
                persistUiState();
                shutdownUiStatePersisted_ = true;
            }
            if (isCoreRunning()) {
                stopCore(true);
            } else {
                stopAuxiliaryCore();
            }
            applySystemProxyModeOnExit(true);
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
    if (registerGlobalHotkeys_) {
        appendResult(globalHotkeyService_->registerHotkeys(config_.globalHotkeys));
    } else {
        appendResult(OperationResult::ok(QCoreApplication::translate(
            "AppBootstrap",
            "Global hotkeys disabled by command line.")));
    }
    if (systemProxyService_ != nullptr) {
        managedSystemProxyActive_ = shouldAdoptManagedSystemProxyOnStartup(
            normalizeSystemProxyMode(config_.sysProxyType),
            config_.mainProxyEnabled,
            systemProxyService_->isEnabled());
    }
    mainWindow_->show();
    if (startHidden_ || !config_.showMainOnStartup) {
        if (trayController_ != nullptr && trayController_->isAvailable()) {
            mainWindow_->hide();
        } else {
            mainWindow_->showMinimized();
            appendResult(OperationResult::ok(QStringLiteral(
                "Start hidden requested, but the system tray is unavailable. The window was minimized instead.")));
        }
    }
    uiReady_ = true;
    if (autoStartCore_ || skipCoreChecks_ || shouldStartCoreOnStartup()) {
        QTimer::singleShot(0, mainWindow_.get(), [this]() {
            startCoreOnStartup();
        });
    }

    return true;
}

void AppBootstrap::wireMainWindow()
{
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
        const VmessItem* existing = findServerById(indexId);
        if (existing == nullptr) {
            mainWindow_->appendLog(QStringLiteral("The selected server could not be found for editing."));
            return;
        }

        const QString activeServerId = resolveActiveServer() == nullptr
            ? QString()
            : resolveActiveServer()->indexId;
        const bool shouldRestartCore = isCoreRunning() && activeServerId == indexId;
        OperationResult result;

        if (existing->configType == ConfigType::Custom) {
            CustomServerDialog dialog(mainWindow_.get());
            dialog.setWindowTitle(QStringLiteral("Edit Custom Server"));
            dialog.setServer(*existing, serverService_->resolveCustomConfigPath(existing->address));
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            result = serverService_->updateServer(config_, indexId, dialog.server());
        } else {
            AddServerDialog dialog(mainWindow_.get());
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
            restartCoreIfRunning(QStringLiteral("Reloading core after editing the active server."));
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
    });

    QObject::connect(mainWindow_.get(), &MainWindow::systemProxyModeSelected, mainWindow_.get(), [this](int mode) {
        setSystemProxyMode(normalizeSystemProxyMode(mode));
    });
    QObject::connect(mainWindow_.get(), &MainWindow::enableSystemProxyRequested, mainWindow_.get(), [this]() {
        enableSystemProxy();
    });
    QObject::connect(mainWindow_.get(), &MainWindow::disableSystemProxyRequested, mainWindow_.get(), [this]() {
        disableSystemProxy();
    });
    QObject::connect(mainWindow_.get(), &MainWindow::routingModeSelected, mainWindow_.get(), [this](int mode) {
        const bool previousAdvancedEnabled = config_.enableRoutingAdvanced;
        const int previousRoutingIndex = config_.routingIndex;
        const OperationResult result = routingService_->setRoutingMode(config_, mode >= 0, mode);
        appendResult(result);
        syncWindow();
        if (result.success
            && (previousAdvancedEnabled != config_.enableRoutingAdvanced
                || previousRoutingIndex != config_.routingIndex)) {
            restartCoreIfRunning(QStringLiteral("Reloading core to apply routing changes."));
        }
    });

    QObject::connect(mainWindow_.get(), &MainWindow::toggleAutoRunRequested, mainWindow_.get(), [this]() {
        toggleAutoRun();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::restoreBackupRequested, mainWindow_.get(), [this]() {
        restoreConfigFromBackup();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::globalHotkeySettingsRequested, mainWindow_.get(), [this]() {
        if (mainWindow_ == nullptr || serverService_ == nullptr) {
            return;
        }

        if (globalHotkeyService_ != nullptr) {
            globalHotkeyService_->setPaused(true);
        }
        GlobalHotkeyDialog dialog(mainWindow_.get());
        dialog.setHotkeys(config_.globalHotkeys);
        const int dialogResult = dialog.exec();
        if (globalHotkeyService_ != nullptr) {
            globalHotkeyService_->setPaused(false);
        }
        if (dialogResult != QDialog::Accepted) {
            return;
        }

        const QList<GlobalHotkeyItem> previousHotkeys = config_.globalHotkeys;
        config_.globalHotkeys = dialog.hotkeys();
        if (!serverService_->save(config_)) {
            config_.globalHotkeys = previousHotkeys;
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Failed to save global hotkey settings.")));
            return;
        }

        appendResult(OperationResult::ok(
            QCoreApplication::translate("AppBootstrap", "Global hotkey settings saved.")));
        if (registerGlobalHotkeys_ && globalHotkeyService_ != nullptr) {
            appendResult(globalHotkeyService_->registerHotkeys(config_.globalHotkeys));
        }
    });

    QObject::connect(mainWindow_.get(), &MainWindow::settingsRequested, mainWindow_.get(), [this]() {
        openSettingsDialog();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::dnsSettingsRequested, mainWindow_.get(), [this]() {
        if (mainWindow_ == nullptr || serverService_ == nullptr) {
            return;
        }

        DnsSettingsDialog dialog(mainWindow_.get());
        dialog.setConfig(config_);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const Config previous = config_;
        config_ = dialog.config();
        if (!serverService_->save(config_)) {
            config_ = previous;
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Failed to save DNS settings.")));
            return;
        }

        appendResult(OperationResult::ok(
            QCoreApplication::translate("AppBootstrap", "DNS settings saved.")));
        if (isCoreRunning()) {
            appendResult(OperationResult::ok(
                QCoreApplication::translate("AppBootstrap", "Restart the core to apply the new DNS settings.")));
        }
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
        const QString activeServerId = resolveActiveServer() == nullptr
            ? QString()
            : resolveActiveServer()->indexId;
        const bool activeServerRemoved = !activeServerId.isEmpty() && indexIds.contains(activeServerId);
        const OperationResult result = serverService_->removeServers(config_, indexIds);
        appendResult(result);
        syncWindow();
        if (!result.success || !activeServerRemoved || !isCoreRunning()) {
            return;
        }

        if (resolveActiveServer() == nullptr) {
            appendResult(OperationResult::ok(QStringLiteral("Stopping core because the active server was removed.")));
            stopCore(false);
            return;
        }

        restartCoreIfRunning(QStringLiteral("Reloading core after removing the active server."));
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
        const QString previousIndexId = config_.currentIndexId;
        const OperationResult result = serverService_->setDefaultServer(config_, indexId);
        appendResult(result);
        syncWindow();
        if (result.success) {
            if (mainWindow_ != nullptr) {
                mainWindow_->setCurrentServerWarning({});
            }
            setCurrentActivationPending_ = true;
            if (isCoreRunning()) {
                if (previousIndexId != config_.currentIndexId) {
                    restartCoreIfRunning(QStringLiteral("Reloading core after switching the default server."));
                } else {
                    setCurrentActivationPending_ = false;
                }
            } else {
                startCore();
            }
        }
    });
    QObject::connect(mainWindow_.get(), &MainWindow::testServersRequested, mainWindow_.get(), [this](const QStringList& indexIds) {
        startSpeedTest(indexIds);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::reloadConfigRequested, mainWindow_.get(), [this]() {
        reloadConfig();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::startCoreRequested, mainWindow_.get(), [this]() {
        if (reloadConfig()) {
            startCore();
        }
    });

    QObject::connect(mainWindow_.get(), &MainWindow::stopCoreRequested, mainWindow_.get(), [this]() {
        stopCore(false);
    });

    QObject::connect(mainWindow_.get(), &MainWindow::clearStatisticsRequested, mainWindow_.get(), [this]() {
        appendResult(statisticsService_->clearAll());
        syncWindow();
    });

    QObject::connect(mainWindow_.get(), &MainWindow::hiddenToTray, mainWindow_.get(), [this]() {
        mainWindow_->appendLog(QStringLiteral("Main window hidden to tray."));
    });

    QObject::connect(trayController_.get(), &TrayController::startCoreRequested, mainWindow_.get(), [this]() {
        if (reloadConfig()) {
            startCore();
        }
    });

    QObject::connect(trayController_.get(), &TrayController::stopCoreRequested, mainWindow_.get(), [this]() {
        stopCore(false);
    });

    QObject::connect(trayController_.get(), &TrayController::defaultServerRequested, mainWindow_.get(), [this](const QString& indexId) {
        const QString previousIndexId = config_.currentIndexId;
        const OperationResult result = serverService_->setDefaultServer(config_, indexId);
        appendResult(result);
        syncWindow();
        if (result.success) {
            if (mainWindow_ != nullptr) {
                mainWindow_->setCurrentServerWarning({});
            }
            setCurrentActivationPending_ = true;
            if (isCoreRunning()) {
                if (previousIndexId != config_.currentIndexId) {
                    restartCoreIfRunning(QStringLiteral("Reloading core after switching the default server."));
                } else {
                    setCurrentActivationPending_ = false;
                }
            } else {
                startCore();
            }
        }
    });

    QObject::connect(trayController_.get(), &TrayController::routingRequested, mainWindow_.get(), [this](int index) {
        const int previousRoutingIndex = config_.routingIndex;
        const OperationResult result = routingService_->selectRouting(config_, index);
        appendResult(result);
        syncWindow();
        if (result.success && previousRoutingIndex != config_.routingIndex) {
            restartCoreIfRunning(QStringLiteral("Reloading core to apply routing changes."));
        }
    });

    QObject::connect(trayController_.get(), &TrayController::quitRequested, mainWindow_.get(), [this]() {
        mainWindow_->requestExit();
    });

    QObject::connect(globalHotkeyService_.get(), &WindowsGlobalHotkeyService::toggleMainWindowRequested, mainWindow_.get(), [this]() {
        if (trayController_ != nullptr && trayController_->isAvailable()) {
            trayController_->toggleMainWindow();
            return;
        }

        if (mainWindow_ == nullptr) {
            return;
        }

        if (mainWindow_->isVisible()) {
            mainWindow_->showMinimized();
            return;
        }

        mainWindow_->show();
        mainWindow_->showNormal();
        mainWindow_->raise();
        mainWindow_->activateWindow();
    });

    QObject::connect(globalHotkeyService_.get(), &WindowsGlobalHotkeyService::enableProxyRequested, mainWindow_.get(), [this]() {
        enableSystemProxy();
    });

    QObject::connect(globalHotkeyService_.get(), &WindowsGlobalHotkeyService::disableProxyRequested, mainWindow_.get(), [this]() {
        disableSystemProxy();
    });

    QObject::connect(
        globalHotkeyService_.get(),
        &WindowsGlobalHotkeyService::keepProxyUnchangedRequested,
        mainWindow_.get(),
        [this]() {
            setSystemProxyMode(SystemProxyMode::Unchanged);
        });
    QObject::connect(
        globalHotkeyService_.get(),
        &WindowsGlobalHotkeyService::pacProxyRequested,
        mainWindow_.get(),
        [this]() {
            setSystemProxyMode(SystemProxyMode::Pac);
        });
}

void AppBootstrap::syncWindow()
{
    if (mainWindow_) {
        mainWindow_->setConfig(config_, statisticsService_ == nullptr ? QList<ServerStatItem>() : statisticsService_->statistics());
    }

    if (trayController_ != nullptr) {
        trayController_->setServers(config_.servers, config_.currentIndexId, config_.trayMenuServersLimit);
        trayController_->setRoutings(config_.routingItems, config_.routingIndex, config_.enableRoutingAdvanced);
    }

    syncStatusIndicators();
}

bool AppBootstrap::tryBeginBackgroundTask(BackgroundTaskKind kind)
{
    if (kind == BackgroundTaskKind::None) {
        return false;
    }

    if (activeBackgroundTask_ != BackgroundTaskKind::None) {
        return false;
    }

    activeBackgroundTask_ = kind;
    syncBackgroundTaskState();
    return true;
}

QString AppBootstrap::backgroundTaskDescription(BackgroundTaskKind kind) const
{
    switch (kind) {
    case BackgroundTaskKind::SpeedTest:
        return QCoreApplication::translate("AppBootstrap", "Running URL Test");
    case BackgroundTaskKind::SubscriptionUpdate:
        return QCoreApplication::translate("AppBootstrap", "Updating subscriptions");
    case BackgroundTaskKind::ProxyAvailabilityCheck:
        return QCoreApplication::translate("AppBootstrap", "Checking proxy availability");
    case BackgroundTaskKind::CoreUpdate:
        return QCoreApplication::translate("AppBootstrap", "Updating core");
    case BackgroundTaskKind::GeoUpdate:
        return QCoreApplication::translate("AppBootstrap", "Updating Geo resources");
    case BackgroundTaskKind::None:
    default:
        return {};
    }
}

void AppBootstrap::finishBackgroundTask(BackgroundTaskKind kind)
{
    if (activeBackgroundTask_ != kind) {
        return;
    }

    activeBackgroundTask_ = BackgroundTaskKind::None;
    syncBackgroundTaskState();
}

void AppBootstrap::syncBackgroundTaskState()
{
    const bool running = activeBackgroundTask_ != BackgroundTaskKind::None;
    const QString description = backgroundTaskDescription(activeBackgroundTask_);
    if (mainWindow_ != nullptr) {
        mainWindow_->setBackgroundTaskRunning(running);
        mainWindow_->setBackgroundTaskDescription(description);
    }
    if (trayController_ != nullptr) {
        trayController_->setBackgroundTaskRunning(running);
        trayController_->setBackgroundTaskDescription(description);
    }
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
    const QString currentServerName = describeServer(findServerById(config_.currentIndexId));
    const QString routingSummary = buildRoutingSummaryText();
    const QString listenSummary = buildListenSummaryText();
    StatisticsSessionState statisticsState = statisticsService_ == nullptr
        ? StatisticsSessionState()
        : statisticsService_->sessionState();
    statisticsState.enabled = config_.enableStatistics;
    if (!statisticsState.running) {
        statisticsState.refreshRateSeconds = qMax(1, config_.statisticsFreshRate);
    }

    if (mainWindow_ != nullptr) {
        mainWindow_->setExistingCoreTypes(existingCoreTypes_);
        mainWindow_->setCurrentServerName(currentServerName);
        mainWindow_->setCurrentServerLocation(currentServerLocation_);
        mainWindow_->setCoreProcessRunning(coreProcessRunning);
        mainWindow_->setRoutingSummary(routingSummary, listenSummary);
        mainWindow_->setCoreRunning(coreRunning, corePending);
        mainWindow_->setSystemProxyState(toLegacySystemProxyModeValue(systemProxyMode_), systemProxyEnabled);
        mainWindow_->setAutoRunEnabled(autoRunEnabled);
        mainWindow_->setBackgroundTaskRunning(activeBackgroundTask_ != BackgroundTaskKind::None);
        mainWindow_->setBackgroundTaskDescription(backgroundTaskDescription(activeBackgroundTask_));
        if (statisticsService_ != nullptr) {
            mainWindow_->setStatisticsSessionState(statisticsState);
        }
    }

    if (trayController_ != nullptr) {
        trayController_->setCurrentServerName(currentServerName);
        trayController_->setCoreProcessRunning(coreProcessRunning);
        trayController_->setCoreRunning(coreRunning, corePending);
        trayController_->setSystemProxyState(toLegacySystemProxyModeValue(systemProxyMode_), systemProxyEnabled);
        trayController_->setAutoRunEnabled(autoRunEnabled);
        trayController_->setRoutingSummary(routingSummary, config_.enableRoutingAdvanced);
        trayController_->setStatisticsSummary(buildStatisticsSummaryText());
        trayController_->setTrafficSummary(buildTrafficSummaryText());
        trayController_->setBackgroundTaskRunning(activeBackgroundTask_ != BackgroundTaskKind::None);
        trayController_->setBackgroundTaskDescription(backgroundTaskDescription(activeBackgroundTask_));
    }
}

void AppBootstrap::clearCurrentServerLocation()
{
    currentServerLocation_.clear();
    if (mainWindow_ != nullptr) {
        mainWindow_->setCurrentServerLocation({});
    }
}

void AppBootstrap::queryCurrentServerLocation(const QString& serverIndexId, CoreType runtimeCore)
{
    if (!coreReady_ || serverIndexId.trimmed().isEmpty()) {
        return;
    }

    clearCurrentServerLocation();

    const VmessItem* activeServer = findServerById(serverIndexId);
    const bool usesDedicatedProbe = activeServer != nullptr
        && activeServer->configType != ConfigType::Custom;

    int httpPort = config_.localPort + 1;
    if (usesDedicatedProbe) {
        httpPort = config_.localPort + LocationProbePortOffset;
    }
    if (httpPort <= 0 || httpPort > 65535) {
        return;
    }

    QPointer<QObject> uiContext(mainWindow_.get());
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    const QString activeServerId = serverIndexId.trimmed();
    QThread* thread = QThread::create([this, uiContext, lifetimeGuard, activeServerId, httpPort]() {
        QNetworkAccessManager manager;
        manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), httpPort));

        QNetworkRequest request(QUrl(QStringLiteral("http://ip-api.com/json/")));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("v2rayq/%1").arg(QCoreApplication::applicationVersion()));

        QEventLoop loop;
        QNetworkReply* reply = manager.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();

        QString location;
        if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
            location = buildLocationSummaryFromPayload(reply->readAll());
        }
        reply->deleteLater();

        QObject* target = uiContext.isNull()
            ? static_cast<QObject*>(QCoreApplication::instance())
            : uiContext.data();
        invokeOnUiThread(target, [this, lifetimeGuard, activeServerId, location]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (!coreReady_ || config_.currentIndexId.trimmed() != activeServerId) {
                return;
            }
            currentServerLocation_ = location.trimmed();
            if (mainWindow_ != nullptr) {
                mainWindow_->setCurrentServerLocation(currentServerLocation_);
            }
        });
    });
    trackBackgroundThread(thread);
    thread->start();
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
    if (uiReady_ && registerGlobalHotkeys_ && globalHotkeyService_ != nullptr) {
        appendResult(globalHotkeyService_->registerHotkeys(config_.globalHotkeys));
    }
    if (statisticsService_ != nullptr) {
        statisticsService_->load();
    }
    syncWindow();
    if (!uiStateRestored_ && mainWindow_ != nullptr) {
        mainWindow_->restoreUiState(config_);
        uiStateRestored_ = true;
    }
    appendResult(OperationResult::ok(QStringLiteral("Configuration reloaded from disk.")));
    appendStartupResourceCheckResults();
    return true;
}

bool AppBootstrap::shouldStartCoreOnStartup() const
{
    if (skipCoreChecks_) {
        return false;
    }

    return config_.mainCoreRunning && resolveActiveServer() != nullptr;
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
    if (skipCoreChecks_) {
        appendResult(OperationResult::ok(QStringLiteral("Startup core checks skipped by command line.")));
        return;
    }

    if (!autoStartCore_ && !shouldStartCoreOnStartup()) {
        return;
    }

    startCore(false, config_.mainProxyEnabled);
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

    if (!config_.currentIndexId.trimmed().isEmpty() && findServerById(config_.currentIndexId) == nullptr && !config_.servers.isEmpty()) {
        lines.append(QStringLiteral(
            "Startup check: The configured default server does not exist. The first available server will be used."));
    }

    int missingCustomConfigCount = 0;
    for (const VmessItem& item : config_.servers) {
        if (item.configType != ConfigType::Custom) {
            continue;
        }

        const QString customConfigPath = resolveCustomConfigPath(item.address);
        if (customConfigPath.trimmed().isEmpty() || !QFileInfo::exists(customConfigPath)) {
            ++missingCustomConfigCount;
        }
    }

    const VmessItem* activeServer = resolveActiveServer();
    if (activeServer != nullptr) {
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
                                 .arg(describeServer(activeServer))
                                 .arg(expectedCoreFilesText(candidates)));
            }

            for (const CoreType auxiliaryCoreType : resolveAuxiliaryTunCompatCoreTypes(config_, *activeServer, existingCoreTypes_)) {
                const QStringList candidates = resolveCoreCandidates(auxiliaryCoreType);
                if (locateFirstExistingFile(candidates).isEmpty()) {
                    lines.append(QStringLiteral("Startup check: Default server \"%1\" also needs the %2 core for TUN compatibility. Expected one of: %3.")
                                     .arg(describeServer(activeServer))
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

void AppBootstrap::startCore()
{
    const bool preserveProxyEnabled = systemProxyService_ != nullptr && systemProxyService_->isEnabled();
    startCore(false, preserveProxyEnabled);
}

void AppBootstrap::startCore(bool skipTunCleanup)
{
    const bool preserveProxyEnabled = systemProxyService_ != nullptr && systemProxyService_->isEnabled();
    startCore(skipTunCleanup, preserveProxyEnabled);
}

void AppBootstrap::startCore(bool skipTunCleanup, std::optional<bool> startupProxyEnabled)
{
    if (coreStartPending_ || coreStopPending_) {
        appendResult(OperationResult::ok(
            coreStopPending_
                ? QStringLiteral("Core stop is already in progress.")
                : QStringLiteral("Core start is already in progress.")));
        return;
    }
    cancelPendingCoreRestarts();
    clearCurrentServerLocation();
    if (isTunRuntimeBlocked(config_, isWindowsPlatform(), isProcessElevated())) {
        appendResult(OperationResult::fail(tunAdminRequiredStartMessage()));
        syncStatusIndicators();
        return;
    }
    const VmessItem* server = resolveActiveServer();
    if (server == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("No active server is available for runtime config generation.")));
        return;
    }

    if (config_.tunModeItem.enableTun && !skipTunCleanup) {
        if (tunCleanupActive_) {
            appendResult(OperationResult::ok(QStringLiteral("TUN adapter cleanup is already in progress.")));
            return;
        }

        tunCleanupActive_ = true;
        resumeCoreStartAfterTunCleanup_ = true;
        syncStatusIndicators();
        removeStaleTunAdapterAsync([this, startupProxyEnabled](const OperationResult& result) {
            tunCleanupActive_ = false;
            const bool resumeStart = resumeCoreStartAfterTunCleanup_;
            resumeCoreStartAfterTunCleanup_ = false;
            appendResult(result);
            syncStatusIndicators();
            if (shouldResumeCoreStartAfterTunCleanup(
                    result.success,
                    resumeStart,
                    shuttingDown_.load(),
                    coreStopPending_)) {
                startCore(true, startupProxyEnabled);
            }
        });
        return;
    }

    const CoreType launchCore = resolveLaunchCoreType(*server);
    const VmessItem runtimeServer = runtimeServerForLaunchCore(*server, launchCore);
    Config runtimeConfig = config_;
    const CoreInfo coreInfo = resolveCoreInfo(runtimeServer);
    if (coreInfo.program.isEmpty()) {
        const CoreType runtimeCore = launchCore;
        const QStringList candidates = resolveCoreCandidates(runtimeCore);
        const QString expectedFiles = expectedCoreFilesText(candidates);
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "No compatible %1 core executable was found for the active server \"%2\". Expected one of: %3.")
                .arg(coreTypeDisplayName(runtimeCore))
                .arg(describeServer(server))
                .arg(expectedFiles)));

        if (mainWindow_ != nullptr) {
            const QString title = QCoreApplication::translate("AppBootstrap", "Install %1 Core")
                                      .arg(coreTypeDisplayName(runtimeCore));
            const QString prompt = QCoreApplication::translate(
                                       "AppBootstrap",
                                       "The active server \"%1\" requires the %2 core, but no compatible executable was found.\nExpected one of: %3.\nInstall it now?")
                                       .arg(describeServer(server))
                                       .arg(coreTypeDisplayName(runtimeCore))
                                       .arg(expectedFiles);
            if (!interactivePromptsEnabled()) {
                appendResult(OperationResult::ok(
                    QCoreApplication::translate(
                        "AppBootstrap",
                        "Non-interactive mode suppressed the missing-core download prompt for \"%1\".")
                        .arg(describeServer(server))));
            } else if (DialogUtils::askYesNoQuestion(
                           mainWindow_.get(),
                           title,
                           prompt,
                           QMessageBox::Yes)
                       == QMessageBox::Yes) {
                updateCore(static_cast<int>(runtimeCore), true, mainWindow_.get(), mainWindow_.get(), true, true, {}, {});
            }
        }
        return;
    }

    for (const CoreType auxiliaryCoreType : resolveAuxiliaryTunCompatCoreTypes(runtimeConfig, runtimeServer, existingCoreTypes_)) {
        const QStringList auxiliaryCandidates = resolveCoreCandidates(auxiliaryCoreType);
        const QString auxiliaryProgram = locateFirstExistingFile(auxiliaryCandidates);
        if (!auxiliaryProgram.isEmpty()) {
            continue;
        }

        const QString expectedFiles = expectedCoreFilesText(auxiliaryCandidates);
        appendResult(OperationResult::fail(
            QCoreApplication::translate(
                "AppBootstrap",
                "The active server \"%1\" needs the %2 core for TUN compatibility, but no compatible executable was found.\nExpected one of: %3.")
                .arg(describeServer(server))
                .arg(coreTypeDisplayName(auxiliaryCoreType))
                .arg(expectedFiles)));

        if (mainWindow_ != nullptr) {
            const QString title = QCoreApplication::translate("AppBootstrap", "Install %1 Core")
                                      .arg(coreTypeDisplayName(auxiliaryCoreType));
            const QString prompt = QCoreApplication::translate(
                                       "AppBootstrap",
                                       "The active server \"%1\" needs the %2 core for TUN compatibility, but no compatible executable was found.\nExpected one of: %3.\nInstall it now?")
                                       .arg(describeServer(server))
                                       .arg(coreTypeDisplayName(auxiliaryCoreType))
                                       .arg(expectedFiles);
            if (!interactivePromptsEnabled()) {
                appendResult(OperationResult::ok(
                    QCoreApplication::translate(
                        "AppBootstrap",
                        "Non-interactive mode suppressed the missing-core download prompt for \"%1\".")
                        .arg(describeServer(server))));
            } else if (DialogUtils::askYesNoQuestion(
                           mainWindow_.get(),
                           title,
                           prompt,
                           QMessageBox::Yes)
                       == QMessageBox::Yes) {
                updateCore(static_cast<int>(auxiliaryCoreType), true, mainWindow_.get(), mainWindow_.get(), true, true, {}, {});
            }
        }
        return;
    }

    const QString coreConfigPath = resolveRuntimeConfigPath(*server);
    if (coreConfigPath.isEmpty()) {
        appendResult(OperationResult::fail(QStringLiteral("Failed to resolve the runtime config output path.")));
        return;
    }

    int statisticsPort = 0;
    const CoreType runtimeCore = launchCore;
    if (config_.enableStatistics) {
        if (runtimeServer.configType == ConfigType::Custom) {
            appendResult(OperationResult::ok(QStringLiteral("Custom servers keep their own statistics settings.")));
        } else {
            statisticsPort = resolveStatisticsPort();
            if (statisticsPort <= 0) {
                appendResult(OperationResult::fail(QStringLiteral("Failed to allocate a local statistics API port.")));
                return;
            }
        }
    }

    QStringList auxiliaryConfigPaths;
    removeStaleSingBoxCache();
    clientConfigWriter_->setExistingCoreTypes(existingCoreTypes_);
    const OperationResult writeResult = clientConfigWriter_->writeClientConfigs(
        runtimeConfig,
        runtimeServer,
        coreConfigPath,
        statisticsPort,
        &auxiliaryConfigPaths);
    appendResult(writeResult);
    if (!writeResult.success) {
        return;
    }

    stopAuxiliaryCore();
    if (!auxiliaryConfigPaths.isEmpty()) {
        const QString auxiliaryProgram = locateFirstExistingFile(resolveCoreCandidates(CoreType::SingBox));
        if (auxiliaryProgram.isEmpty()) {
            appendResult(OperationResult::fail(
                QStringLiteral("TUN compatibility mode requires sing-box, but no compatible sing-box executable was found.")));
            return;
        }

        CoreInfo auxiliaryCoreInfo;
        auxiliaryCoreInfo.program = auxiliaryProgram;
        auxiliaryCoreInfo.arguments = QStringList{QStringLiteral("run"), QStringLiteral("-c"), auxiliaryCoreInfo.configPlaceholder};
        auxiliaryCoreInfo.appendConfigArgument = false;
        auxiliaryCoreInfo.workingDirectory = QFileInfo(auxiliaryProgram).absolutePath();

        appendResult(OperationResult::ok(QStringLiteral("Starting TUN compatibility relay core...")));
        const OperationResult auxiliaryStartResult = auxiliaryCoreLifecycleService_->start(
            auxiliaryCoreInfo,
            auxiliaryConfigPaths.constFirst());
        appendResult(auxiliaryStartResult);
        if (!auxiliaryStartResult.success) {
            stopAuxiliaryCore();
            return;
        }
    }

    stopStatisticsBackends();

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
         statisticsPort,
         customServer = (server->configType == ConfigType::Custom),
         startupProxyEnabled](const QString& message) mutable {
            disconnectPendingCoreStartConnection();
            appendResult(OperationResult::ok(message));
            handleCoreStarted(serverIndexId, runtimeCore, statisticsPort, customServer, startupProxyEnabled);
        });

    coreStartPending_ = true;
    coreReady_ = false;
    coreTunEnabledAtStart_ = runtimeConfig.tunModeItem.enableTun;
    const OperationResult startResult = coreLifecycleService_->start(launchCoreInfo, coreConfigPath);
    appendResult(startResult);
    if (!startResult.success) {
        coreStartPending_ = false;
        coreReady_ = false;
        coreTunEnabledAtStart_ = false;
        disconnectPendingCoreStartConnection();
        stopAuxiliaryCore();
        stopStatisticsSession();
        syncStatusIndicators();
        return;
    }

    syncStatusIndicators();
}

void AppBootstrap::handleCoreStarted(
    const QString& serverIndexId,
    CoreType runtimeCore,
    int statisticsPort,
    bool customServer,
    std::optional<bool> startupProxyEnabled)
{
    if (mainWindow_ != nullptr) {
        mainWindow_->setCurrentServerWarning({});
    }
    coreStartPending_ = false;
    coreReady_ = true;
    disconnectPendingCoreStartConnection();
    if (statisticsService_ != nullptr) {
        statisticsService_->startSession(
            config_,
            serverIndexId,
            runtimeCore,
            statisticsPort,
            statisticsPort > 0,
            customServer);
    }
    if (statisticsPort > 0) {
        appendResult(OperationResult::ok(QStringLiteral("Statistics API enabled on 127.0.0.1:%1").arg(statisticsPort)));
        if (grpcStatisticsBackend_ != nullptr) {
            appendResult(grpcStatisticsBackend_->start(
                QStringLiteral("127.0.0.1"),
                statisticsPort,
                qMax(1, config_.statisticsFreshRate)));
        }
    }

    bool shouldQueryLocation = false;
    const bool shouldRestoreProxy = !startupProxyEnabled.has_value() || startupProxyEnabled.value();
    if (systemProxyService_ != nullptr
        && normalizeSystemProxyMode(config_.sysProxyType) == SystemProxyMode::ForcedChange
        && shouldRestoreProxy) {
        const bool wasApplied = systemProxyService_->isEnabled();
        const bool proxyUpdated = systemProxyService_->update(
            SystemProxyMode::ForcedChange,
            config_.localPort + 1,
            config_.localPort,
            buildSystemProxyExceptions(),
            config_.systemProxyAdvancedProtocol);
        if (!proxyUpdated) {
            appendResult(OperationResult::fail(QStringLiteral(
                "Failed to apply the configured Global system proxy after starting the core.")));
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
        } else if (!wasApplied) {
            appendResult(OperationResult::ok(QStringLiteral(
                "Applied the configured Global system proxy after starting the core.")));
        }
        if (proxyUpdated) {
            managedSystemProxyActive_ = true;
            shouldQueryLocation = true;
        }
    } else {
        clearCurrentServerLocation();
    }
    setCurrentActivationPending_ = false;

    syncStatusIndicators();
    if (shouldQueryLocation) {
        queryCurrentServerLocation(serverIndexId, runtimeCore);
    } else {
        clearCurrentServerLocation();
    }
}

void AppBootstrap::handleCoreStartFailed(const QString& message)
{
    coreStartPending_ = false;
    coreStopPending_ = false;
    coreReady_ = false;
    coreTunEnabledAtStart_ = false;
    disconnectPendingCoreStartConnection();
    appendResult(OperationResult::fail(message));
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
    stopStatisticsSession();
    clearCurrentServerLocation();
    syncStatusIndicators();
}

void AppBootstrap::disconnectPendingCoreStartConnection()
{
    if (coreStartedConnection_) {
        QObject::disconnect(coreStartedConnection_);
        coreStartedConnection_ = {};
    }
}

void AppBootstrap::stopStatisticsBackends()
{
    if (grpcStatisticsBackend_ != nullptr) {
        grpcStatisticsBackend_->stop();
    }
}

void AppBootstrap::stopStatisticsSession()
{
    if (statisticsService_ != nullptr) {
        statisticsService_->stopSession();
        statisticsService_->save();
    }
}

void AppBootstrap::stopCore(bool immediate)
{
    stopCoreInternal(immediate, true);
}

void AppBootstrap::stopCoreInternal(bool immediate, bool clearRestartAfterStop)
{
    coreStartPending_ = false;
    coreReady_ = false;
    clearCurrentServerLocation();
    disconnectPendingCoreStartConnection();
    cancelPendingCoreRestarts();
    resumeCoreStartAfterTunCleanup_ = false;
    if (clearRestartAfterStop) {
        restartAfterStopPending_ = false;
    }
    if (clearRestartAfterStop
        && systemProxyService_ != nullptr
        && shouldClearManagedSystemProxy(managedSystemProxyActive_, systemProxyService_->isEnabled())) {
        const bool proxyCleared = systemProxyService_->update(
            SystemProxyMode::ForcedClear,
            config_.localPort + 1,
            config_.localPort,
            buildSystemProxyExceptions(),
            config_.systemProxyAdvancedProtocol,
            config_.pacUrl);
        appendResult(proxyCleared
            ? OperationResult::ok(QStringLiteral("System proxy disabled because the core was stopped."))
            : OperationResult::fail(QStringLiteral("Failed to disable system proxy after stopping the core.")));
        if (proxyCleared) {
            managedSystemProxyActive_ = false;
        }
    }
    stopStatisticsBackends();
    if (!immediate && coreLifecycleService_ != nullptr && coreLifecycleService_->isRunning()) {
        coreStopPending_ = true;
    } else {
        coreStopPending_ = false;
    }
    skipTunCleanupOnNextCoreExit_ = immediate
        && shouldCleanupTunAfterCoreStop(isWindowsPlatform(), coreTunEnabledAtStart_);
    const bool cleanupTunSynchronously = skipTunCleanupOnNextCoreExit_;
    const OperationResult stopResult = coreLifecycleService_->stop(immediate);
    appendResult(stopResult);
    stopAuxiliaryCore(immediate);
    if (cleanupTunSynchronously && stopResult.success) {
        tunCleanupActive_ = false;
        removeStaleTunAdapter();
    }
    stopStatisticsSession();
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
    if (recordedPids.isEmpty()) {
        return;
    }

    static const wchar_t* kCoreProcessNames[] = {
        L"xray.exe",
        L"sing-box-client.exe",
        L"sing-box.exe",
        L"tuic.exe",
    };

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    QStringList terminatedProcesses;
    QSet<qint64> remainingPids = recordedPids;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (!recordedPids.contains(static_cast<qint64>(entry.th32ProcessID))) {
                continue;
            }

            bool nameMatches = false;
            for (const wchar_t* targetName : kCoreProcessNames) {
                if (_wcsicmp(entry.szExeFile, targetName) == 0) {
                    nameMatches = true;
                    break;
                }
            }

            if (!nameMatches) {
                remainingPids.remove(static_cast<qint64>(entry.th32ProcessID));
                continue;
            }

            HANDLE processHandle = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, entry.th32ProcessID);
            if (processHandle != nullptr) {
                if (TerminateProcess(processHandle, 1)) {
                    terminatedProcesses.append(QString::fromWCharArray(entry.szExeFile));
                }
                CloseHandle(processHandle);
            }
            remainingPids.remove(static_cast<qint64>(entry.th32ProcessID));
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    writeCorePids(QSet<qint64>());

    if (!terminatedProcesses.isEmpty()) {
        appendResult(OperationResult::ok(
            QStringLiteral("Cleaned up orphan core processes: %1").arg(terminatedProcesses.join(QStringLiteral(", ")))));
    }
#endif
}

void AppBootstrap::handleCoreExited(int exitCode, int status, bool stopRequested, bool auxiliary)
{
    if (!auxiliary) {
        coreStartPending_ = false;
        coreReady_ = false;
        disconnectPendingCoreStartConnection();
    }

    if (!auxiliary) {
        coreStopPending_ = false;
        stopStatisticsSession();
        stopStatisticsBackends();
    }

    const bool cleanupTunAfterStop = !auxiliary && shouldCleanupTunAfterCoreStop(
        isWindowsPlatform(),
        coreTunEnabledAtStart_);
    const bool cleanupAlreadyHandledOnImmediateStop = !auxiliary && skipTunCleanupOnNextCoreExit_;
    if (!auxiliary) {
        coreTunEnabledAtStart_ = false;
        skipTunCleanupOnNextCoreExit_ = false;
    }

    const bool restartAfterStop = !auxiliary && stopRequested && restartAfterStopPending_;
    if (restartAfterStop) {
        restartAfterStopPending_ = false;
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
        removeStaleTunAdapterAsync([this, restartAfterStop](const OperationResult& result) {
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
                    restartAfterStop,
                    shuttingDown_.load())) {
                startCore();
            }
        });
        if (shuttingDown_.load() || stopRequested) {
            return;
        }
    } else if (coreUpdatePendingAfterStop_ && !shuttingDown_.load()) {
        continuePendingCoreUpdate();
    } else if (restartAfterStop && !shuttingDown_.load()) {
        startCore();
    }
    if (shuttingDown_.load() || stopRequested) {
        return;
    }

    const QProcess::ExitStatus exitStatus = static_cast<QProcess::ExitStatus>(status);
    const QString core = auxiliary ? QStringLiteral("Auxiliary core") : QStringLiteral("Core");
    const QString kind = exitStatus == QProcess::CrashExit
        ? QStringLiteral("crash")
        : QStringLiteral("exit");
    scheduleCoreRestart(
        QStringLiteral("%1 %2 detected (code=%3). Restarting in 3s...")
            .arg(core, kind).arg(exitCode),
        auxiliary);
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

            if (!auxiliary && config_.tunModeItem.enableTun) {
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
                        startCore(true);
                    }
                });
                return;
            }

            startCore();
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

void AppBootstrap::restartCoreIfRunning(const QString& reason)
{
    if (!isCoreRunning() || coreStopPending_) {
        setCurrentActivationPending_ = false;
        return;
    }

    if (!reason.isEmpty()) {
        appendResult(OperationResult::ok(reason));
    }
    restartAfterStopPending_ = true;
    stopCoreInternal(false, false);
}

void AppBootstrap::setSystemProxyMode(SystemProxyMode mode)
{
    if (serverService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("System proxy mode cannot be changed before the configuration service is ready.")));
        return;
    }

    // PAC mode is disabled until the built-in PAC server is wired up. Treat
    // any attempt to activate it (including stale configs) as ForcedClear so
    // the registry never points to a non-existent local PAC endpoint.
    if (mode == SystemProxyMode::Pac) {
        mode = SystemProxyMode::ForcedClear;
    }

    const int previousValue = config_.sysProxyType;
    config_.sysProxyType = toLegacySystemProxyModeValue(mode);
    if (!serverService_->save(config_)) {
        config_.sysProxyType = previousValue;
        appendResult(OperationResult::fail(QStringLiteral("Failed to save the selected system proxy mode.")));
        syncStatusIndicators();
        return;
    }

    const bool deferManagedApply = mode == SystemProxyMode::ForcedChange && !isCoreRunning();
    const bool success = deferManagedApply
        || systemProxyService_ == nullptr
        || systemProxyService_->update(
            mode,
            config_.localPort + 1,
            config_.localPort,
            buildSystemProxyExceptions(),
            config_.systemProxyAdvancedProtocol,
            config_.pacUrl);
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

    if (mode == SystemProxyMode::ForcedChange && !isCoreRunning()) {
        appendResult(OperationResult::ok(QStringLiteral("Starting the active core because Global system proxy mode was enabled.")));
        startCore();
        return;
    }

    if (mode == SystemProxyMode::ForcedClear && isCoreRunning()) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping the active core because system proxy was cleared.")));
        cancelPendingCoreRestarts();
        QTimer::singleShot(0, mainWindow_.get(), [this]() {
            stopCore(false);
        });
        return;
    }

    syncStatusIndicators();
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
    systemProxyService_->update(
        mode,
        config_.localPort + 1,
        config_.localPort,
        buildSystemProxyExceptions(),
        config_.systemProxyAdvancedProtocol,
        config_.pacUrl);
    managedSystemProxyActive_ = expectedSystemProxyEnabled(mode);
}

void AppBootstrap::enableSystemProxy()
{
    setSystemProxyMode(SystemProxyMode::ForcedChange);
}

void AppBootstrap::disableSystemProxy()
{
    if (serverService_ == nullptr) {
        appendResult(OperationResult::fail(QStringLiteral("System proxy mode cannot be changed before the configuration service is ready.")));
        return;
    }

    const int previousValue = config_.sysProxyType;
    config_.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear);
    if (!serverService_->save(config_)) {
        config_.sysProxyType = previousValue;
        appendResult(OperationResult::fail(QStringLiteral("Failed to save the system proxy mode.")));
        syncStatusIndicators();
        return;
    }

    const bool success = systemProxyService_ == nullptr
        || systemProxyService_->update(
            SystemProxyMode::ForcedClear,
            config_.localPort + 1,
            config_.localPort,
            buildSystemProxyExceptions(),
            config_.systemProxyAdvancedProtocol,
            config_.pacUrl);
    appendResult(success
        ? OperationResult::ok(QStringLiteral("System proxy disabled."))
        : OperationResult::fail(QStringLiteral("Failed to disable system proxy.")));
    if (success) {
        managedSystemProxyActive_ = false;
    }
    syncStatusIndicators();
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

    if (!tryBeginBackgroundTask(BackgroundTaskKind::SubscriptionUpdate)) {
        return;
    }

    const QString startupMessage = QCoreApplication::translate(
        "AppBootstrap",
        "Importing clipboard content in the background...");
    if (mainWindow_ != nullptr) {
        mainWindow_->appendLog(startupMessage);
        mainWindow_->showTransientStatus(startupMessage, 3000);
        mainWindow_->setSubscriptionUpdateRunning(true);
    }
    subscriptionUpdateRunning_ = true;

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
            subscriptionUpdateRunning_ = false;
            finishBackgroundTask(BackgroundTaskKind::SubscriptionUpdate);
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

    if (!tryBeginBackgroundTask(BackgroundTaskKind::SubscriptionUpdate)) {
        return;
    }

    const QString activeSubscriptionId = resolveActiveServer() == nullptr
        ? QString()
        : resolveActiveServer()->subId.trimmed();
    if (mainWindow_ != nullptr) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Importing subscription URLs from the clipboard in the background...");
        mainWindow_->appendLog(message);
        mainWindow_->showTransientStatus(message, 3000);
    }

    for (auto& server : config_.servers) {
        server.testResult = QStringLiteral("...");
    }
    syncWindow();

    subscriptionUpdateRunning_ = true;
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
                subscriptionUpdateRunning_ = false;
                finishBackgroundTask(BackgroundTaskKind::SubscriptionUpdate);
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
            workerConfig.mainSelectedSubId = lastSubscriptionId;
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
            subscriptionUpdateRunning_ = false;
            finishBackgroundTask(BackgroundTaskKind::SubscriptionUpdate);
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
                restartCoreIfRunning(QStringLiteral("Reloading core after updating subscriptions."));
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

OperationResult AppBootstrap::importCustomConfigText(const QString& text)
{
    return importCustomConfigTextWithService(text, config_, *serverService_);
}

void AppBootstrap::updateAllSubscriptions()
{
    if (subscriptionUpdateRunning_) {
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
    for (int index = 0; index < config_.subscriptions.size(); ++index) {
        if (config_.subscriptions.at(index).id.trimmed() == trimmedSubscriptionId) {
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
    const QString activeSubscriptionId = resolveActiveServer() == nullptr
        ? QString()
        : resolveActiveServer()->subId.trimmed();
    const bool activeSubscriptionRemoved = !activeSubscriptionId.isEmpty() && activeSubscriptionId == normalizedId;

    const OperationResult result = subscriptionService_->removeSubscription(config_, normalizedId);
    appendResult(result);
    syncWindow();
    if (!result.success || !activeSubscriptionRemoved || !isCoreRunning()) {
        return;
    }

    if (resolveActiveServer() == nullptr) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping core because the active subscription was deleted.")));
        stopCore(false);
        return;
    }

    restartCoreIfRunning(QStringLiteral("Reloading core after deleting the active subscription."));
}

void AppBootstrap::runProxyAvailabilityCheck()
{
    if (mainWindow_ == nullptr) {
        return;
    }

    if (!tryBeginBackgroundTask(BackgroundTaskKind::ProxyAvailabilityCheck)) {
        return;
    }

    proxyAvailabilityCheckRunning_ = true;
    const Config workerConfig = config_;
    QObject* uiContext = mainWindow_.get();
    mainWindow_->showTransientStatus(
        QCoreApplication::translate("AppBootstrap", "Running availability check in the background..."),
        3000);

    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;
    QThread* thread = QThread::create([this, workerConfig, uiContext, lifetimeGuard]() {
        ProxyAvailabilityCheckService proxyAvailabilityCheckService;
        const OperationResult result = proxyAvailabilityCheckService.check(workerConfig);

        invokeOnUiThread(uiContext, [this, result, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            proxyAvailabilityCheckRunning_ = false;
            finishBackgroundTask(BackgroundTaskKind::ProxyAvailabilityCheck);
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

    if (!tryBeginBackgroundTask(BackgroundTaskKind::CoreUpdate)) {
        return;
    }

    const CoreType coreType = resolveRuntimeCoreType(static_cast<CoreType>(coreTypeValue));
    const QString title = QCoreApplication::translate("AppBootstrap", "Install / Update %1 Core")
                              .arg(coreTypeDisplayName(coreType));
    const QString installDirectory = resolveCoreInstallDirectory(coreType);
    const VmessItem* activeServer = resolveActiveServer();
    const bool shouldRestartRunningCore = isCoreRunning()
        && activeServer != nullptr
        && resolveRuntimeCoreType(activeServer->coreType) == coreType;
    const Config workerConfig = config_;
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
        finishBackgroundTask(BackgroundTaskKind::CoreUpdate);
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
        stopCore(false);
        return;
    }

    const QString startupMessage = QCoreApplication::translate("AppBootstrap", "Starting %1 update...")
                                       .arg(coreTypeDisplayName(coreType));
    mainWindow_->appendLog(startupMessage);
    mainWindow_->showTransientStatus(startupMessage, 3000);

    coreUpdateRunning_ = true;
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
    Config workerConfig,
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
            mainWindow_->showTransientStatus(message, 0);
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
    coreUpdateRunning_ = false;
    finishBackgroundTask(BackgroundTaskKind::CoreUpdate);
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
                mainWindow_->showTransientStatus(message, 0);
            }
            startCore();
        } else if (!result.success) {
            const QString message = QCoreApplication::translate(
                "AppBootstrap",
                "Update failed. Restoring the previous running core...");
            if (mainWindow_ != nullptr) {
                mainWindow_->appendLog(message);
                mainWindow_->showTransientStatus(message, 0);
            }
            startCore();
        }
    }

    if (!stoppedForInstall && startAfterSuccess && result.success) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Starting the downloaded core...");
        if (mainWindow_ != nullptr) {
            mainWindow_->appendLog(message);
            mainWindow_->showTransientStatus(message, 0);
        }
        startCore();
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
    const Config workerConfig = config_;
    const QString startupMessage = QCoreApplication::translate("AppBootstrap", "Starting %1 update...")
                                       .arg(coreTypeDisplayName(coreType));
    mainWindow_->appendLog(startupMessage);
    mainWindow_->showTransientStatus(startupMessage, 3000);

    coreUpdateRunning_ = true;
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

    if (!tryBeginBackgroundTask(BackgroundTaskKind::GeoUpdate)) {
        return;
    }

    geoUpdateRunning_ = true;
    const QString targetDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    QObject* uiContext = mainWindow_.get();
    const QString title = QCoreApplication::translate("AppBootstrap", "Update Geo Files");
    const QString message = QCoreApplication::translate("AppBootstrap", "Updating Geo resources in the background...");
    mainWindow_->appendLog(message);
    mainWindow_->showTransientStatus(message, 3000);

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
            geoUpdateRunning_ = false;
            finishBackgroundTask(BackgroundTaskKind::GeoUpdate);
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
        if (rowIndex < 0 || rowIndex >= config_.subscriptions.size()) {
            continue;
        }

        const QString subscriptionId = config_.subscriptions.at(rowIndex).id.trimmed();
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
    if (!tryBeginBackgroundTask(BackgroundTaskKind::SubscriptionUpdate)) {
        return;
    }

    const QString activeSubscriptionId = resolveActiveServer() == nullptr
        ? QString()
        : resolveActiveServer()->subId.trimmed();
    if (mainWindow_ != nullptr) {
        mainWindow_->appendLog(startupMessage);
        mainWindow_->showTransientStatus(startupMessage, 3000);
    }

    // Clear test results as loading indicator.
    for (auto& server : config_.servers) {
        server.testResult = QStringLiteral("...");
    }
    syncWindow();

    subscriptionUpdateRunning_ = true;
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
            subscriptionUpdateRunning_ = false;
            finishBackgroundTask(BackgroundTaskKind::SubscriptionUpdate);
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
                restartCoreIfRunning(QStringLiteral("Reloading core after updating subscriptions."));
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
        stopCore(true);
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

    if (systemProxyService_ != nullptr && normalizeSystemProxyMode(config_.sysProxyType) != SystemProxyMode::Unchanged) {
        const bool proxyUpdated = systemProxyService_->update(
            normalizeSystemProxyMode(config_.sysProxyType),
            config_.localPort + 1,
            config_.localPort,
            buildSystemProxyExceptions(),
            config_.systemProxyAdvancedProtocol);
        if (!proxyUpdated) {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Failed to reapply the restored system proxy settings.")));
        } else {
            managedSystemProxyActive_ = expectedSystemProxyEnabled(normalizeSystemProxyMode(config_.sysProxyType));
        }
    }

    if (wasCoreRunning && resolveActiveServer() != nullptr) {
        startCore();
    }

    appendResult(OperationResult::ok(
        QCoreApplication::translate("AppBootstrap", "Configuration restored from backup.")));
}

void AppBootstrap::openSettingsDialog(int initialTab)
{
    if (mainWindow_ == nullptr || serverService_ == nullptr) {
        return;
    }

    SettingsDialog dialog(mainWindow_.get());
    QPointer<SettingsDialog> dialogGuard(&dialog);
    dialog.selectTab(initialTab);
    dialog.setConfig(config_);
    refreshExistingCoreTypes();
    dialog.setExistingCoreTypes(existingCoreTypes_);
    const QString configPath = resolveConfigPath();
    const std::weak_ptr<char> lifetimeGuard = lifetimeGuard_;

    for (const CoreType coreType : availableCoreTypes()) {
        const QStringList candidates = resolveCoreCandidatesForConfigPath(coreType, configPath);
        auto* thread = QThread::create([coreType, candidates, dialogGuard, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }

            const QString version = readCoreVersion(coreType, locateFirstExistingFileInCandidates(candidates));
            if (!dialogGuard.isNull()) {
                QMetaObject::invokeMethod(dialogGuard.data(), [dialogGuard, coreType, version]() {
                    if (!dialogGuard.isNull()) {
                        dialogGuard->setCoreVersion(coreType, version);
                    }
                }, Qt::QueuedConnection);
            }
        });
        trackBackgroundThread(thread);
        thread->start();
    }

    QObject::connect(&dialog, &SettingsDialog::coreDownloadRequested, &dialog, [this, dialogGuard](int coreTypeValue) {
        if (dialogGuard.isNull()) {
            return;
        }

        const CoreType requestedCoreType = static_cast<CoreType>(coreTypeValue);
        const auto updateStarted = std::make_shared<bool>(false);
        updateCore(
            coreTypeValue,
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

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // Handle Restore Backup action - opens file picker but doesn't save settings
    if (dialog.wasRestoreBackupRequested()) {
        restoreConfigFromBackup();
        return;
    }

    // Handle Update Selected Subscriptions
    if (dialog.wasUpdateSubRequested()) {
        const QList<int> selectedRows = dialog.selectedSubRows();
        const OperationResult saveResult = subscriptionService_->saveSubscriptions(config_, dialog.config().subscriptions);
        appendResult(saveResult);
        if (saveResult.success) {
            syncWindow();
            updateSelectedSubscriptions(selectedRows);
        }
        dialog.clearUpdateSubRequested();
        return;
    }

    // Save settings from dialog
    const Config previousConfig = config_;
    Config updatedConfig = dialog.config();
    const auto normalizeLanguageCode = [](QString value) {
        value = value.trimmed().toLower();
        value.replace(QChar('-'), QChar('_'));
        return value;
    };
    const bool runningOnWindows = isWindowsPlatform();
    const bool processElevated = isProcessElevated();
    const TunSettingsSaveBehavior tunSaveBehavior = evaluateTunSettingsSaveBehavior(
        previousConfig,
        updatedConfig,
        runningOnWindows,
        processElevated,
        isCoreRunning());
    const TunSettingsApplyDecision& tunDecision = tunSaveBehavior.applyDecision;

    const bool runtimeSettingsChanged =
        shouldHotApplyRuntimeSettings(previousConfig, updatedConfig, tunDecision);
    const bool systemProxySettingsChanged = previousConfig.localPort != updatedConfig.localPort
        || previousConfig.systemProxyExceptions != updatedConfig.systemProxyExceptions
        || previousConfig.systemProxyAdvancedProtocol != updatedConfig.systemProxyAdvancedProtocol;
    const bool languageChanged =
        normalizeLanguageCode(previousConfig.languageCode) != normalizeLanguageCode(updatedConfig.languageCode);

    config_ = tunSaveBehavior.configToPersist;
    if (!serverService_->save(config_)) {
        config_ = previousConfig;
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to save settings.")));
        syncWindow();
        return;
    }

    appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Settings saved.")));
    if (tunSaveBehavior.shouldPromptForAdminRestart) {
        appendResult(OperationResult::fail(tunAdminRequiredSaveMessage()));
    }
    syncWindow();

    if (previousConfig.autoRunEnabled != updatedConfig.autoRunEnabled && autoRunService_ != nullptr) {
        const bool success = autoRunService_->setEnabled(updatedConfig.autoRunEnabled);
        if (!success) {
            appendResult(OperationResult::fail(QStringLiteral("Failed to update auto run setting.")));
        }
    }

    if (systemProxySettingsChanged
        && systemProxyService_ != nullptr
        && normalizeSystemProxyMode(config_.sysProxyType) != SystemProxyMode::Unchanged) {
        const bool proxyUpdated = systemProxyService_->update(
            normalizeSystemProxyMode(config_.sysProxyType),
            config_.localPort + 1,
            config_.localPort,
            buildSystemProxyExceptions(),
            config_.systemProxyAdvancedProtocol);
        if (!proxyUpdated) {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Failed to reapply system proxy settings.")));
        } else {
            managedSystemProxyActive_ = expectedSystemProxyEnabled(normalizeSystemProxyMode(config_.sysProxyType));
        }
    }

    if (runtimeSettingsChanged) {
        restartCoreIfRunning(QCoreApplication::translate(
            "AppBootstrap",
            "Reloading core after applying settings changes."));
    } else {
        syncStatusIndicators();
    }

    const bool adminRestartInitiated =
        tunSaveBehavior.shouldPromptForAdminRestart && promptRestartAsAdministratorForTun();

    if (shouldPromptForLanguageRestartAfterSettingsSave(languageChanged, adminRestartInitiated)) {
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

bool AppBootstrap::promptRestartAsAdministratorForTun()
{
    if (mainWindow_ == nullptr || !isWindowsPlatform() || isProcessElevated()) {
        return false;
    }

    if (DialogUtils::askYesNoQuestion(
            mainWindow_.get(),
            tunAdminRestartPromptTitle(),
            tunAdminRestartPromptMessage(),
            QMessageBox::Yes)
        != QMessageBox::Yes) {
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
        isWindowsPlatform() && !isProcessElevated() && config_.tunModeItem.enableTun;
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

    if (!tryBeginBackgroundTask(BackgroundTaskKind::SpeedTest)) {
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
        Config runtimeConfig = config_;

        items.append(SpeedTestRequestItem{
            *server,
            runtimeConfig,
            runtimeServer,
            resolveCoreInfo(runtimeServer)});
    }

    const OperationResult startResult = speedTestService_->start(config_, items);
    if (!startResult.success) {
        appendResult(startResult);
    }

    if (!startResult.success) {
        finishBackgroundTask(BackgroundTaskKind::SpeedTest);
    }

    if (startResult.success) {
        static const QString pending = QStringLiteral("...");
        QStringList pendingIds;
        for (const auto& item : items) {
            auto it = std::find_if(config_.servers.begin(), config_.servers.end(),
                [&item](const VmessItem& s) { return s.indexId == item.server.indexId; });
            if (it != config_.servers.end()) {
                it->testResult = pending;
                pendingIds.append(it->indexId);
            }
        }
        if (mainWindow_ != nullptr) {
            mainWindow_->updateServerTestResults(pendingIds, pending);
        }
    }
}

void AppBootstrap::trackBackgroundThread(QThread* thread)
{
    if (thread == nullptr) {
        return;
    }

    backgroundThreads_.append(thread);
    QObject* cleanupContext = mainWindow_ != nullptr
        ? static_cast<QObject*>(mainWindow_.get())
        : static_cast<QObject*>(QCoreApplication::instance());
    if (cleanupContext == nullptr) {
        return;
    }

    QObject::connect(thread, &QThread::finished, cleanupContext, [this, thread]() {
        backgroundThreads_.removeAll(thread);
        thread->deleteLater();
    });
}

void AppBootstrap::waitForBackgroundThreads()
{
    // Workers cooperatively check QThread::isInterruptionRequested() at their
    // iteration boundaries (subscription update, geo update) or via service-level
    // cancel flags. With a healthy worker they should bail within a few seconds;
    // the soft cap below catches a stuck worker so app shutdown is not blocked
    // indefinitely. We do NOT terminate() -- that would leak QNetworkAccessManager
    // / temp dir state -- the OS reaps the thread on process exit.
    constexpr unsigned long kShutdownWaitMs = 15000;
    const QList<QThread*> threads = backgroundThreads_;
    for (QThread* thread : threads) {
        if (thread == nullptr) {
            continue;
        }

        thread->requestInterruption();
        if (!thread->wait(kShutdownWaitMs)) {
            qWarning("AppBootstrap: background thread did not honor interruption within %lums; leaving it to the OS",
                kShutdownWaitMs);
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
    if (!configPath_.isEmpty()) {
        return configPath_;
    }

    const QString currentDirectoryPath = QDir::current().filePath(QStringLiteral("guiNConfig.json"));
    if (QFileInfo::exists(currentDirectoryPath)) {
        return currentDirectoryPath;
    }

    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("guiNConfig.json"));
}

const VmessItem* AppBootstrap::resolveActiveServer() const
{
    if (!config_.currentIndexId.trimmed().isEmpty()) {
        for (const VmessItem& item : config_.servers) {
            if (item.indexId == config_.currentIndexId) {
                return &item;
            }
        }
    }

    return config_.servers.isEmpty() ? nullptr : &config_.servers.constFirst();
}

const VmessItem* AppBootstrap::findServerById(const QString& indexId) const
{
    if (indexId.trimmed().isEmpty()) {
        return nullptr;
    }

    for (const VmessItem& item : config_.servers) {
        if (item.indexId == indexId) {
            return &item;
        }
    }

    return nullptr;
}

QString AppBootstrap::describeServer(const VmessItem* server) const
{
    if (server == nullptr) {
        return {};
    }

    if (!server->remarks.trimmed().isEmpty()) {
        return server->remarks.trimmed();
    }

    if (!server->address.trimmed().isEmpty() && server->port > 0) {
        return QStringLiteral("%1:%2").arg(server->address.trimmed()).arg(server->port);
    }

    return server->address.trimmed();
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

QString AppBootstrap::resolveStatisticsFilePath() const
{
    return QFileInfo(resolveConfigPath()).dir().filePath(QStringLiteral("StatisticLogOverall.json"));
}

int AppBootstrap::resolveStatisticsPort() const
{
    const QSet<int> reservedPorts{
        config_.localPort,
        config_.localPort + 1,
        config_.localPort + 2,
        config_.localPort + 3,
        config_.localPort + 103};

    for (int attempt = 0; attempt < 16; ++attempt) {
        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost, 0)) {
            return 0;
        }

        const int port = server.serverPort();
        server.close();
        if (port > 0 && !reservedPorts.contains(port)) {
            return port;
        }
    }

    return 0;
}

QString AppBootstrap::buildTrafficSummaryText() const
{
    if (statisticsService_ == nullptr) {
        return QCoreApplication::translate(
            "AppBootstrap",
            "Traffic: Speed D 0B/s / U 0B/s | Today D 0B / U 0B | Total D 0B / U 0B");
    }

    const ServerStatItem item = statisticsService_->currentServerStat();
    const StatisticsSessionState state = statisticsService_->sessionState();
    const quint64 refreshRateSeconds = static_cast<quint64>(qMax(1, state.refreshRateSeconds));
    const quint64 downSpeed = state.running && state.pollingAvailable
        ? state.lastDeltaDown / refreshRateSeconds
        : 0;
    const quint64 upSpeed = state.running && state.pollingAvailable
        ? state.lastDeltaUp / refreshRateSeconds
        : 0;

    return QCoreApplication::translate(
               "AppBootstrap",
               "Traffic: Speed D %1/s / U %2/s | Today D %3 / U %4 | Total D %5 / U %6")
        .arg(formatByteCount(downSpeed))
        .arg(formatByteCount(upSpeed))
        .arg(formatByteCount(item.todayDown))
        .arg(formatByteCount(item.todayUp))
        .arg(formatByteCount(item.totalDown))
        .arg(formatByteCount(item.totalUp));
}

QString AppBootstrap::buildStatisticsSummaryText() const
{
    if (statisticsService_ == nullptr || !config_.enableStatistics) {
        return QCoreApplication::translate("AppBootstrap", "Stats: Off");
    }
    const StatisticsSessionState state = statisticsService_->sessionState();
    if (!state.running) {
        return QCoreApplication::translate("AppBootstrap", "Stats: Idle");
    }
    if (state.externallyManaged) {
        return QCoreApplication::translate("AppBootstrap", "Stats: External");
    }
    if (state.runtimeConfigReady && state.statePort > 0 && state.pollingAvailable) {
        return QCoreApplication::translate("AppBootstrap", "Stats API 127.0.0.1:%1").arg(state.statePort);
    }
    if (state.runtimeConfigReady && state.statePort > 0) {
        return QCoreApplication::translate("AppBootstrap", "Stats API 127.0.0.1:%1 (waiting)").arg(state.statePort);
    }

    return QCoreApplication::translate("AppBootstrap", "Stats: Enabled");
}

QString AppBootstrap::buildRoutingSummaryText() const
{
    if (!config_.enableRoutingAdvanced || config_.routingItems.isEmpty()) {
        return QCoreApplication::translate("AppBootstrap", "Basic");
    }

    if (config_.routingIndex >= 0 && config_.routingIndex < config_.routingItems.size()) {
        const QString remarks = config_.routingItems.at(config_.routingIndex).remarks.trimmed();
        if (!remarks.isEmpty()) {
            return QCoreApplication::translate("AppBootstrap", "%1 (Advanced)").arg(remarks);
        }

        return QCoreApplication::translate("AppBootstrap", "Routing %1 (Advanced)").arg(config_.routingIndex + 1);
    }

    return QCoreApplication::translate("AppBootstrap", "Advanced");
}

QString AppBootstrap::buildListenSummaryText() const
{
    const QString listenHost = config_.allowLanConnection
        ? QStringLiteral("0.0.0.0")
        : QStringLiteral("127.0.0.1");
    if (config_.localPort <= 0) {
        return QCoreApplication::translate("AppBootstrap", "Unavailable");
    }

    return QCoreApplication::translate("AppBootstrap", "SOCKS %1:%2 | HTTP %1:%3")
        .arg(listenHost)
        .arg(config_.localPort)
        .arg(config_.localPort + 1);
}

QString AppBootstrap::buildSystemProxyExceptions() const
{
    QStringList entries = splitProxyExceptions(
        config_.defIeProxyExceptions.trimmed().isEmpty()
            ? QString::fromUtf8(DefaultIeProxyExceptions)
            : config_.defIeProxyExceptions.trimmed());

    for (const QString& item : splitProxyExceptions(config_.systemProxyExceptions.trimmed())) {
        appendUniqueProxyException(entries, item);
    }

    for (const QString& item : collectRouteDerivedProxyExceptions(config_)) {
        appendUniqueProxyException(entries, item);
    }

    return entries.join(QStringLiteral(";"));
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

