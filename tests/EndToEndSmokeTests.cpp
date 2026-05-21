#include <QtTest>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "common/SystemProxyMode.h"
#include "common/UserAgent.h"
#include "persistence/JsonConfigRepository.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/CoreLifecycleService.h"
#include "runtime/ProtocolCoreCompat.h"
#include "runtime/QtCoreProcessHost.h"
#include "services/CoreUpdateService.h"
#include "services/GeoResourceUpdateService.h"
#include "services/ProxyAvailabilityCheckService.h"
#include "services/SpeedTestServiceInternal.h"
#include "services/SubscriptionService.h"
#include "services/SubscriptionUpdateService.h"

#if defined(Q_OS_WIN)
#include "platform/windows/WindowsSystemProxyService.h"

#include <windows.h>
#include <wininet.h>
#endif

class EndToEndSmokeTests : public QObject {
    Q_OBJECT

private slots:
    void subscriptionToGoogleSmoke();
};

namespace {

constexpr auto kSmokeEnabledEnv = "SONGBIRD_RUN_E2E_SMOKE";
constexpr auto kSmokeSubscriptionUrlEnv = "SONGBIRD_SMOKE_SUB_URL";
constexpr auto kSmokeTargetUrlEnv = "SONGBIRD_SMOKE_TARGET_URL";
constexpr auto kSmokeSpeedUrlEnv = "SONGBIRD_SMOKE_SPEED_URL";
constexpr auto kSmokeMaxServersEnv = "SONGBIRD_SMOKE_MAX_SERVERS";
constexpr auto kSmokeServerOffsetEnv = "SONGBIRD_SMOKE_SERVER_OFFSET";
constexpr auto kSmokeReportPathEnv = "SONGBIRD_SMOKE_REPORT_PATH";
constexpr int kDefaultMaxServers = (std::numeric_limits<int>::max)();
constexpr int kCoreReadyTimeoutMs = 10000;
constexpr int kHttpProbeTimeoutMs = 30000;
constexpr int kPortReadyTimeoutMs = 5000;

const QString kLoopbackAddress = QStringLiteral("127.0.0.1");
const QString kDefaultTargetUrl = QStringLiteral("https://google.com");

QString envString(const char* name)
{
    return QString::fromLocal8Bit(qgetenv(name)).trimmed();
}

bool envFlagEnabled(const char* name)
{
    const QString value = envString(name).toLower();
    return value == QStringLiteral("1")
        || value == QStringLiteral("true")
        || value == QStringLiteral("yes")
        || value == QStringLiteral("on");
}

int envInt(const char* name, int fallback)
{
    bool ok = false;
    const int value = envString(name).toInt(&ok);
    return ok && value > 0 ? value : fallback;
}

QString testTargetUrl()
{
    const QString value = envString(kSmokeTargetUrlEnv);
    return value.isEmpty() ? kDefaultTargetUrl : value;
}

QString testSpeedUrl()
{
    const QString value = envString(kSmokeSpeedUrlEnv);
    return value.isEmpty() ? QStringLiteral("https://www.gstatic.com/generate_204") : value;
}

QString smokeReportPath()
{
    const QString value = envString(kSmokeReportPathEnv);
    if (!value.isEmpty()) {
        return QFileInfo(value).absoluteFilePath();
    }
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("end-to-end-smoke-report.txt"));
}

QString smokeProxyExceptions()
{
    return QStringLiteral(
        "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;"
        "172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*");
}

void configureSmokeConsoleLogging()
{
    QLoggingCategory::setFilterRules(QStringLiteral(
        "qt.network.monitor.info=false\n"
        "qt.network.monitor.warning=false"));
}

int takeAvailableSmokePortBase()
{
    for (int attempt = 0; attempt < 64; ++attempt) {
        QTcpServer socksProbe;
        if (!socksProbe.listen(QHostAddress::LocalHost, 0)) {
            continue;
        }

        const int candidate = socksProbe.serverPort();
        if (candidate <= 0 || candidate >= 65535) {
            continue;
        }

        QTcpServer httpProbe;
        if (!httpProbe.listen(QHostAddress::LocalHost, candidate + 1)) {
            continue;
        }

        socksProbe.close();
        httpProbe.close();
        return candidate;
    }

    return 0;
}

CoreInfo makeSingBoxCoreInfo(const QString& program)
{
    CoreInfo info;
    info.program = program;
    info.workingDirectory = QFileInfo(program).absolutePath();
    info.arguments = QStringList{QStringLiteral("run"), QStringLiteral("-c"), info.configPlaceholder};
    info.appendConfigArgument = false;
    return info;
}

QString findSingBoxExecutable(const QString& directoryPath)
{
    const QStringList names{
        QStringLiteral("sing-box-client.exe"),
        QStringLiteral("sing-box.exe")};
    for (const QString& name : names) {
        const QString path = QDir(directoryPath).filePath(name);
        if (QFileInfo::exists(path)) {
            return QFileInfo(path).absoluteFilePath();
        }
    }
    return {};
}

void addSingBoxPolicy(Config& config)
{
    QList<CoreTypeItem> items;
    for (const ConfigType configType : configurableCoreProtocols()) {
        items.append(CoreTypeItem{
            static_cast<int>(configType),
            static_cast<int>(CoreType::SingBox)});
    }
    config.policy().coreTypeItems = items;
}

Config createSmokeConfig()
{
    Config config;
    config.localProtocol = QStringLiteral("socks");
    config.udpEnabled = true;
    config.sniffingEnabled = true;
    config.routeOnly = false;
    config.allowLanConnection = false;
    config.logEnabled = true;
    config.logLevel = QStringLiteral("debug");
    config.defaults().speedPingTestUrl = testSpeedUrl();
    config.dns().enableCacheFile4Sbox = false;
    config.dns().remoteDns = QStringLiteral("https://cloudflare-dns.com/dns-query");
    config.dns().directDns = QStringLiteral("https://dns.alidns.com/dns-query");
    config.dns().bootstrapDns = QStringLiteral("223.5.5.5");
    config.ignoreGeoUpdateCore = true;
    addSingBoxPolicy(config);
    return config;
}

std::optional<double> parseLatencyMs(const QString& value)
{
    double latency = 0.0;
    if (SpeedTestServiceInternal::tryParseUrlProbeLatency(value, latency)) {
        return latency;
    }
    return std::nullopt;
}

QString serverDisplayName(const VmessItem& server)
{
    return server.remarks.trimmed().isEmpty()
        ? QStringLiteral("%1:%2").arg(server.address).arg(server.port)
        : server.remarks.trimmed();
}

QString compactSingleLine(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return value.trimmed();
}

QString truncateForLog(const QString& value, int maxLength = 320)
{
    const QString compact = compactSingleLine(value);
    if (compact.size() <= maxLength) {
        return compact;
    }
    return compact.left(maxLength - 3) + QStringLiteral("...");
}

QString summarizeProbeFailure(const QString& detail)
{
    if (detail.contains(QStringLiteral("unexpected HTTP response status: 521"))) {
        return QStringLiteral("Proxy probe failed: upstream returned HTTP 521");
    }
    if (detail.contains(QStringLiteral("unexpected HTTP response status: 530"))) {
        return QStringLiteral("Proxy probe failed: upstream returned HTTP 530");
    }
    if (detail.contains(QStringLiteral("authentication failed"), Qt::CaseInsensitive)) {
        return QStringLiteral("Proxy probe failed: upstream authentication failed");
    }
    if (detail.contains(QStringLiteral("tls: handshake failure"), Qt::CaseInsensitive)) {
        return QStringLiteral("Proxy probe failed: TLS handshake failure");
    }
    if (detail.contains(QStringLiteral("i/o timeout"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("timed out"), Qt::CaseInsensitive)) {
        return QStringLiteral("Proxy probe failed: timeout");
    }
    if (detail.contains(QStringLiteral("Connection closed"), Qt::CaseInsensitive)) {
        return QStringLiteral("Proxy probe failed: connection closed");
    }
    return truncateForLog(detail);
}

QString summarizeFailureCategories(const QList<VmessItem>& servers)
{
    int success = 0;
    int http521 = 0;
    int http530 = 0;
    int authentication = 0;
    int tls = 0;
    int timeout = 0;
    int closed = 0;
    int other = 0;

    for (const VmessItem& server : servers) {
        const QString result = server.testResult.trimmed();
        if (result.isEmpty()) {
            continue;
        }
        if (parseLatencyMs(result).has_value()) {
            ++success;
        } else if (result.contains(QStringLiteral("HTTP 521"))) {
            ++http521;
        } else if (result.contains(QStringLiteral("HTTP 530"))) {
            ++http530;
        } else if (result.contains(QStringLiteral("authentication"), Qt::CaseInsensitive)) {
            ++authentication;
        } else if (result.contains(QStringLiteral("TLS"), Qt::CaseInsensitive)) {
            ++tls;
        } else if (result.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)) {
            ++timeout;
        } else if (result.contains(QStringLiteral("closed"), Qt::CaseInsensitive)) {
            ++closed;
        } else {
            ++other;
        }
    }

    QStringList parts;
    if (success > 0) {
        parts.append(QStringLiteral("usable=%1").arg(success));
    }
    if (http521 > 0) {
        parts.append(QStringLiteral("http521=%1").arg(http521));
    }
    if (http530 > 0) {
        parts.append(QStringLiteral("http530=%1").arg(http530));
    }
    if (authentication > 0) {
        parts.append(QStringLiteral("auth=%1").arg(authentication));
    }
    if (tls > 0) {
        parts.append(QStringLiteral("tls=%1").arg(tls));
    }
    if (timeout > 0) {
        parts.append(QStringLiteral("timeout=%1").arg(timeout));
    }
    if (closed > 0) {
        parts.append(QStringLiteral("closed=%1").arg(closed));
    }
    if (other > 0) {
        parts.append(QStringLiteral("other=%1").arg(other));
    }
    return parts.isEmpty() ? QStringLiteral("none") : parts.join(QStringLiteral(", "));
}

bool waitForLocalPortReady(int port, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QTcpSocket socket;
        socket.connectToHost(kLoopbackAddress, port);
        if (socket.waitForConnected(200)) {
            socket.disconnectFromHost();
            return true;
        }
        QCoreApplication::processEvents();
    }
    return false;
}

OperationResult probeUrlViaHttpProxy(const QString& url, int httpPort, int timeoutMs, qint64* elapsedMs = nullptr)
{
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, kLoopbackAddress, httpPort));

    QNetworkRequest request(QUrl::fromUserInput(url));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, fallbackUserAgent());

    QElapsedTimer timer;
    timer.start();
    bool timedOut = false;
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(timeoutMs);
    loop.exec();
    timeoutTimer.stop();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorText = reply->errorString();
    const qint64 elapsed = timer.elapsed();
    reply->deleteLater();
    if (elapsedMs != nullptr) {
        *elapsedMs = elapsed;
    }

    if (!timedOut && statusCode >= 200 && statusCode < 400) {
        return OperationResult::ok(QStringLiteral("Fetched %1 via proxy in %2 ms (HTTP %3).")
                                       .arg(url)
                                       .arg(elapsed)
                                       .arg(statusCode));
    }

    if (timedOut) {
        return OperationResult::fail(QStringLiteral("Fetching %1 via proxy timed out.").arg(url));
    }

    return OperationResult::fail(QStringLiteral("Fetching %1 via proxy failed: HTTP %2, %3")
                                     .arg(url)
                                     .arg(statusCode)
                                     .arg(errorText));
}

OperationResult probeUrlViaSystemProxy(const QString& url, int timeoutMs)
{
    const bool previousUseSystemConfiguration = QNetworkProxyFactory::usesSystemConfiguration();
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl::fromUserInput(url));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, fallbackUserAgent());

    QElapsedTimer timer;
    timer.start();
    bool timedOut = false;
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(timeoutMs);
    loop.exec();
    timeoutTimer.stop();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorText = reply->errorString();
    reply->deleteLater();
    QNetworkProxyFactory::setUseSystemConfiguration(previousUseSystemConfiguration);

    if (!timedOut && statusCode >= 200 && statusCode < 400) {
        return OperationResult::ok(QStringLiteral("Fetched %1 via system proxy in %2 ms (HTTP %3).")
                                       .arg(url)
                                       .arg(timer.elapsed())
                                       .arg(statusCode));
    }

    if (timedOut) {
        return OperationResult::fail(QStringLiteral("Fetching %1 via system proxy timed out.").arg(url));
    }

    return OperationResult::fail(QStringLiteral("Fetching %1 via system proxy failed: HTTP %2, %3")
                                     .arg(url)
                                     .arg(statusCode)
                                     .arg(errorText));
}

QList<VmessItem> compatibleServers(const QList<VmessItem>& servers, int maxCount, int offset)
{
    QList<VmessItem> result;
    result.reserve(qMin(maxCount, servers.size()));
    int skipped = 0;
    for (VmessItem server : servers) {
        const QString address = server.address.trimmed();
        if (address.isEmpty() || server.port <= 0 || server.port > 65535) {
            continue;
        }
        if (address == QStringLiteral("127.0.0.1")
            || address.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        static const QRegularExpression plausibleHostPattern(
            QStringLiteral(R"(^(\[[0-9A-Fa-f:.]+\]|[0-9A-Fa-f:.]+|[A-Za-z0-9][A-Za-z0-9.-]*[A-Za-z0-9])$)"));
        if (!plausibleHostPattern.match(address).hasMatch()) {
            continue;
        }
        if (server.configType == ConfigType::Custom || server.configType == ConfigType::Unknown) {
            continue;
        }
        if (!protocolSupportsCore(server.configType, CoreType::SingBox)) {
            continue;
        }
        if (skipped < offset) {
            ++skipped;
            continue;
        }
        server.coreType = CoreType::SingBox;
        result.append(server);
        if (result.size() >= maxCount) {
            break;
        }
    }
    return result;
}

VmessItem selectFastestServer(const Config& config)
{
    const VmessItem* bestServer = nullptr;
    double bestLatency = 0.0;
    for (const VmessItem& server : config.collection().servers) {
        const std::optional<double> latency = parseLatencyMs(server.testResult);
        if (!latency.has_value()) {
            continue;
        }
        if (bestServer == nullptr || latency.value() < bestLatency) {
            bestServer = &server;
            bestLatency = latency.value();
        }
    }
    return bestServer == nullptr ? VmessItem{} : *bestServer;
}

QString summarizeSmokeSpeedResults(const QList<VmessItem>& servers)
{
    QStringList lines;
    int recordedCount = 0;
    for (const VmessItem& server : servers) {
        if (server.testResult.trimmed().isEmpty()) {
            continue;
        }
        ++recordedCount;
        lines.append(QStringLiteral("%1 => %2")
                         .arg(serverDisplayName(server).left(64))
                         .arg(server.testResult.trimmed()));
        if (lines.size() >= 16) {
            break;
        }
    }
    if (recordedCount > lines.size()) {
        lines.append(QStringLiteral("... %1 more recorded results in the smoke report").arg(recordedCount - lines.size()));
    }
    return lines.isEmpty() ? QStringLiteral("No per-server results were recorded.") : lines.join(QStringLiteral("; "));
}

class SmokeReport {
public:
    explicit SmokeReport(QString path)
        : path_(std::move(path))
        , file_(path_)
    {
    }

    bool open(QString* error)
    {
        const QFileInfo info(path_);
        QDir dir(info.absolutePath());
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            if (error != nullptr) {
                *error = QStringLiteral("Failed to create smoke report directory: %1").arg(info.absolutePath());
            }
            return false;
        }
        if (!file_.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (error != nullptr) {
                *error = QStringLiteral("Failed to open smoke report: %1").arg(file_.errorString());
            }
            return false;
        }
        return true;
    }

    void append(const QString& line)
    {
        const QString stamped = QStringLiteral("[%1] %2\n")
                                    .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs), line);
        file_.write(stamped.toUtf8());
        file_.flush();
    }

    QString path() const
    {
        return path_;
    }

private:
    QString path_;
    QFile file_;
};

#if defined(Q_OS_WIN)
class ScopedSystemProxyRestore {
public:
    ScopedSystemProxyRestore()
        : settings_(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"),
              QSettings::NativeFormat)
        , proxyEnable_(settings_.value(QStringLiteral("ProxyEnable")))
        , proxyServer_(settings_.value(QStringLiteral("ProxyServer")))
        , proxyOverride_(settings_.value(QStringLiteral("ProxyOverride")))
        , autoConfigUrl_(settings_.value(QStringLiteral("AutoConfigURL")))
        , hadProxyEnable_(settings_.contains(QStringLiteral("ProxyEnable")))
        , hadProxyServer_(settings_.contains(QStringLiteral("ProxyServer")))
        , hadProxyOverride_(settings_.contains(QStringLiteral("ProxyOverride")))
        , hadAutoConfigUrl_(settings_.contains(QStringLiteral("AutoConfigURL")))
    {
    }

    ~ScopedSystemProxyRestore()
    {
        restoreValue(QStringLiteral("ProxyEnable"), hadProxyEnable_, proxyEnable_);
        restoreValue(QStringLiteral("ProxyServer"), hadProxyServer_, proxyServer_);
        restoreValue(QStringLiteral("ProxyOverride"), hadProxyOverride_, proxyOverride_);
        restoreValue(QStringLiteral("AutoConfigURL"), hadAutoConfigUrl_, autoConfigUrl_);
        settings_.sync();
        InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
        InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
    }

private:
    void restoreValue(const QString& key, bool existed, const QVariant& value)
    {
        if (existed) {
            settings_.setValue(key, value);
        } else {
            settings_.remove(key);
        }
    }

    QSettings settings_;
    QVariant proxyEnable_;
    QVariant proxyServer_;
    QVariant proxyOverride_;
    QVariant autoConfigUrl_;
    bool hadProxyEnable_ = false;
    bool hadProxyServer_ = false;
    bool hadProxyOverride_ = false;
    bool hadAutoConfigUrl_ = false;
};
#endif

class ScopedCoreStop {
public:
    explicit ScopedCoreStop(CoreLifecycleService& service)
        : service_(service)
    {
    }

    ~ScopedCoreStop()
    {
        service_.stop(true);
    }

private:
    CoreLifecycleService& service_;
};

struct SmokeLatencyResult {
    bool success = false;
    qint64 latencyMs = -1;
    QString message;
    QString detail;
};

SmokeLatencyResult testServerLatency(
    const Config& config,
    const VmessItem& server,
    const QString& singBoxPath,
    const QString& workDirectory,
    int sequence)
{
    const QString runtimeConfigPath = QDir(workDirectory).filePath(
        QStringLiteral("probe-%1.json").arg(sequence));

    ClientConfigWriter configWriter(workDirectory);
    configWriter.setExistingCoreTypes(QList<CoreType>{CoreType::SingBox});
    const OperationResult writeResult = configWriter.writeClientConfig(config, server, runtimeConfigPath);
    if (!writeResult.success) {
        return SmokeLatencyResult{false, -1, writeResult.message};
    }

    QtCoreProcessHost processHost;
    CoreLifecycleService coreLifecycle(processHost);
    QStringList coreOutput;
    QObject::connect(&coreLifecycle, &CoreLifecycleService::outputReceived, &coreLifecycle, [&coreOutput](const QString& line) {
        if (coreOutput.size() >= 8) {
            coreOutput.removeFirst();
        }
        coreOutput.append(line);
    });

    QSignalSpy coreStartedSpy(&coreLifecycle, SIGNAL(started(QString)));
    const OperationResult startResult = coreLifecycle.start(makeSingBoxCoreInfo(singBoxPath), runtimeConfigPath);
    if (!startResult.success) {
        return SmokeLatencyResult{false, -1, startResult.message};
    }

    ScopedCoreStop stopCore(coreLifecycle);
    if (coreStartedSpy.isEmpty() && !coreStartedSpy.wait(kCoreReadyTimeoutMs)) {
        return SmokeLatencyResult{false, -1, QStringLiteral("Timed out while waiting for the probe core to start.")};
    }
    if (!waitForLocalPortReady(config.localPort + 1, kPortReadyTimeoutMs)) {
        return SmokeLatencyResult{false, -1, QStringLiteral("Probe HTTP inbound did not become ready.")};
    }

    qint64 latencyMs = -1;
    const OperationResult probeResult = probeUrlViaHttpProxy(
        testSpeedUrl(),
        config.localPort + 1,
        kHttpProbeTimeoutMs,
        &latencyMs);
    if (probeResult.success) {
        return SmokeLatencyResult{true, latencyMs, probeResult.message};
    }

    coreLifecycle.stop(true);
    QCoreApplication::processEvents();
    const QString output = coreOutput.isEmpty()
        ? QString()
        : QStringLiteral(" Core output: %1").arg(coreOutput.join(QStringLiteral(" | ")));
    const QString detail = probeResult.message + output;
    return SmokeLatencyResult{false, -1, summarizeProbeFailure(detail), truncateForLog(detail, 1600)};
}

} // namespace

void EndToEndSmokeTests::subscriptionToGoogleSmoke()
{
#if !defined(Q_OS_WIN)
    QSKIP("The end-to-end smoke test currently exercises Windows system proxy integration.");
#else
    if (!envFlagEnabled(kSmokeEnabledEnv)) {
        QSKIP("Set SONGBIRD_RUN_E2E_SMOKE=1 to run the real end-to-end smoke test.");
    }
    configureSmokeConsoleLogging();

    const QString subscriptionUrl = envString(kSmokeSubscriptionUrlEnv);
    if (subscriptionUrl.isEmpty()) {
        QSKIP("Set SONGBIRD_SMOKE_SUB_URL to a real subscription URL before running the smoke test.");
    }

    QTemporaryDir workDir;
    QVERIFY2(workDir.isValid(), "Failed to create smoke test working directory.");

    SmokeReport report(smokeReportPath());
    QString reportError;
    QVERIFY2(report.open(&reportError), qPrintable(reportError));
    qInfo().noquote() << QStringLiteral("End-to-end smoke report: %1").arg(QDir::toNativeSeparators(report.path()));
    report.append(QStringLiteral("Smoke run started. target=%1 speedProbe=%2")
                      .arg(testTargetUrl(), testSpeedUrl()));
    qInfo().noquote() << QStringLiteral("Preparing sing-box, geo resources, and subscription data...");

    const QString configPath = workDir.filePath(QStringLiteral("songbird.json"));
    const QString runtimeConfigPath = workDir.filePath(QStringLiteral("runtime.json"));
    const QString installDirectory = workDir.path();
    JsonConfigRepository repository(configPath);
    SubscriptionService subscriptionService(repository);
    QNetworkAccessManager networkAccessManager;
    SubscriptionUpdateService subscriptionUpdateService(repository, subscriptionService, networkAccessManager);

    Config config = createSmokeConfig();
    config.collection().subscriptions.append(SubItem{
        QStringLiteral("smoke-sub"),
        QStringLiteral("Smoke subscription"),
        subscriptionUrl,
        true,
        {}});
    QVERIFY2(repository.save(config), "Failed to save the initial smoke test config.");

    CoreUpdateService coreUpdateService;
    CoreUpdateService::UpdateOptions coreUpdateOptions;
    coreUpdateOptions.skipLocalVersionCheck = true;
    coreUpdateOptions.progressHandler = [&report](const QString& message) {
        report.append(QStringLiteral("core-update: %1").arg(compactSingleLine(message)));
    };
    const OperationResult coreUpdateResult = coreUpdateService.update(
        CoreType::SingBox,
        CoreUpdateConfig{false, true},
        installDirectory,
        coreUpdateOptions);
    QVERIFY2(coreUpdateResult.success, qPrintable(coreUpdateResult.message));

    const QString singBoxPath = findSingBoxExecutable(installDirectory);
    QVERIFY2(!singBoxPath.isEmpty(), "sing-box executable was not installed by the core update step.");

    GeoResourceUpdateService geoResourceUpdateService(installDirectory);
    const OperationResult geositeResult = geoResourceUpdateService.update(QStringLiteral("geosite"));
    QVERIFY2(geositeResult.success, qPrintable(geositeResult.message));
    const OperationResult geoipResult = geoResourceUpdateService.update(QStringLiteral("geoip"));
    QVERIFY2(geoipResult.success, qPrintable(geoipResult.message));

    OperationResult updateResult = subscriptionUpdateService.updateByIds(
        config,
        QStringList{QStringLiteral("smoke-sub")},
        false);
    QVERIFY2(updateResult.success, qPrintable(updateResult.message));
    QVERIFY2(!config.collection().servers.isEmpty(), "The subscription update imported no servers.");
    const QString importLine = QStringLiteral("Imported %1 servers from subscription.")
                                   .arg(config.collection().servers.size());
    report.append(importLine);

    const int maxServers = envInt(kSmokeMaxServersEnv, kDefaultMaxServers);
    const int serverOffset = qMax(0, envInt(kSmokeServerOffsetEnv, 0));
    const QList<VmessItem> testServers = compatibleServers(config.collection().servers, maxServers, serverOffset);
    QVERIFY2(!testServers.isEmpty(), "The imported subscription has no sing-box-compatible servers for smoke testing.");
    const QString testLine = QStringLiteral("Testing %1 sing-box-compatible servers from offset %2.")
                                 .arg(testServers.size())
                                 .arg(serverOffset);
    report.append(testLine);
    qInfo().noquote() << QStringLiteral("%1 %2").arg(importLine, testLine);

    int testedCount = 0;
    for (const VmessItem& testServer : testServers) {
        Config probeConfig = config;
        probeConfig.localPort = takeAvailableSmokePortBase();
        QVERIFY2(probeConfig.localPort > 0, "Failed to reserve local proxy ports for smoke URL test.");

        const QString name = serverDisplayName(testServer);
        const SmokeLatencyResult latencyResult = testServerLatency(
            probeConfig,
            testServer,
            singBoxPath,
            workDir.path(),
            testedCount);
        ++testedCount;

        const QString result = latencyResult.success
            ? QStringLiteral("%1 ms").arg(latencyResult.latencyMs)
            : latencyResult.message;
        report.append(QStringLiteral("[%1/%2] %3 -> %4")
                          .arg(testedCount)
                          .arg(testServers.size())
                          .arg(name)
                          .arg(result));
        if (!latencyResult.detail.isEmpty()) {
            report.append(QStringLiteral("[%1/%2] detail: %3")
                              .arg(testedCount)
                              .arg(testServers.size())
                              .arg(latencyResult.detail));
        }
        for (VmessItem& server : config.collection().servers) {
            if (server.indexId == testServer.indexId) {
                server.testResult = result;
                break;
            }
        }

        if (latencyResult.success) {
            const QString successLine = QStringLiteral("Found usable server after testing %1 server(s): %2 (%3)")
                                            .arg(testedCount)
                                            .arg(name)
                                            .arg(result);
            report.append(successLine);
            qInfo().noquote() << successLine;
            break;
        }
    }

    VmessItem fastestServer = selectFastestServer(config);
    const QString summaryLine = QStringLiteral("Smoke test summary: tested=%1/%2, categories={%3}")
                                    .arg(testedCount)
                                    .arg(testServers.size())
                                    .arg(summarizeFailureCategories(config.collection().servers));
    report.append(summaryLine);
    qInfo().noquote() << summaryLine;
    QVERIFY2(
        !fastestServer.indexId.trimmed().isEmpty(),
        qPrintable(QStringLiteral("No tested server returned a usable latency after testing %1/%2 compatible servers. Report: %3. Categories: %4. Results: %5")
                       .arg(testedCount)
                       .arg(testServers.size())
                       .arg(QDir::toNativeSeparators(report.path()))
                       .arg(summarizeFailureCategories(config.collection().servers))
                       .arg(summarizeSmokeSpeedResults(config.collection().servers))));
    config.currentIndexId = fastestServer.indexId;
    config.localPort = takeAvailableSmokePortBase();
    QVERIFY2(config.localPort > 0, "Failed to reserve local proxy ports for the final smoke proxy.");
    QVERIFY2(repository.save(config), "Failed to persist the fastest smoke-test server.");
    const QString selectedLine = QStringLiteral("Selected server for final proxy: %1")
                                     .arg(serverDisplayName(fastestServer));
    report.append(selectedLine);
    qInfo().noquote() << selectedLine;

    ClientConfigWriter configWriter(workDir.path());
    configWriter.setExistingCoreTypes(QList<CoreType>{CoreType::SingBox});
    const OperationResult writeResult = configWriter.writeClientConfig(config, fastestServer, runtimeConfigPath);
    QVERIFY2(writeResult.success, qPrintable(writeResult.message));

    QtCoreProcessHost processHost;
    CoreLifecycleService coreLifecycle(processHost);
    QSignalSpy coreStartedSpy(&coreLifecycle, SIGNAL(started(QString)));
    const OperationResult startResult = coreLifecycle.start(makeSingBoxCoreInfo(singBoxPath), runtimeConfigPath);
    QVERIFY2(startResult.success, qPrintable(startResult.message));

    ScopedCoreStop stopCore(coreLifecycle);
    if (coreStartedSpy.isEmpty()) {
        QVERIFY2(coreStartedSpy.wait(kCoreReadyTimeoutMs), "Timed out while waiting for the core process to start.");
    }
    QVERIFY2(waitForLocalPortReady(config.localPort + 1, kPortReadyTimeoutMs), "Final HTTP inbound did not become ready.");

    ProxyAvailabilityCheckService availabilityCheckService;
    ProxyAvailabilityCheckConfig availabilityConfig;
    availabilityConfig.localPort = config.localPort;
    availabilityConfig.tunEnabled = false;
    availabilityConfig.speedPingTestUrl = config.defaults().speedPingTestUrl;
    const OperationResult availabilityResult = availabilityCheckService.check(availabilityConfig);
    QVERIFY2(availabilityResult.success, qPrintable(availabilityResult.message));

    ScopedSystemProxyRestore restoreSystemProxy;
    WindowsSystemProxyService systemProxyService;
    QVERIFY2(
        systemProxyService.update(
            SystemProxyMode::ForcedChange,
            config.localPort + 1,
            config.localPort,
            smokeProxyExceptions(),
            config.systemProxyAdvancedProtocol),
        "Failed to enable the Windows system proxy.");

    const OperationResult targetProbe = probeUrlViaHttpProxy(testTargetUrl(), config.localPort + 1, kHttpProbeTimeoutMs);
    QVERIFY2(targetProbe.success, qPrintable(targetProbe.message));
    report.append(targetProbe.message);
    qInfo().noquote() << targetProbe.message;

    const OperationResult systemProxyProbe = probeUrlViaSystemProxy(testTargetUrl(), kHttpProbeTimeoutMs);
    QVERIFY2(systemProxyProbe.success, qPrintable(systemProxyProbe.message));
    report.append(systemProxyProbe.message);
    qInfo().noquote() << systemProxyProbe.message;
#endif
}

QTEST_MAIN(EndToEndSmokeTests)

#include "EndToEndSmokeTests.moc"


