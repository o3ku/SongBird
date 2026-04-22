#include "services/SpeedTestService.h"

#include <QCoreApplication>
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

#include <algorithm>
#include <utility>

#include "runtime/ClientConfigWriter.h"

namespace {

constexpr int kPingTimeoutMs = 6000;
constexpr int kTcpPingTimeoutMs = 5000;
constexpr int kRuntimeStartupTimeoutMs = 5000;
constexpr int kRuntimeRequestTimeoutMs = 8000;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");
const QString kDefaultSpeedDownloadUrl = QStringLiteral("http://cachefly.cachefly.net/10mb.test");
const QString kDefaultSpeedPingUrl = QStringLiteral("https://www.google.com/generate_204");

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

QString formatSpeedResult(qint64 bytesReceived, qint64 elapsedMs)
{
    if (bytesReceived <= 0 || elapsedMs <= 0) {
        return QStringLiteral("Timeout");
    }

    const double seconds = std::max(0.001, static_cast<double>(elapsedMs) / 1000.0);
    const double megabytesPerSecond = static_cast<double>(bytesReceived) / (1024.0 * 1024.0 * seconds);
    return QStringLiteral("%1 MB/s").arg(QString::number(megabytesPerSecond, 'f', 2));
}

QString defaultSpeedDownloadUrl(const Config& config)
{
    return config.speedTestUrl.trimmed().isEmpty()
        ? kDefaultSpeedDownloadUrl
        : config.speedTestUrl.trimmed();
}

QString defaultSpeedPingUrl(const Config& config)
{
    return config.speedPingTestUrl.trimmed().isEmpty()
        ? kDefaultSpeedPingUrl
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

QString modeDisplayName(SpeedTestMode mode)
{
    switch (mode) {
    case SpeedTestMode::Ping:
        return QStringLiteral("Ping");
    case SpeedTestMode::TcpPing:
        return QStringLiteral("TCP Ping");
    case SpeedTestMode::RealPing:
        return QStringLiteral("Real Ping");
    case SpeedTestMode::DownloadSpeed:
        return QStringLiteral("Download Speedtest");
    default:
        return QStringLiteral("Speed Test");
    }
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

QString executePingProcess(const QString& address)
{
    const QString trimmedAddress = address.trimmed();
    if (trimmedAddress.isEmpty()) {
        return QStringLiteral("Address missing");
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProgram(QStringLiteral("ping"));
#ifdef Q_OS_WIN
    process.setArguments(QStringList{
        QStringLiteral("-n"),
        QStringLiteral("1"),
        QStringLiteral("-w"),
        QString::number(kTcpPingTimeoutMs),
        trimmedAddress});
#else
    process.setArguments(QStringList{
        QStringLiteral("-c"),
        QStringLiteral("1"),
        QStringLiteral("-W"),
        QStringLiteral("5"),
        trimmedAddress});
#endif

    process.start();
    if (!process.waitForStarted(1000)) {
        return QStringLiteral("Ping unavailable");
    }
    if (!process.waitForFinished(kPingTimeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        return QStringLiteral("Timeout");
    }

    const QString output = QString::fromLocal8Bit(process.readAll()).trimmed();
    const QRegularExpression latencyPattern(
        QStringLiteral("(?:time|时间)\\s*[=<]\\s*(\\d+)\\s*ms"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression averagePattern(
        QStringLiteral("(?:Average|平均)\\D*(\\d+)\\s*ms"),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch latencyMatch = latencyPattern.match(output);
    if (latencyMatch.hasMatch()) {
        return formatLatencyResult(latencyMatch.captured(1).toLongLong());
    }

    const QRegularExpressionMatch averageMatch = averagePattern.match(output);
    if (averageMatch.hasMatch()) {
        return formatLatencyResult(averageMatch.captured(1).toLongLong());
    }

    if (output.contains(QStringLiteral("<1ms"), Qt::CaseInsensitive)
        || output.contains(QStringLiteral("时间<1ms"))) {
        return formatLatencyResult(1);
    }

    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        return QStringLiteral("Reachable");
    }

    return normalizeErrorText(output);
}

bool waitForProxyPort(int port, int timeoutMs)
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

        QThread::msleep(50);
    }

    return false;
}

QString runTcpPingTest(const VmessItem& server)
{
    QTcpSocket socket;
    QElapsedTimer timer;
    timer.start();
    socket.connectToHost(server.address.trimmed(), server.port);
    if (!socket.waitForConnected(kTcpPingTimeoutMs)) {
        return socket.error() == QAbstractSocket::SocketTimeoutError
            ? QStringLiteral("Timeout")
            : normalizeErrorText(socket.errorString());
    }

    socket.disconnectFromHost();
    return formatLatencyResult(timer.elapsed());
}

QString runRuntimeProxyRequest(
    const SpeedTestRequestItem& item,
    const Config& config,
    const QString& customConfigDirectory,
    const QString& url,
    bool downloadMode)
{
    if (item.coreInfo.program.trimmed().isEmpty() || !QFileInfo::exists(item.coreInfo.program)) {
        return QStringLiteral("Core missing");
    }

    const int localPort = takeAvailablePortBase();
    if (localPort <= 0) {
        return QStringLiteral("Port busy");
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return QStringLiteral("Temp dir failed");
    }

    Config runtimeConfig = config;
    runtimeConfig.localPort = localPort;
    runtimeConfig.allowLanConnection = false;
    runtimeConfig.enableStatistics = false;
    runtimeConfig.logEnabled = false;

    const QString configPath = temporaryDirectory.filePath(QStringLiteral("runtime.json"));
    ClientConfigWriter clientConfigWriter(customConfigDirectory);
    const OperationResult writeResult = clientConfigWriter.writeClientConfig(runtimeConfig, item.server, configPath, 0);
    if (!writeResult.success) {
        return normalizeErrorText(writeResult.message);
    }

    QProcess coreProcess;
    coreProcess.setProcessChannelMode(QProcess::MergedChannels);
    coreProcess.setProgram(item.coreInfo.program);
    coreProcess.setArguments(buildCoreArguments(item.coreInfo, configPath));
    coreProcess.start();
    if (!coreProcess.waitForStarted(kRuntimeStartupTimeoutMs)) {
        return normalizeErrorText(coreProcess.errorString());
    }

    const auto stopRuntime = [&coreProcess]() {
        coreProcess.terminate();
        if (!coreProcess.waitForFinished(3000)) {
            coreProcess.kill();
            coreProcess.waitForFinished(3000);
        }
    };

    if (!waitForProxyPort(localPort, kRuntimeStartupTimeoutMs)) {
        const QString output = QString::fromUtf8(coreProcess.readAll()).trimmed();
        stopRuntime();
        return output.isEmpty()
            ? QStringLiteral("Proxy startup timeout")
            : normalizeErrorText(output);
    }

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(QNetworkProxy::Socks5Proxy, kLoopbackAddress, localPort));

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("v2rayq-speedtest"));

    QElapsedTimer timer;
    timer.start();

    qint64 bytesReceived = 0;
    qint64 streamedBytes = 0;
    bool timedOut = false;
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::downloadProgress, &loop, [&bytesReceived](qint64 received, qint64) {
        bytesReceived = std::max(bytesReceived, received);
    });
    QObject::connect(reply, &QIODevice::readyRead, &loop, [&]() {
        streamedBytes += reply->readAll().size();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(kRuntimeRequestTimeoutMs);
    loop.exec();
    timeoutTimer.stop();

    streamedBytes += reply->readAll().size();
    bytesReceived = std::max(bytesReceived, streamedBytes);
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorText = reply->errorString();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    stopRuntime();

    if (downloadMode) {
        if (bytesReceived > 0) {
            return formatSpeedResult(bytesReceived, timer.elapsed());
        }
        return timedOut ? QStringLiteral("Timeout") : normalizeErrorText(errorText);
    }

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

QString runRealPingTest(
    const SpeedTestRequestItem& item,
    const Config& config,
    const QString& customConfigDirectory)
{
    return runRuntimeProxyRequest(item, config, customConfigDirectory, defaultSpeedPingUrl(config), false);
}

QString runDownloadSpeedTest(
    const SpeedTestRequestItem& item,
    const Config& config,
    const QString& customConfigDirectory)
{
    return runRuntimeProxyRequest(item, config, customConfigDirectory, defaultSpeedDownloadUrl(config), true);
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

    void runBatch(SpeedTestMode mode, const Config& config, const QList<SpeedTestRequestItem>& items)
    {
        int completed = 0;
        for (const SpeedTestRequestItem& item : items) {
            if (cancelled_.load()) {
                break;
            }

            emit logGenerated(QStringLiteral("%1: %2").arg(modeDisplayName(mode), describeServer(item.server)));

            QString result;
            if (item.server.configType == ConfigType::Custom) {
                result = QStringLiteral("Unsupported");
            } else {
                switch (mode) {
                case SpeedTestMode::Ping:
                    result = executePingProcess(item.server.address);
                    break;
                case SpeedTestMode::TcpPing:
                    result = runTcpPingTest(item.server);
                    break;
                case SpeedTestMode::RealPing:
                    result = runRealPingTest(item, config, customConfigDirectory_);
                    break;
                case SpeedTestMode::DownloadSpeed:
                    result = runDownloadSpeedTest(item, config, customConfigDirectory_);
                    break;
                }
            }

            if (result.trimmed().isEmpty()) {
                result = QStringLiteral("Failed");
            }

            ++completed;
            emit testResultReady(item.server.indexId, result);
            emit logGenerated(QStringLiteral("%1 result | %2 -> %3")
                                  .arg(modeDisplayName(mode), describeServer(item.server), result));
        }

        const bool wasCancelled = cancelled_.load();
        emit batchFinished(wasCancelled
            ? QStringLiteral("%1 cancelled after %2 server(s).").arg(modeDisplayName(mode)).arg(completed)
            : QStringLiteral("%1 finished for %2 server(s).").arg(modeDisplayName(mode)).arg(completed));
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

OperationResult SpeedTestService::start(SpeedTestMode mode, const Config& config, const QList<SpeedTestRequestItem>& items)
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
    const QString message = QStringLiteral("%1 started for %2 server(s).")
                                .arg(modeDisplayName(mode))
                                .arg(items.size());
    emit runningChanged(true);
    emit logGenerated(message);

    auto* worker = static_cast<SpeedTestWorker*>(worker_);
    const bool invoked = QMetaObject::invokeMethod(
        worker,
        [worker, mode, config, items]() {
            worker->runBatch(mode, config, items);
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
