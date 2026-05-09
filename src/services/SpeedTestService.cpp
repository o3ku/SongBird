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

#include <algorithm>
#include <utility>

#include "runtime/ClientConfigWriter.h"

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

bool waitForProxyPort(int port, int timeoutMs, const std::atomic_bool& cancelled)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (cancelled.load()) {
            return false;
        }

        QTcpSocket socket;
        socket.connectToHost(kLoopbackAddress, port);
        if (socket.waitForConnected(200)) {
            socket.disconnectFromHost();
            return true;
        }

        if (cancelled.load()) {
            return false;
        }

        QThread::msleep(50);
    }

    return false;
}

QString runUrlTest(
    const SpeedTestRequestItem& item,
    const Config& config,
    const QString& customConfigDirectory,
    const std::atomic_bool& cancelled)
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
    runtimeConfig.tunModeItem.enableTun = false;

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
        return cancelled.load() ? QStringLiteral("Cancelled") : normalizeErrorText(coreProcess.errorString());
    }

    const auto stopRuntime = [&coreProcess]() {
        coreProcess.terminate();
        if (!coreProcess.waitForFinished(3000)) {
            coreProcess.kill();
            coreProcess.waitForFinished(3000);
        }
    };

    if (!waitForProxyPort(localPort, kRuntimeStartupTimeoutMs, cancelled)) {
        const QString output = QString::fromUtf8(coreProcess.readAll()).trimmed();
        stopRuntime();
        if (cancelled.load()) {
            return QStringLiteral("Cancelled");
        }
        return output.isEmpty()
            ? QStringLiteral("Proxy startup timeout")
            : QStringLiteral("Proxy startup timeout: %1").arg(normalizeErrorText(output));
    }

    const QString url = defaultUrlTestUrl(config);

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy(QNetworkProxy::Socks5Proxy, kLoopbackAddress, localPort));

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
                result = runUrlTest(item, config, customConfigDirectory_, cancelled_);
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

