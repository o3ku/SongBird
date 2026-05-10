#include "services/SpeedTestService.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
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
#include <functional>
#include <optional>
#include <utility>

#include "runtime/ClientConfigWriter.h"
#include "services/SpeedTestServiceInternal.h"

namespace {

constexpr int kRuntimeStartupTimeoutMs = 5000;
constexpr int kRuntimeRequestTimeoutMs = 8000;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");
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

QString formatLatencyResult(qint64 milliseconds)
{
    return milliseconds >= 0
        ? QStringLiteral("%1 ms").arg(milliseconds)
        : QStringLiteral("Timeout");
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
        return candidate;
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

QString runUrlTest(
    const SpeedTestRequestItem& item,
    const QString& customConfigDirectory,
    const std::atomic_bool& cancelled,
    const std::function<void(const QString&)>& log)
{
    const QString serverName = describeServer(item.server);
    if (item.coreInfo.program.trimmed().isEmpty() || !QFileInfo::exists(item.coreInfo.program)) {
        return QStringLiteral("Core missing");
    }

    const int socksPort = takeAvailablePortBase();
    if (socksPort <= 0) {
        return QStringLiteral("Port busy");
    }
    const int httpPort = socksPort + 1;

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return QStringLiteral("Temp dir failed");
    }

    Config runtimeConfig = SpeedTestServiceInternal::makeUrlTestRuntimeConfig(item.runtimeConfig);
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

    const QString url = defaultUrlTestUrl(item.runtimeConfig);
    log(QStringLiteral("URL Test proxy ready | %1 | type=%2 | port=%3 | url=%4")
            .arg(serverName)
            .arg(readyProxy->name)
            .arg(readyProxy->port)
            .arg(url));

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(readyProxy->type, kLoopbackAddress, readyProxy->port));

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("v2rayq-urltest"));

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
    timeoutTimer.start(kRuntimeRequestTimeoutMs);
    loop.exec();
    timeoutTimer.stop();

    const QNetworkReply::NetworkError error = reply->error();
    const QString errorText = reply->errorString();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    stopRuntime();

    const bool remoteClosed = error == QNetworkReply::RemoteHostClosedError
        || errorText.contains(QStringLiteral("Connection closed"), Qt::CaseInsensitive);

    if (!timedOut && (statusCode == 200 || statusCode == 204 || remoteClosed)) {
        return formatLatencyResult(timer.elapsed());
    }

    if (!timedOut
        && (error == QNetworkReply::NoError || error == QNetworkReply::OperationCanceledError)
        && statusCode == 0) {
        return formatLatencyResult(timer.elapsed());
    }

    return timedOut ? QStringLiteral("Timeout") : normalizeErrorText(errorText);
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

        for (const SpeedTestRequestItem& item : items) {
            if (cancelled_.load()) {
                break;
            }
            const QString serverName = describeServer(item.server);
            emit logGenerated(QStringLiteral("URL Test: %1").arg(serverName));

            QString result;
            if (item.server.configType == ConfigType::Custom) {
                result = QStringLiteral("Unsupported");
            } else if (cancelled_.load()) {
                break;
            } else {
                result = runUrlTest(item, customConfigDirectory_, cancelled_, [this](const QString& message) {
                    emit logGenerated(message);
                });
            }
            if (result.trimmed().isEmpty()) {
                result = QStringLiteral("Failed");
            }
            ++completed;
            if (!cancelled_.load()) {
                emit testResultReady(item.server.indexId, result);
                emit logGenerated(QStringLiteral("URL Test result | %1 -> %2")
                                      .arg(serverName, result));
            }
        }

        const bool wasCancelled = cancelled_.load();
        emit batchFinished(wasCancelled
            ? QStringLiteral("URL Test cancelled after %1 server(s).").arg(completed)
            : QStringLiteral("URL Test finished for %1 server(s).").arg(completed));
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

    running_ = true;
    cancelled_ = false;
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

    workerThread_->quit();
    workerThread_->wait();
}

#include "SpeedTestService.moc"

