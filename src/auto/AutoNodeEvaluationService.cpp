#include "auto/AutoNodeEvaluationService.h"

#include <QDateTime>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <future>
#include <limits>
#include <optional>
#include <vector>

#include "appcore/OutboundLocationProbeService.h"
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

// `error` is rendered verbatim as the node's state text in the SongBirdAuto node
// table (SongBirdAutoWindow::availabilityText), so every argument at a call site is
// user-visible prose and has to be localized here, not at the display site.
AutoNodeEvaluation unavailableResult(const SpeedTestRequestItem& item, const QString& error)
{
    AutoNodeEvaluation result;
    result.indexId = item.indexId;
    result.displayName = item.displayName.trimmed().isEmpty()
        ? serverDisplayName(item.runtimeServer)
        : item.displayName;
    result.error = error.trimmed().isEmpty()
        ? QStringLiteral("Failed")
        : error.trimmed();
    result.tested = true;
    result.checkedAt = QDateTime::currentDateTimeUtc();
    return result;
}

// One attempt, or no attempt: the reservation scans for the same free-triple 64 times and the scan
// is a race against every other core and probe on the machine, so a single empty result is a lost
// race rather than a busy machine. Retrying is what separates "the ports really are exhausted" from
// "a worker took ours", and only the former says anything about the node.
constexpr int kPortReservationAttempts = 3;
constexpr int kPortReservationRetryDelayMs = 150;

constexpr int kInconclusive = std::numeric_limits<int>::min();

int takePortsWithRetry(PortPool::Ports& ports, const std::atomic_bool& cancelled)
{
    for (int attempt = 0; attempt < kPortReservationAttempts; ++attempt) {
        ports = PortPool::takeAvailable();
        if (ports.socksPort > 0 && ports.httpPort > 0 && ports.locationProbePort > 0) {
            return 0;
        }

        if (cancelled.load()) {
            return kInconclusive;
        }
        if (attempt + 1 < kPortReservationAttempts) {
            QThread::msleep(kPortReservationRetryDelayMs);
        }
    }

    return kInconclusive;
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

    PortPool::Ports ports;
    if (takePortsWithRetry(ports, cancelled) != 0) {
        // No probe ran, so this is not a verdict on the node. Reporting it as a failed test would
        // push the circuit breaker's failure counter toward tripping a node that is perfectly
        // healthy -- the symptom would be "a good node went red" with nothing in the log to
        // explain it.
        AutoNodeEvaluation result = unavailableResult(item, QStringLiteral("Port busy"));
        result.inconclusive = true;
        return result;
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

    // A std::future destructor is not a completion: libstdc++ blocks on the task, MSVC detaches
    // from it. So a cancelled batch used to leave tasks running after this function returned,
    // reading `&request` and `&cancelled` -- both of which belong to the caller. The join at the
    // bottom is what makes those two references safe to leave behind.
    struct PendingItem {
        std::future<AutoNodeEvaluation> future;
    };

    std::vector<PendingItem> pending;
    pending.reserve(static_cast<std::size_t>(maxConcurrency));
    int nextIndex = 0;

    // Joins every task still in flight. Each one holds its own ports, temp dir and QProcess, and
    // every wait inside it is bounded, so this ends on its own: cancellation cannot shorten a
    // probe that is already out, and pretending otherwise is what left the tasks running.
    auto joinPending = [&]() {
        for (PendingItem& item : pending) {
            item.future.wait();
        }
    };

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

    // Waits even when the loop above ended normally -- the last flush can leave a task a
    // millisecond short of ready. Results that land after the loop are discarded, because
    // flushReady only reports them when the batch was not cancelled.
    joinPending();
    flushReady();

    return results;
}
