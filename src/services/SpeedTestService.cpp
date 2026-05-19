#include "services/SpeedTestService.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QFile>

#include <algorithm>
#include <future>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "runtime/ClientConfigWriter.h"
#include "services/SpeedTestServiceInternal.h"

namespace {

constexpr int kRuntimeStartupTimeoutMs = 5000;
constexpr int kRuntimeRequestTimeoutMs = 3000;
constexpr int kUrlTestMaxConcurrency = 32;
// Per-item fallback spawns a full core per server; running too many in
// parallel just contends on disk/CPU and pushes every spawn past its
// startup timeout. Keep this conservative.
constexpr int kFallbackMaxConcurrency = 4;
constexpr int kBatchProbeTimeoutMs = 3000;
constexpr int kBatchStartupTimeoutMs = 6000;
constexpr int kProbeRetryThresholdMs = 800;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");
// gstatic responds with 204 on every working egress and is not blocked from
// most CN exits, so it produces fewer false negatives than google.com.
const QString kDefaultUrlTestUrl = QStringLiteral("https://www.gstatic.com/generate_204");

QString describeServer(const VmessItem& server)
{
    if (!server.remarks.trimmed().isEmpty()) {
        return server.remarks.trimmed();
    }
    if (!server.address.trimmed().isEmpty() && server.port > 0) {
        return QStringLiteral("%1:%2").arg(server.address.trimmed()).arg(server.port);
    }
    return server.indexId.trimmed();
}

QString defaultUrlTestUrl(const Config& config)
{
    return config.speedPingTestUrl.trimmed().isEmpty()
        ? kDefaultUrlTestUrl
        : config.speedPingTestUrl.trimmed();
}

QString normalizeErrorText(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Failed");
    }

    if (trimmed.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("timed out"), Qt::CaseInsensitive)) {
        return QStringLiteral("Timeout");
    }

    const QString firstLine = trimmed.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts).value(0).trimmed();
    return firstLine.left(96);
}

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

QString formatRuntimeCommand(const QString& program, const QStringList& arguments)
{
    QStringList parts;
    parts.reserve(arguments.size() + 1);
    parts.append(QDir::toNativeSeparators(program));
    for (const QString& argument : arguments) {
        QString escaped = argument;
        escaped.replace('"', QStringLiteral("\\\""));
        if (escaped.contains(' ')) {
            escaped = QStringLiteral("\"%1\"").arg(escaped);
        }
        parts.append(escaped);
    }
    return parts.join(' ');
}

int takeAvailablePortBase()
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
        if (SpeedTestServiceInternal::reserveProxyPorts(candidate, candidate + 1)) {
            return candidate;
        }
    }

    return 0;
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

bool isProxyPortReady(int port)
{
    QTcpSocket socket;
    socket.connectToHost(kLoopbackAddress, port);
    if (socket.waitForConnected(200)) {
        socket.disconnectFromHost();
        return true;
    }

    return false;
}

std::optional<SpeedTestServiceInternal::ReadyProxy> waitForProxy(
    const int socksPort,
    const int httpPort,
    int timeoutMs,
    const std::atomic_bool& cancelled,
    const std::function<bool()>& hasProcessExited = {})
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (cancelled.load()) {
            return std::nullopt;
        }

        if (hasProcessExited && hasProcessExited()) {
            return std::nullopt;
        }

        if (const auto readyProxy = SpeedTestServiceInternal::detectReadyProxy(
                socksPort,
                httpPort,
                [](int port) { return isProxyPortReady(port); });
            readyProxy.has_value()) {
            return readyProxy;
        }

        if (cancelled.load()) {
            return std::nullopt;
        }

        QThread::msleep(50);
    }

    return std::nullopt;
}

QString readProcessOutput(QProcess& process)
{
    return QString::fromUtf8(process.readAll()).trimmed();
}

SpeedTestServiceInternal::UrlProbeResult probeProxiedUrl(
    QNetworkProxy::ProxyType proxyType,
    int proxyPort,
    const QString& url,
    int timeoutMs)
{
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(proxyType, kLoopbackAddress, proxyPort));

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SongBox-urltest"));

    QElapsedTimer timer;
    timer.start();

    bool timedOut = false;
    bool headersObserved = false;
    int headerStatusCode = 0;
    qint64 headerElapsedMs = -1;
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::metaDataChanged, &loop, [&]() {
        if (headersObserved) {
            return;
        }

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 0) {
            return;
        }

        headersObserved = true;
        headerStatusCode = statusCode;
        headerElapsedMs = timer.elapsed();
        if (statusCode == 200 || statusCode == 204) {
            reply->abort();
            loop.quit();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(timeoutMs);
    loop.exec();
    timeoutTimer.stop();

    const QString errorText = reply->errorString();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    const bool successFromHeaders = !timedOut && headersObserved && (headerStatusCode == 200 || headerStatusCode == 204);
    const bool successFromFinishedReply = !timedOut && (statusCode == 200 || statusCode == 204);
    const qint64 successLatencyMs = successFromHeaders ? headerElapsedMs : timer.elapsed();
    const qint64 reportedLatencyMs = (successFromHeaders || successFromFinishedReply)
        ? successLatencyMs
        : (timedOut ? -1 : timer.elapsed());
    return SpeedTestServiceInternal::classifyUrlProbeResult(
        successFromHeaders || successFromFinishedReply,
        timedOut,
        reportedLatencyMs,
        normalizeErrorText(errorText));
}

QString runUrlTest(
    const Config& probeConfigTemplate,
    const QString& urlTestUrl,
    const SpeedTestRequestItem& item,
    const QString& customConfigDirectory,
    const std::atomic_bool& cancelled,
    const std::function<void(const QString&)>& log)
{
    const QString serverName = describeServer(item.server);
    if (item.coreInfo.program.trimmed().isEmpty() || !QFileInfo::exists(item.coreInfo.program)) {
        return QStringLiteral("Core missing");
    }

    if (cancelled.load()) {
        return QStringLiteral("Cancelled");
    }
    const int socksPort = takeAvailablePortBase();
    if (socksPort <= 0) {
        return QStringLiteral("Port busy");
    }
    const int httpPort = socksPort + 1;
    struct ScopedProxyPortRelease
    {
        int socksPort = 0;
        int httpPort = 0;

        ~ScopedProxyPortRelease()
        {
            SpeedTestServiceInternal::releaseProxyPorts(socksPort, httpPort);
        }
    } reservedPorts{socksPort, httpPort};

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return QStringLiteral("Temp dir failed");
    }

    Config runtimeConfig = probeConfigTemplate;
    runtimeConfig.localPort = socksPort;

    const QString configPath = temporaryDirectory.filePath(QStringLiteral("runtime.json"));
    ClientConfigWriter clientConfigWriter(customConfigDirectory);
    const OperationResult writeResult = clientConfigWriter.writeClientConfig(runtimeConfig, item.runtimeServer, configPath, 0);
    if (!writeResult.success) {
        return normalizeErrorText(writeResult.message);
    }

    const QString runtimeConfigPreview = readTextFileIfExists(configPath);
    if (!runtimeConfigPreview.isEmpty()) {
        log(QStringLiteral("URL Test config preview | %1 | %2")
                .arg(serverName, summarizeProcessOutput(runtimeConfigPreview)));
    }

    const QStringList runtimeArguments = buildCoreArguments(item.coreInfo, configPath);
    log(QStringLiteral("URL Test runtime | %1 | core=%2 | workdir=%3")
            .arg(serverName,
                 QDir::toNativeSeparators(item.coreInfo.program),
                 QDir::toNativeSeparators(item.coreInfo.workingDirectory)));
    log(QStringLiteral("URL Test command | %1")
            .arg(formatRuntimeCommand(item.coreInfo.program, runtimeArguments)));
    log(QStringLiteral("URL Test ports | %1 | socks=%2 | http=%3 | tun=off")
            .arg(serverName)
            .arg(socksPort)
            .arg(httpPort));

    QProcess coreProcess;
    coreProcess.setProcessChannelMode(QProcess::MergedChannels);
    coreProcess.setProgram(item.coreInfo.program);
    coreProcess.setArguments(runtimeArguments);
    if (!item.coreInfo.workingDirectory.trimmed().isEmpty()) {
        coreProcess.setWorkingDirectory(item.coreInfo.workingDirectory);
    }
    coreProcess.start();
    if (!coreProcess.waitForStarted(kRuntimeStartupTimeoutMs)) {
        const QString errorText = cancelled.load() ? QStringLiteral("Cancelled") : normalizeErrorText(coreProcess.errorString());
        log(QStringLiteral("URL Test start failed | %1 | %2").arg(serverName, errorText));
        return errorText;
    }

    log(QStringLiteral("URL Test waiting for local proxy | %1 | timeout=%2ms")
            .arg(serverName)
            .arg(kRuntimeStartupTimeoutMs));

    const auto stopRuntime = [&coreProcess]() {
        coreProcess.terminate();
        if (!coreProcess.waitForFinished(3000)) {
            coreProcess.kill();
            coreProcess.waitForFinished(3000);
        }
    };

    const std::optional<SpeedTestServiceInternal::ReadyProxy> readyProxy =
        waitForProxy(socksPort, httpPort, kRuntimeStartupTimeoutMs, cancelled, [&coreProcess]() {
            return coreProcess.state() == QProcess::NotRunning;
        });
    if (!readyProxy.has_value()) {
        const QString output = readProcessOutput(coreProcess);
        const bool exitedBeforeReady = coreProcess.state() == QProcess::NotRunning;
        const int exitCode = exitedBeforeReady ? coreProcess.exitCode() : -1;
        const QProcess::ExitStatus exitStatus = exitedBeforeReady
            ? coreProcess.exitStatus()
            : QProcess::NormalExit;
        stopRuntime();
        if (cancelled.load()) {
            return QStringLiteral("Cancelled");
        }
        const QString outputSummary = summarizeProcessOutput(output);
        if (exitedBeforeReady) {
            const QString statusText = exitStatus == QProcess::CrashExit
                ? QStringLiteral("crash")
                : QStringLiteral("exit");
            log(QStringLiteral("URL Test proxy exited before ready | %1 | code=%2 | status=%3 | output=%4")
                    .arg(serverName)
                    .arg(exitCode)
                    .arg(statusText, outputSummary));
            return output.isEmpty()
                ? QStringLiteral("Proxy exited before listening (code %1)").arg(exitCode)
                : QStringLiteral("Proxy exited before listening: %1").arg(normalizeErrorText(output));
        }

        log(QStringLiteral("URL Test startup timeout | %1 | output=%2")
                .arg(serverName, outputSummary));
        if (!runtimeConfigPreview.isEmpty()) {
            log(QStringLiteral("URL Test config on timeout | %1 | %2")
                    .arg(serverName, summarizeProcessOutput(runtimeConfigPreview)));
        }
        return output.isEmpty()
            ? QStringLiteral("Proxy startup timeout (no core output)")
            : QStringLiteral("Proxy startup timeout: %1").arg(normalizeErrorText(output));
    }
    const QString url = urlTestUrl;
    log(QStringLiteral("URL Test proxy ready | %1 | type=%2 | port=%3 | url=%4")
            .arg(serverName)
            .arg(readyProxy->name)
            .arg(readyProxy->port)
            .arg(url));

    SpeedTestServiceInternal::UrlProbeResult probeResult = probeProxiedUrl(
        readyProxy->type,
        readyProxy->port,
        url,
        kRuntimeRequestTimeoutMs);

    // A connect-reset that happens within the first ~hundreds of ms usually
    // means the outbound was still warming up (TLS handshake to the upstream
    // not yet established). One retry catches that without doubling the cost
    // for healthy servers.
    if (probeResult.status == SpeedTestServiceInternal::UrlProbeStatus::Failed
        && probeResult.latencyMs >= 0
        && probeResult.latencyMs < kProbeRetryThresholdMs
        && !cancelled.load()) {
        log(QStringLiteral("URL Test retry | %1 | first attempt failed in %2 ms")
                .arg(serverName)
                .arg(probeResult.latencyMs));
        probeResult = probeProxiedUrl(
            readyProxy->type,
            readyProxy->port,
            url,
            kRuntimeRequestTimeoutMs);
    }

    stopRuntime();

    if (probeResult.status == SpeedTestServiceInternal::UrlProbeStatus::Failed
        && !probeResult.errorText.isEmpty()) {
        log(QStringLiteral("URL Test failure detail | %1 | %2")
                .arg(serverName, probeResult.errorText));
    }
    return SpeedTestServiceInternal::formatUrlProbeResult(probeResult);
}

struct BatchProbeEntry {
    QString indexId;
    QString serverName;
    int socksPort = 0;
    QString outboundTag;
    QString inboundTag;
    QJsonObject outbound;
    QString url;
};

enum class BatchFlavor {
    Legacy,   // Xray-style outbound (has "protocol")
    SingBox,  // sing-box-style outbound (has "type")
};

// Extracts the primary outbound from a writer-generated root and tags the
// flavor so the caller knows which batch shape to assemble. sing-box and
// Xray-legacy can't be mixed inside one core's config.
std::optional<std::pair<BatchFlavor, QJsonObject>> extractPrimaryOutbound(const QJsonObject& root)
{
    const QJsonValue outboundsValue = root.value(QStringLiteral("outbounds"));
    if (!outboundsValue.isArray()) {
        return std::nullopt;
    }
    const QJsonArray outboundsArray = outboundsValue.toArray();
    if (outboundsArray.isEmpty()) {
        return std::nullopt;
    }
    const QJsonValue firstValue = outboundsArray.first();
    if (!firstValue.isObject()) {
        return std::nullopt;
    }
    const QJsonObject outbound = firstValue.toObject();
    if (outbound.contains(QStringLiteral("type"))) {
        return std::make_pair(BatchFlavor::SingBox, outbound);
    }
    if (outbound.contains(QStringLiteral("protocol"))) {
        return std::make_pair(BatchFlavor::Legacy, outbound);
    }
    return std::nullopt;
}

QJsonObject buildBatchSocksInbound(int port, const QString& tag)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), tag);
    inbound.insert(QStringLiteral("port"), port);
    inbound.insert(QStringLiteral("listen"), kLoopbackAddress);
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("socks"));

    QJsonObject settings;
    settings.insert(QStringLiteral("auth"), QStringLiteral("noauth"));
    settings.insert(QStringLiteral("udp"), false);
    settings.insert(QStringLiteral("allowTransparent"), false);
    inbound.insert(QStringLiteral("settings"), settings);
    return inbound;
}

QJsonObject buildBatchDirectOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("freedom"));
    outbound.insert(QStringLiteral("settings"), QJsonObject{});
    return outbound;
}

QJsonObject buildBatchBlackholeOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("block"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("blackhole"));
    outbound.insert(QStringLiteral("settings"), QJsonObject{});
    return outbound;
}

QJsonObject buildBatchRoutingRule(const QString& inboundTag, const QString& outboundTag)
{
    QJsonObject rule;
    rule.insert(QStringLiteral("type"), QStringLiteral("field"));
    QJsonArray tags;
    tags.append(inboundTag);
    rule.insert(QStringLiteral("inboundTag"), tags);
    rule.insert(QStringLiteral("outboundTag"), outboundTag);
    return rule;
}

QJsonObject assembleBatchConfig(const QList<BatchProbeEntry>& entries)
{
    QJsonObject root;
    QJsonObject log;
    log.insert(QStringLiteral("loglevel"), QStringLiteral("warning"));
    root.insert(QStringLiteral("log"), log);

    QJsonArray inbounds;
    QJsonArray outbounds;
    QJsonArray rules;
    for (const BatchProbeEntry& entry : entries) {
        inbounds.append(buildBatchSocksInbound(entry.socksPort, entry.inboundTag));
        outbounds.append(entry.outbound);
        rules.append(buildBatchRoutingRule(entry.inboundTag, entry.outboundTag));
    }
    outbounds.append(buildBatchDirectOutbound());
    outbounds.append(buildBatchBlackholeOutbound());
    root.insert(QStringLiteral("inbounds"), inbounds);
    root.insert(QStringLiteral("outbounds"), outbounds);

    QJsonObject routing;
    routing.insert(QStringLiteral("domainStrategy"), QStringLiteral("AsIs"));
    routing.insert(QStringLiteral("rules"), rules);
    root.insert(QStringLiteral("routing"), routing);

    return root;
}

QJsonObject buildSingBoxBatchSocksInbound(int port, const QString& tag)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("type"), QStringLiteral("socks"));
    inbound.insert(QStringLiteral("tag"), tag);
    inbound.insert(QStringLiteral("listen"), kLoopbackAddress);
    inbound.insert(QStringLiteral("listen_port"), port);
    return inbound;
}

QJsonObject buildSingBoxBatchDirectOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("type"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    return outbound;
}

QJsonObject buildSingBoxBatchRoutingRule(const QString& inboundTag, const QString& outboundTag)
{
    QJsonObject rule;
    QJsonArray tags;
    tags.append(inboundTag);
    rule.insert(QStringLiteral("inbound"), tags);
    rule.insert(QStringLiteral("outbound"), outboundTag);
    return rule;
}

QJsonObject assembleSingBoxBatchConfig(const QList<BatchProbeEntry>& entries)
{
    QJsonObject root;
    QJsonObject log;
    log.insert(QStringLiteral("level"), QStringLiteral("warn"));
    root.insert(QStringLiteral("log"), log);

    QJsonArray inbounds;
    QJsonArray outbounds;
    QJsonArray rules;
    for (const BatchProbeEntry& entry : entries) {
        inbounds.append(buildSingBoxBatchSocksInbound(entry.socksPort, entry.inboundTag));
        outbounds.append(entry.outbound);
        rules.append(buildSingBoxBatchRoutingRule(entry.inboundTag, entry.outboundTag));
    }
    outbounds.append(buildSingBoxBatchDirectOutbound());
    root.insert(QStringLiteral("inbounds"), inbounds);
    root.insert(QStringLiteral("outbounds"), outbounds);

    QJsonObject route;
    route.insert(QStringLiteral("final"), QStringLiteral("direct"));
    route.insert(QStringLiteral("rules"), rules);
    root.insert(QStringLiteral("route"), route);

    return root;
}

// Runs a batched URL test for items sharing the same core binary. Returns
// true if the batch path was used (results emitted via emitResult). Returns
// false if the group should fall back to per-item runUrlTest -- e.g. when
// outbound shapes are mixed across flavors or when a TUN-compat sidecar is
// required for any item.
bool runBatchedGroup(
    const Config& probeConfigTemplate,
    const QString& urlTestUrl,
    const QList<SpeedTestRequestItem>& groupItems,
    const QString& customConfigDirectory,
    const std::atomic_bool& cancelled,
    const std::function<void(const QString&)>& log,
    const std::function<void(const QString&, const QString&)>& emitResult)
{
    if (groupItems.isEmpty()) {
        return true;
    }

    ClientConfigWriter clientConfigWriter(customConfigDirectory);
    QList<BatchProbeEntry> entries;
    entries.reserve(groupItems.size());
    std::optional<BatchFlavor> flavor;

    for (int i = 0; i < groupItems.size(); ++i) {
        const SpeedTestRequestItem& item = groupItems[i];
        if (item.server.configType == ConfigType::Custom) {
            return false;
        }

        const ClientConfigWriter::GeneratedConfigSet generated = clientConfigWriter.generateClientConfigs(
            probeConfigTemplate,
            item.runtimeServer,
            0);

        // Auxiliary configs (TUN-compat sing-box sidecars) can't be merged
        // into a single batch -- fall back to per-item path.
        if (!generated.auxiliary.isEmpty()) {
            return false;
        }

        const auto extracted = extractPrimaryOutbound(generated.primary.root);
        if (!extracted.has_value()) {
            return false;
        }

        const BatchFlavor entryFlavor = extracted->first;
        if (!flavor.has_value()) {
            flavor = entryFlavor;
        } else if (*flavor != entryFlavor) {
            // Mixed flavors in one group shouldn't normally happen (items
            // are grouped by core program path) but defend anyway.
            return false;
        }

        QJsonObject outbound = extracted->second;
        const QString outboundTag = QStringLiteral("proxy_%1").arg(i);
        const QString inboundTag = QStringLiteral("socks_in_%1").arg(i);
        outbound.insert(QStringLiteral("tag"), outboundTag);

        BatchProbeEntry entry;
        entry.indexId = item.server.indexId;
        entry.serverName = describeServer(item.server);
        entry.outboundTag = outboundTag;
        entry.inboundTag = inboundTag;
        entry.outbound = outbound;
        entry.url = urlTestUrl;
        entries.append(entry);
    }

    if (entries.isEmpty() || !flavor.has_value()) {
        return true;
    }

    struct PortReservation {
        int socksPort = 0;
        int httpPort = 0;
    };
    QList<PortReservation> reservations;
    auto releaseAllPorts = [&reservations]() {
        for (const PortReservation& r : reservations) {
            SpeedTestServiceInternal::releaseProxyPorts(r.socksPort, r.httpPort);
        }
        reservations.clear();
    };

    for (BatchProbeEntry& entry : entries) {
        const int port = takeAvailablePortBase();
        if (port <= 0) {
            log(QStringLiteral("URL Test batch | port allocation failed at entry %1")
                    .arg(reservations.size()));
            releaseAllPorts();
            return false;
        }
        entry.socksPort = port;
        reservations.append({port, port + 1});
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        log(QStringLiteral("URL Test batch | temp dir failed"));
        releaseAllPorts();
        return false;
    }

    const QString configPath = temporaryDirectory.filePath(QStringLiteral("batch.json"));
    const QJsonObject batchRoot = (*flavor == BatchFlavor::SingBox)
        ? assembleSingBoxBatchConfig(entries)
        : assembleBatchConfig(entries);
    {
        QFile configFile(configPath);
        if (!configFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            log(QStringLiteral("URL Test batch | failed to open config for write"));
            releaseAllPorts();
            return false;
        }
        configFile.write(QJsonDocument(batchRoot).toJson(QJsonDocument::Compact));
    }

    const CoreInfo& coreInfo = groupItems.first().coreInfo;
    if (coreInfo.program.trimmed().isEmpty() || !QFileInfo::exists(coreInfo.program)) {
        log(QStringLiteral("URL Test batch | core missing"));
        releaseAllPorts();
        return false;
    }

    const QStringList runtimeArguments = buildCoreArguments(coreInfo, configPath);
    const QString flavorName = (*flavor == BatchFlavor::SingBox)
        ? QStringLiteral("sing-box")
        : QStringLiteral("xray");
    log(QStringLiteral("URL Test batch start | count=%1 | flavor=%2 | core=%3")
            .arg(entries.size())
            .arg(flavorName, QDir::toNativeSeparators(coreInfo.program)));
    log(QStringLiteral("URL Test batch command | %1")
            .arg(formatRuntimeCommand(coreInfo.program, runtimeArguments)));

    QProcess coreProcess;
    coreProcess.setProcessChannelMode(QProcess::MergedChannels);
    coreProcess.setProgram(coreInfo.program);
    coreProcess.setArguments(runtimeArguments);
    if (!coreInfo.workingDirectory.trimmed().isEmpty()) {
        coreProcess.setWorkingDirectory(coreInfo.workingDirectory);
    }
    coreProcess.start();
    if (!coreProcess.waitForStarted(kBatchStartupTimeoutMs)) {
        log(QStringLiteral("URL Test batch | core failed to start: %1")
                .arg(normalizeErrorText(coreProcess.errorString())));
        releaseAllPorts();
        return false;
    }

    const auto stopRuntime = [&coreProcess]() {
        coreProcess.terminate();
        if (!coreProcess.waitForFinished(3000)) {
            coreProcess.kill();
            coreProcess.waitForFinished(3000);
        }
    };

    // Wait until ANY of the SOCKS inbounds is accepting connections. Xray
    // binds all listeners during startup, so once one is up the rest are
    // typically ready within milliseconds.
    QElapsedTimer waitTimer;
    waitTimer.start();
    bool ready = false;
    while (waitTimer.elapsed() < kBatchStartupTimeoutMs) {
        if (cancelled.load() || coreProcess.state() == QProcess::NotRunning) {
            break;
        }
        if (isProxyPortReady(entries.first().socksPort)) {
            ready = true;
            break;
        }
        QThread::msleep(50);
    }

    if (!ready) {
        const QString output = summarizeProcessOutput(readProcessOutput(coreProcess));
        log(QStringLiteral("URL Test batch | core not ready before timeout | output=%1").arg(output));
        stopRuntime();
        releaseAllPorts();
        return false;
    }

    log(QStringLiteral("URL Test batch ready | probing %1 inbounds").arg(entries.size()));

    // Probe all inbounds concurrently. Each probe is just an HTTP request to
    // a loopback SOCKS port -- no per-server core startup cost, so we can
    // safely fire them all at once even for large batches.
    std::vector<std::future<QString>> futures;
    futures.reserve(entries.size());
    for (const BatchProbeEntry& entry : entries) {
        futures.push_back(std::async(std::launch::async, [&entry, &cancelled]() -> QString {
            if (cancelled.load()) {
                return QStringLiteral("Cancelled");
            }
            SpeedTestServiceInternal::UrlProbeResult probeResult = probeProxiedUrl(
                QNetworkProxy::Socks5Proxy,
                entry.socksPort,
                entry.url,
                kBatchProbeTimeoutMs);

            if (probeResult.status == SpeedTestServiceInternal::UrlProbeStatus::Failed
                && probeResult.latencyMs >= 0
                && probeResult.latencyMs < kProbeRetryThresholdMs
                && !cancelled.load()) {
                probeResult = probeProxiedUrl(
                    QNetworkProxy::Socks5Proxy,
                    entry.socksPort,
                    entry.url,
                    kBatchProbeTimeoutMs);
            }
            return SpeedTestServiceInternal::formatUrlProbeResult(probeResult);
        }));
    }

    for (int i = 0; i < entries.size(); ++i) {
        QString result;
        try {
            result = futures[i].get();
        } catch (...) {
            result = QStringLiteral("Failed");
        }
        if (result.trimmed().isEmpty()) {
            result = QStringLiteral("Failed");
        }
        if (!cancelled.load()) {
            log(QStringLiteral("URL Test result | %1 -> %2").arg(entries[i].serverName, result));
            emitResult(entries[i].indexId, result);
        }
    }

    stopRuntime();
    releaseAllPorts();
    return true;
}

class SpeedTestWorker final : public QObject
{
    Q_OBJECT

public:
    explicit SpeedTestWorker(QString customConfigDirectory, std::atomic_bool& cancelFlag)
        : customConfigDirectory_(std::move(customConfigDirectory))
        , cancelled_(cancelFlag)
    {
    }

    void runBatch(const Config& config, const QList<SpeedTestRequestItem>& items)
    {
        int completed = 0;
        const Config probeConfigTemplate = SpeedTestServiceInternal::makeUrlTestRuntimeConfig(config);
        const QString urlTestUrl = defaultUrlTestUrl(config);

        auto logCallback = [this](const QString& message) {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit logGenerated(message);
            }, Qt::QueuedConnection);
        };

        auto emitResultCallback = [this, &completed](const QString& indexId, const QString& result) {
            ++completed;
            if (!cancelled_.load()) {
                emit testResultReady(indexId, result);
            }
        };

        // Group items by core program path so each group shares one core
        // process. Items whose core path resolves to the same file go in
        // the same batch; Custom configs are handled separately because
        // they can't be merged into a multi-outbound Xray config.
        QMap<QString, QList<int>> groupsByCore;
        QList<int> customIndices;
        for (int i = 0; i < items.size(); ++i) {
            const SpeedTestRequestItem& item = items[i];
            if (item.server.configType == ConfigType::Custom) {
                customIndices.append(i);
                continue;
            }

            const QFileInfo coreInfo(item.coreInfo.program);
            const QString canonical = coreInfo.canonicalFilePath();
            const QString key = canonical.isEmpty() ? item.coreInfo.program : canonical;
            groupsByCore[key].append(i);
        }

        for (int index : customIndices) {
            if (cancelled_.load()) {
                break;
            }
            const QString serverName = describeServer(items[index].server);
            emit logGenerated(QStringLiteral("URL Test: %1").arg(serverName));
            const QString result = QStringLiteral("Unsupported");
            ++completed;
            emit testResultReady(items[index].server.indexId, result);
            emit logGenerated(QStringLiteral("URL Test result | %1 -> %2").arg(serverName, result));
        }

        for (auto it = groupsByCore.constBegin(); it != groupsByCore.constEnd(); ++it) {
            if (cancelled_.load()) {
                break;
            }

            const QList<int>& indices = it.value();
            QList<SpeedTestRequestItem> groupItems;
            groupItems.reserve(indices.size());
            for (int idx : indices) {
                groupItems.append(items[idx]);
                emit logGenerated(QStringLiteral("URL Test: %1").arg(describeServer(items[idx].server)));
            }

            const bool batched = runBatchedGroup(
                probeConfigTemplate,
                urlTestUrl,
                groupItems,
                customConfigDirectory_,
                cancelled_,
                logCallback,
                emitResultCallback);

            if (batched) {
                continue;
            }

            // Fallback per-item path (sing-box, TUN-compat aux configs,
            // non-legacy outbound shapes). Reuses the existing concurrent
            // pool so per-server core spawns don't serialize unnecessarily.
            runFallbackGroup(indices, items, probeConfigTemplate, urlTestUrl, completed);
        }

        const bool wasCancelled = cancelled_.load();
        emit batchFinished(wasCancelled
            ? QStringLiteral("URL Test cancelled after %1 server(s).").arg(completed)
            : QStringLiteral("URL Test finished for %1 server(s).").arg(completed));
    }

private:
    void runFallbackGroup(
        const QList<int>& indices,
        const QList<SpeedTestRequestItem>& items,
        const Config& probeConfigTemplate,
        const QString& urlTestUrl,
        int& completed)
    {
        struct PendingItem {
            QString indexId;
            QString serverName;
            std::future<QString> future;
        };

        const int itemCount = indices.size();
        const int maxConcurrency = std::max(1, std::min(kFallbackMaxConcurrency, itemCount));
        std::vector<PendingItem> pending;
        pending.reserve(static_cast<std::size_t>(maxConcurrency));

        auto emitCompletedResult = [this, &completed](PendingItem& pendingItem) {
            QString result;
            try {
                result = pendingItem.future.get();
            } catch (...) {
                result = QStringLiteral("Failed");
            }

            if (result.trimmed().isEmpty()) {
                result = QStringLiteral("Failed");
            }

            ++completed;
            if (!cancelled_.load()) {
                emit testResultReady(pendingItem.indexId, result);
                emit logGenerated(QStringLiteral("URL Test result | %1 -> %2")
                                      .arg(pendingItem.serverName, result));
            }
        };

        auto flushReadyTasks = [&pending, &emitCompletedResult]() {
            for (auto it = pending.begin(); it != pending.end();) {
                if (it->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                    emitCompletedResult(*it);
                    it = pending.erase(it);
                } else {
                    ++it;
                }
            }
        };

        for (int idx : indices) {
            if (cancelled_.load()) {
                break;
            }

            const SpeedTestRequestItem item = items[idx];
            const QString serverName = describeServer(item.server);

            pending.push_back(PendingItem{
                item.server.indexId,
                serverName,
                std::async(std::launch::async, [this, item, &probeConfigTemplate, &urlTestUrl]() -> QString {
                    if (item.server.configType == ConfigType::Custom) {
                        return QStringLiteral("Unsupported");
                    }
                    if (cancelled_.load()) {
                        return QStringLiteral("Cancelled");
                    }
                    return runUrlTest(probeConfigTemplate, urlTestUrl, item, customConfigDirectory_, cancelled_, [this](const QString& message) {
                        QMetaObject::invokeMethod(this, [this, message]() {
                            emit logGenerated(message);
                        }, Qt::QueuedConnection);
                    });
                })});

            while (!cancelled_.load() && pending.size() >= static_cast<std::size_t>(maxConcurrency)) {
                flushReadyTasks();
                if (pending.size() >= static_cast<std::size_t>(maxConcurrency)) {
                    QThread::msleep(25);
                }
            }
        }

        while (!pending.empty()) {
            flushReadyTasks();
            if (!pending.empty()) {
                QThread::msleep(25);
            }
        }
    }

signals:
    void logGenerated(const QString& message);
    void testResultReady(const QString& indexId, const QString& result);
    void batchFinished(const QString& summary);

private:
    QString customConfigDirectory_;
    std::atomic_bool& cancelled_;
};

} // namespace

SpeedTestService::SpeedTestService(QString customConfigDirectory, QObject* parent)
    : QObject(parent)
    , customConfigDirectory_(std::move(customConfigDirectory))
{
    workerThread_ = new QThread(this);
    auto* worker = new SpeedTestWorker(customConfigDirectory_, cancelled_);
    worker_ = worker;
    worker->moveToThread(workerThread_);

    QObject::connect(workerThread_, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(worker, &SpeedTestWorker::logGenerated, this, &SpeedTestService::logGenerated);
    QObject::connect(worker, &SpeedTestWorker::testResultReady, this, &SpeedTestService::testResultReady);
    QObject::connect(worker, &SpeedTestWorker::batchFinished, this, [this](const QString& summary) {
        running_ = false;
        emit logGenerated(summary);
        emit finished(summary);
        emit runningChanged(false);
    });

    workerThread_->start();
}

OperationResult SpeedTestService::start(const Config& config, const QList<SpeedTestRequestItem>& items)
{
    if (running_) {
        return OperationResult::fail(QStringLiteral("Another speed test batch is already running."));
    }

    if (items.isEmpty()) {
        return OperationResult::fail(QStringLiteral("No servers were selected for speed testing."));
    }

    if (worker_ == nullptr || workerThread_ == nullptr || !workerThread_->isRunning()) {
        return OperationResult::fail(QStringLiteral("Speed test worker thread is unavailable."));
    }

    cancelled_ = false;
    running_ = true;
    const QString message = QStringLiteral("URL Test started for %1 server(s).")
                                .arg(items.size());
    emit runningChanged(true);
    emit logGenerated(message);

    auto* worker = static_cast<SpeedTestWorker*>(worker_);
    const bool invoked = QMetaObject::invokeMethod(
        worker,
        [worker, config, items]() {
            worker->runBatch(config, items);
        },
        Qt::QueuedConnection);
    if (!invoked) {
        running_ = false;
        emit runningChanged(false);
        return OperationResult::fail(QStringLiteral("Failed to queue the speed test batch."));
    }

    return OperationResult::ok(message);
}

bool SpeedTestService::isRunning() const
{
    return running_;
}

void SpeedTestService::cancel()
{
    if (running_) {
        cancelled_ = true;
    }
}

SpeedTestService::~SpeedTestService()
{
    if (workerThread_ == nullptr) {
        return;
    }

    // The worker checks cancelled_ at every batch-loop iteration. Without this
    // signal the synchronous worker keeps spawning per-server probes (each up
    // to kRuntimeRequestTimeoutMs long), and the wait() below blocks until the
    // entire pending batch drains -- minutes on a large server list at shutdown.
    cancelled_ = true;

    workerThread_->quit();
    workerThread_->wait();
}

#include "SpeedTestService.moc"
