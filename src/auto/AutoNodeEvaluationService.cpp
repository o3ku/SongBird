#include "auto/AutoNodeEvaluationService.h"

#include <QDateTime>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <future>
#include <optional>
#include <vector>

#include "app/OutboundLocationProbeService.h"
#include "auto/AutoCountrySupport.h"
#include "common/ServerDisplayName.h"
#include "runtime/ClientConfigWriter.h"
#include "services/SpeedTestPortReservation.h"
#include "services/SpeedTestRuntimeProcess.h"
#include "services/SpeedTestServiceInternal.h"
#include "services/SpeedTestUrlProbe.h"

namespace {

constexpr int kStartupTimeoutMs = 7000;
constexpr int kUrlProbeTimeoutMs = 3000;
constexpr int kLocationProbeTimeoutMs = 5000;

namespace PortPool = SpeedTestPortReservation;
namespace RuntimeProcess = SpeedTestRuntimeProcess;
namespace UrlProbe = SpeedTestUrlProbe;

QString normalizedError(const QString& value)
{
    return UrlProbe::normalizeErrorText(value);
}

void logLine(const AutoNodeEvaluationService::LogCallback& log, const QString& line)
{
    if (log) {
        log(line);
    }
}

Config makeProbeConfig(Config config, const PortPool::Ports& ports)
{
    config.allowLanConnection = false;
    config.logEnabled = false;
    config.tun().tunModeItem.enableTun = false;
    config.dns().enableCacheFile4Sbox = false;
    config.collection().servers.clear();
    config.collection().subscriptions.clear();
    config.policy().coreTypeItems.clear();
    config.localPort = ports.socksPort;
    config.localHttpPort = ports.httpPort;
    config.localLocationProbePort = ports.locationProbePort;
    return config;
}

AutoNodeEvaluation unavailableResult(const SpeedTestRequestItem& item, const QString& error)
{
    AutoNodeEvaluation result;
    result.indexId = item.indexId;
    result.displayName = item.displayName.trimmed().isEmpty()
        ? serverDisplayName(item.runtimeServer)
        : item.displayName;
    result.error = error.trimmed().isEmpty() ? QStringLiteral("Failed") : error.trimmed();
    result.tested = true;
    result.checkedAt = QDateTime::currentDateTimeUtc();
    return result;
}

AutoNodeEvaluation evaluateOne(
    const AutoNodeEvaluationService::Request& request,
    const SpeedTestRequestItem& item,
    const std::atomic_bool& cancelled,
    const AutoNodeEvaluationService::LogCallback& log)
{
    const QString serverName = item.displayName.trimmed().isEmpty()
        ? serverDisplayName(item.runtimeServer)
        : item.displayName;

    if (cancelled.load()) {
        return unavailableResult(item, QStringLiteral("Cancelled"));
    }
    if (item.configType == ConfigType::Custom) {
        return unavailableResult(item, QStringLiteral("Unsupported custom config"));
    }
    if (item.coreInfo.program.trimmed().isEmpty() || !QFileInfo::exists(item.coreInfo.program)) {
        return unavailableResult(item, QStringLiteral("Core missing"));
    }

    const PortPool::Ports ports = PortPool::takeAvailable();
    if (ports.socksPort <= 0 || ports.httpPort <= 0 || ports.locationProbePort <= 0) {
        return unavailableResult(item, QStringLiteral("Port busy"));
    }
    struct ScopedPortRelease
    {
        PortPool::Ports ports;
        ~ScopedPortRelease() { PortPool::release(ports); }
    } portRelease{ports};

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return unavailableResult(item, QStringLiteral("Temp dir failed"));
    }

    Config runtimeConfig = makeProbeConfig(request.config, ports);
    const QString configPath = temporaryDirectory.filePath(QStringLiteral("auto-node.json"));
    ClientConfigWriter writer(request.customConfigDirectory);
    const OperationResult writeResult = writer.writeClientConfig(runtimeConfig, item.runtimeServer, configPath);
    if (!writeResult.success) {
        return unavailableResult(item, normalizedError(writeResult.message));
    }

    QProcess coreProcess;
    coreProcess.setProcessChannelMode(QProcess::MergedChannels);
    coreProcess.setProgram(item.coreInfo.program);
    coreProcess.setArguments(RuntimeProcess::buildCoreArguments(item.coreInfo, configPath));
    if (!item.coreInfo.workingDirectory.trimmed().isEmpty()) {
        coreProcess.setWorkingDirectory(item.coreInfo.workingDirectory);
    }

    coreProcess.start();
    if (!coreProcess.waitForStarted(kStartupTimeoutMs)) {
        return unavailableResult(item, normalizedError(coreProcess.errorString()));
    }

    const auto readyProxy = PortPool::waitForProxy(
        ports.socksPort,
        ports.httpPort,
        kStartupTimeoutMs,
        cancelled,
        [&coreProcess]() { return coreProcess.state() == QProcess::NotRunning; });
    if (!readyProxy.has_value()) {
        const QString output = RuntimeProcess::readProcessOutput(coreProcess);
        RuntimeProcess::stopProcess(coreProcess);
        if (cancelled.load()) {
            return unavailableResult(item, QStringLiteral("Cancelled"));
        }
        return unavailableResult(
            item,
            output.trimmed().isEmpty()
                ? QStringLiteral("Proxy startup timeout")
                : normalizedError(output));
    }

    const QString url = request.urlTestUrl.trimmed().isEmpty()
        ? QStringLiteral("https://www.gstatic.com/generate_204")
        : request.urlTestUrl.trimmed();
    const SpeedTestServiceInternal::UrlProbeResult probeResult = UrlProbe::probeReadyProxyWithRetry(
        *readyProxy,
        url,
        kUrlProbeTimeoutMs,
        cancelled);
    if (probeResult.status != SpeedTestServiceInternal::UrlProbeStatus::Accessible) {
        RuntimeProcess::stopProcess(coreProcess);
        return unavailableResult(item, SpeedTestServiceInternal::formatUrlProbeResult(probeResult));
    }

    OutboundLocationDetails location;
    {
        OutboundLocationProbeService locationProbe;
        location = locationProbe.probeStructured(ports.locationProbePort);
    }
    RuntimeProcess::stopProcess(coreProcess);

    if (cancelled.load()) {
        return unavailableResult(item, QStringLiteral("Cancelled"));
    }
    if (location.countryCode.trimmed().isEmpty() && location.countryName.trimmed().isEmpty()) {
        return unavailableResult(
            item,
            location.error.trimmed().isEmpty()
                ? QStringLiteral("Outbound country unavailable")
                : location.error.trimmed());
    }

    AutoNodeEvaluation result;
    result.indexId = item.indexId;
    result.displayName = serverName;
    result.countryCode = normalizeAutoCountryCode(location.countryCode);
    result.countryName = location.countryName.trimmed();
    result.countryDisplay = autoCountryDisplayName(result.countryCode, result.countryName);
    result.locationSummary = location.location.trimmed();
    result.latencyMs = probeResult.latencyMs;
    result.available = true;
    result.tested = true;
    result.checkedAt = QDateTime::currentDateTimeUtc();
    logLine(log, QStringLiteral("Auto test OK | %1 | %2 | %3 ms")
                     .arg(serverName, result.countryDisplay)
                     .arg(result.latencyMs));
    return result;
}

} // namespace

QList<AutoNodeEvaluation> AutoNodeEvaluationService::evaluate(
    const Request& request,
    const std::atomic_bool& cancelled,
    const LogCallback& log,
    const ResultCallback& resultReady)
{
    QList<AutoNodeEvaluation> results;
    if (request.items.isEmpty()) {
        return results;
    }

    const int maxConcurrency = std::max(1, std::min(request.maxConcurrency, request.items.size()));
    struct PendingItem {
        std::future<AutoNodeEvaluation> future;
    };

    std::vector<PendingItem> pending;
    pending.reserve(static_cast<std::size_t>(maxConcurrency));
    int nextIndex = 0;

    auto flushReady = [&]() {
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                AutoNodeEvaluation result = it->future.get();
                if (!cancelled.load()) {
                    if (resultReady) {
                        resultReady(result);
                    }
                    results.append(result);
                }
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
    };

    while (!cancelled.load() && (nextIndex < request.items.size() || !pending.empty())) {
        while (!cancelled.load()
            && nextIndex < request.items.size()
            && pending.size() < static_cast<std::size_t>(maxConcurrency)) {
            const SpeedTestRequestItem item = request.items.at(nextIndex++);
            logLine(log, QStringLiteral("Auto test: %1").arg(item.displayName));
            pending.push_back(PendingItem{std::async(
                std::launch::async,
                [&request, item, &cancelled, log]() {
                    return evaluateOne(request, item, cancelled, log);
                })});
        }

        flushReady();
        if (!pending.empty()) {
            QThread::msleep(25);
        }
    }

    while (!pending.empty()) {
        flushReady();
        if (!pending.empty()) {
            QThread::msleep(25);
        }
    }

    return results;
}
