#include "services/SpeedTestRuntimeRunner.h"

#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <optional>

#include "runtime/ClientConfigWriter.h"
#include "services/SpeedTestPortReservation.h"
#include "services/SpeedTestRuntimeProcess.h"
#include "services/SpeedTestServiceInternal.h"
#include "services/SpeedTestUrlProbe.h"

namespace {

constexpr int kRuntimeStartupTimeoutMs = 5000;
constexpr int kRuntimeRequestTimeoutMs = 3000;
namespace PortPool = SpeedTestPortReservation;
namespace RuntimeProcess = SpeedTestRuntimeProcess;
namespace UrlProbe = SpeedTestUrlProbe;
// gstatic responds with 204 on every working egress and is not blocked from
// most CN exits, so it produces fewer false negatives than google.com.
const QString kDefaultUrlTestUrl = QStringLiteral("https://www.gstatic.com/generate_204");

} // namespace

namespace SpeedTestRuntimeRunner {

QString defaultUrlTestUrl(const Config& config)
{
    return config.defaults().speedPingTestUrl.trimmed().isEmpty()
        ? kDefaultUrlTestUrl
        : config.defaults().speedPingTestUrl.trimmed();
}

QString runUrlTest(
    const Config& probeConfigTemplate,
    const QString& urlTestUrl,
    const SpeedTestRequestItem& item,
    const QString& customConfigDirectory,
    const std::atomic_bool& cancelled,
    const std::function<void(const QString&)>& log)
{
    const QString serverName = item.displayName;
    if (item.coreInfo.program.trimmed().isEmpty() || !QFileInfo::exists(item.coreInfo.program)) {
        return QStringLiteral("Core missing");
    }

    if (cancelled.load()) {
        return QStringLiteral("Cancelled");
    }
    const PortPool::Ports ports = PortPool::takeAvailable();
    if (ports.socksPort <= 0 || ports.httpPort <= 0 || ports.locationProbePort <= 0) {
        return QStringLiteral("Port busy");
    }
    struct ScopedProxyPortRelease
    {
        int socksPort = 0;
        int httpPort = 0;
        int locationProbePort = 0;

        ~ScopedProxyPortRelease()
        {
            PortPool::release(PortPool::Ports{socksPort, httpPort, locationProbePort});
        }
    } reservedPorts{ports.socksPort, ports.httpPort, ports.locationProbePort};

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return QStringLiteral("Temp dir failed");
    }

    Config runtimeConfig = probeConfigTemplate;
    runtimeConfig.localPort = ports.socksPort;
    runtimeConfig.localHttpPort = ports.httpPort;
    runtimeConfig.localLocationProbePort = ports.locationProbePort;

    const QString configPath = temporaryDirectory.filePath(QStringLiteral("runtime.json"));
    ClientConfigWriter clientConfigWriter(customConfigDirectory);
    const OperationResult writeResult = clientConfigWriter.writeClientConfig(runtimeConfig, item.runtimeServer, configPath);
    if (!writeResult.success) {
        return UrlProbe::normalizeErrorText(writeResult.message);
    }

    const QStringList runtimeArguments = RuntimeProcess::buildCoreArguments(item.coreInfo, configPath);
    const QString runtimeConfigPreview = RuntimeProcess::readTextFileIfExists(configPath);

    QProcess coreProcess;
    coreProcess.setProcessChannelMode(QProcess::MergedChannels);
    coreProcess.setProgram(item.coreInfo.program);
    coreProcess.setArguments(runtimeArguments);
    if (!item.coreInfo.workingDirectory.trimmed().isEmpty()) {
        coreProcess.setWorkingDirectory(item.coreInfo.workingDirectory);
    }
    coreProcess.start();
    if (!coreProcess.waitForStarted(kRuntimeStartupTimeoutMs)) {
        const QString errorText = cancelled.load() ? QStringLiteral("Cancelled") : UrlProbe::normalizeErrorText(coreProcess.errorString());
        log(QStringLiteral("URL Test start failed | %1 | %2").arg(serverName, errorText));
        return errorText;
    }

    const std::optional<SpeedTestServiceInternal::ReadyProxy> readyProxy =
        PortPool::waitForProxy(ports.socksPort, ports.httpPort, kRuntimeStartupTimeoutMs, cancelled, [&coreProcess]() {
            return coreProcess.state() == QProcess::NotRunning;
        });
    if (!readyProxy.has_value()) {
        const QString output = RuntimeProcess::readProcessOutput(coreProcess);
        const bool exitedBeforeReady = coreProcess.state() == QProcess::NotRunning;
        const int exitCode = exitedBeforeReady ? coreProcess.exitCode() : -1;
        const QProcess::ExitStatus exitStatus = exitedBeforeReady
            ? coreProcess.exitStatus()
            : QProcess::NormalExit;
        RuntimeProcess::stopProcess(coreProcess);
        if (cancelled.load()) {
            return QStringLiteral("Cancelled");
        }
        const QString outputSummary = RuntimeProcess::summarizeProcessOutput(output);
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
                : QStringLiteral("Proxy exited before listening: %1").arg(UrlProbe::normalizeErrorText(output));
        }

        log(QStringLiteral("URL Test startup timeout | %1 | output=%2")
                .arg(serverName, outputSummary));
        if (!runtimeConfigPreview.isEmpty()) {
            log(QStringLiteral("URL Test config on timeout | %1 | %2")
                    .arg(serverName, RuntimeProcess::summarizeProcessOutput(runtimeConfigPreview)));
        }
        return output.isEmpty()
            ? QStringLiteral("Proxy startup timeout (no core output)")
            : QStringLiteral("Proxy startup timeout: %1").arg(UrlProbe::normalizeErrorText(output));
    }
    const SpeedTestServiceInternal::UrlProbeResult probeResult = UrlProbe::probeReadyProxyWithRetry(
        *readyProxy,
        urlTestUrl,
        kRuntimeRequestTimeoutMs,
        cancelled);

    RuntimeProcess::stopProcess(coreProcess);

    if (probeResult.status == SpeedTestServiceInternal::UrlProbeStatus::Failed
        && !probeResult.errorText.isEmpty()) {
        log(QStringLiteral("URL Test failure detail | %1 | %2")
                .arg(serverName, probeResult.errorText));
    }
    return SpeedTestServiceInternal::formatUrlProbeResult(probeResult);
}

} // namespace SpeedTestRuntimeRunner
