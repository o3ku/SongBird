#include "services/SpeedTestBatchRuntimeRunner.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>

#include <chrono>
#include <future>
#include <optional>
#include <vector>

#include "runtime/ClientConfigWriter.h"
#include "services/SpeedTestBatchConfig.h"
#include "services/SpeedTestPortReservation.h"
#include "services/SpeedTestRuntimeProcess.h"
#include "services/SpeedTestServiceInternal.h"
#include "services/SpeedTestUrlProbe.h"

namespace {

constexpr int kBatchProbeTimeoutMs = 3000;
constexpr int kBatchStartupTimeoutMs = 6000;
// Probes are cheap (one HTTP request to a loopback SOCKS port), but they all
// fan out through one core process; an unbounded burst of hundreds of TLS
// handshakes through that single process contends on its CPU and inflates the
// latency readings of every node measured in the burst.
constexpr int kBatchProbeMaxConcurrency = 16;
namespace BatchConfig = SpeedTestBatchConfig;
namespace PortPool = SpeedTestPortReservation;
namespace RuntimeProcess = SpeedTestRuntimeProcess;
namespace UrlProbe = SpeedTestUrlProbe;

} // namespace

bool SpeedTestBatchRuntimeRunner::runBatchedGroup(
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
    QList<BatchConfig::ProbeEntry> entries;
    entries.reserve(groupItems.size());
    std::optional<BatchConfig::BatchFlavor> flavor;

    for (int i = 0; i < groupItems.size(); ++i) {
        const SpeedTestRequestItem& item = groupItems[i];
        if (item.configType == ConfigType::Custom) {
            return false;
        }

        const ClientConfigWriter::GeneratedConfigSet generated = clientConfigWriter.generateClientConfigs(
            probeConfigTemplate,
            item.runtimeServer);

        // Auxiliary configs (TUN-compat sing-box sidecars) can't be merged
        // into a single batch -- fall back to per-item path.
        if (!generated.auxiliary.isEmpty()) {
            return false;
        }

        const auto extracted = BatchConfig::extractPrimaryOutbound(generated.primary.root);
        if (!extracted.has_value()) {
            return false;
        }

        const BatchConfig::BatchFlavor entryFlavor = extracted->first;
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

        BatchConfig::ProbeEntry entry;
        entry.indexId = item.indexId;
        entry.serverName = item.displayName;
        entry.outboundTag = outboundTag;
        entry.inboundTag = inboundTag;
        entry.outbound = outbound;
        entry.url = urlTestUrl;
        entries.append(entry);
    }

    if (entries.isEmpty() || !flavor.has_value()) {
        return true;
    }

    QList<int> reservedSocksPorts;
    auto releaseAllPorts = [&reservedSocksPorts]() {
        for (const int socksPort : reservedSocksPorts) {
            PortPool::releaseSocksPort(socksPort);
        }
        reservedSocksPorts.clear();
    };

    // The batch config only opens one SOCKS inbound per entry, so a full
    // three-port reservation per entry would reserve two ports that are never
    // bound and make the allocation scan fail more often on large batches.
    for (BatchConfig::ProbeEntry& entry : entries) {
        const int socksPort = PortPool::takeSocksPort();
        if (socksPort <= 0) {
            log(QStringLiteral("URL Test batch | port allocation failed at entry %1")
                    .arg(reservedSocksPorts.size()));
            releaseAllPorts();
            return false;
        }
        entry.socksPort = socksPort;
        reservedSocksPorts.append(socksPort);
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        log(QStringLiteral("URL Test batch | temp dir failed"));
        releaseAllPorts();
        return false;
    }

    const QString configPath = temporaryDirectory.filePath(QStringLiteral("batch.json"));
    const QJsonObject batchRoot = (*flavor == BatchConfig::BatchFlavor::SingBox)
        ? BatchConfig::assembleSingBoxConfig(entries)
        : BatchConfig::assembleLegacyConfig(entries);
    {
        QFile configFile(configPath);
        if (!configFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            log(QStringLiteral("URL Test batch | failed to open config for write"));
            releaseAllPorts();
            return false;
        }
        configFile.write(QJsonDocument(batchRoot).toJson(QJsonDocument::Compact));
    }

    if (groupItems.isEmpty()) {
        log(QStringLiteral("URL Test batch | no servers to test"));
        releaseAllPorts();
        return false;
    }

    const CoreInfo& coreInfo = groupItems.first().coreInfo;
    if (coreInfo.program.trimmed().isEmpty() || !QFileInfo::exists(coreInfo.program)) {
        log(QStringLiteral("URL Test batch | core missing"));
        releaseAllPorts();
        return false;
    }

    const QStringList runtimeArguments = RuntimeProcess::buildCoreArguments(coreInfo, configPath);

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
                .arg(UrlProbe::normalizeErrorText(coreProcess.errorString())));
        releaseAllPorts();
        return false;
    }

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
        if (PortPool::isProxyPortReady(entries.first().socksPort)) {
            ready = true;
            break;
        }
        QThread::msleep(50);
    }

    if (!ready) {
        const QString output = RuntimeProcess::summarizeProcessOutput(RuntimeProcess::readProcessOutput(coreProcess));
        log(QStringLiteral("URL Test batch | core not ready before timeout | output=%1").arg(output));
        RuntimeProcess::stopProcess(coreProcess);
        releaseAllPorts();
        return false;
    }

    // Probe the inbounds concurrently, capped by a sliding window: each probe
    // is just an HTTP request to a loopback SOCKS port, but all of them are
    // served by the one batch core process, so more in-flight probes than
    // kBatchProbeMaxConcurrency would skew the latency readings of the batch.
    // Same sliding-window pattern as SpeedTestWorker::runFallbackGroup.
    struct PendingProbe {
        int entryIndex = 0;
        std::future<QString> future;
    };
    std::vector<PendingProbe> pending;
    pending.reserve(kBatchProbeMaxConcurrency);

    auto collectReadyProbes = [&pending, &entries, &cancelled, &log, &emitResult]() {
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
                ++it;
                continue;
            }

            QString result;
            try {
                result = it->future.get();
            } catch (...) {
                result = QCoreApplication::translate("SpeedTestController", "Failed");
            }
            if (result.trimmed().isEmpty()) {
                result = QCoreApplication::translate("SpeedTestController", "Failed");
            }
            const BatchConfig::ProbeEntry& entry = entries[it->entryIndex];
            if (!cancelled.load()) {
                log(QStringLiteral("URL Test result | %1 -> %2").arg(entry.serverName, result));
                emitResult(entry.indexId, result);
            }
            it = pending.erase(it);
        }
    };

    for (int i = 0; i < entries.size(); ++i) {
        if (cancelled.load()) {
            break;
        }

        const BatchConfig::ProbeEntry probeEntry = entries[i];
        pending.push_back(PendingProbe{
            i,
            std::async(std::launch::async, [probeEntry, &cancelled]() -> QString {
                if (cancelled.load()) {
                    return QCoreApplication::translate("SpeedTestController", "Cancelled");
                }
                const SpeedTestServiceInternal::UrlProbeResult probeResult = UrlProbe::probeSocksWithRetry(
                    probeEntry.socksPort,
                    probeEntry.url,
                    kBatchProbeTimeoutMs,
                    cancelled);
                return SpeedTestServiceInternal::formatUrlProbeResult(probeResult);
            })});

        while (!cancelled.load() && pending.size() >= kBatchProbeMaxConcurrency) {
            collectReadyProbes();
            if (pending.size() >= kBatchProbeMaxConcurrency) {
                QThread::msleep(25);
            }
        }
    }

    while (!pending.empty()) {
        collectReadyProbes();
        if (!pending.empty()) {
            QThread::msleep(25);
        }
    }

    RuntimeProcess::stopProcess(coreProcess);
    releaseAllPorts();
    return true;
}
