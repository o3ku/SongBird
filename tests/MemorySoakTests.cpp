#include <QtTest>

#include <QApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QNetworkProxy>
#include <QSslSocket>
#include <QTcpServer>
#include <QThreadPool>
#include <QProcess>
#include <QThread>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QFont>


#include "common/LogFileWriter.h"
#include "domain/models/Config.h"
#include "domain/models/RuntimeState.h"
#include "persistence/JsonConfigSerialization.h"
#include "persistence/JsonConfigStateSerialization.h"
#include "runtime/CoreProcessOutputBuffer.h"
#include "services/ProxyAvailabilityCheckService.h"
#include "subscription/SubscriptionContentParser.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/models/LogListModel.h"
#include "ui/theme/AppTheme.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace {

// Process-committed bytes, the number Task Manager shows as "Commit size" and the
// one that grows when something is retained. WorkingSet also works but is noisier:
// the OS trims it at will and a leak can hide behind a trim.
qint64 committedBytes()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters{};
    GetProcessMemoryInfo(GetCurrentProcess(),
                         reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                         sizeof(counters));
    return static_cast<qint64>(counters.PrivateUsage);
#else
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QByteArray text = status.readAll();
    const int start = text.indexOf("VmSize:");
    if (start < 0) {
        return 0;
    }
    return text.mid(start + 7, text.indexOf('\n', start) - start - 7).trimmed().toLongLong() * 1024;
#endif
}

double megabytes(qint64 bytes)
{
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

QString formatDelta(qint64 before, qint64 after)
{
    return QStringLiteral("growth %1 -> %2 MB (delta %3 MB)")
        .arg(QString::number(megabytes(before), 'f', 1),
             QString::number(megabytes(after), 'f', 1),
             QString::number(megabytes(after - before), 'f', 1));
}

VmessItem soakServer(int index)
{
    VmessItem item;
    item.indexId = QStringLiteral("soak-%1").arg(index);
    item.remarks = QStringLiteral("Soak server %1").arg(index);
    item.address = QStringLiteral("192.0.2.%1").arg(index % 254 + 1);
    item.port = 443 + (index % 1000);
    item.configType = ConfigType::Trojan;
    item.id = QStringLiteral("9a732b8e-%1-4c1e-9d2a-00000000000%1").arg(index % 10);
    item.coreType = CoreType::SingBox;
    item.subId = QStringLiteral("sub-soak");
    return item;
}

Config soakConfig(int serverCount)
{
    Config config;
    SubItem subscription;
    subscription.id = QStringLiteral("sub-soak");
    subscription.remarks = QStringLiteral("Soak subscription");
    subscription.url = QStringLiteral("https://example.com/sub");
    config.collectionState.subscriptions.append(subscription);

    for (int index = 0; index < serverCount; ++index) {
        config.collectionState.servers.append(soakServer(index));
    }
    config.currentIndexId = config.collectionState.servers.constFirst().indexId;
    return config;
}

// A local 204 responder, so the probe phases measure the client side (the
// QNetworkAccessManager and reply lifecycle) rather than any network.
// A local 204 responder, so the probe phases measure the client side (the
// QNetworkAccessManager and reply lifecycle) rather than any network.
class Probe204Server final {
public:
    bool start()
    {
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            return false;
        }
        QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this]() {
            while (server_.hasPendingConnections()) {
                QTcpSocket* socket = server_.nextPendingConnection();
                sockets_.append(socket);
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                    socket->readAll();
                    socket->write(
                        "HTTP/1.1 204 No Content\r\n"
                        "Content-Length: 0\r\n"
                        "Connection: close\r\n\r\n");
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, [this, socket]() {
                    sockets_.removeOne(socket);
                    socket->deleteLater();
                });
            }
        });
        return true;
    }

    QUrl url() const
    {
        QUrl result;
        result.setScheme(QStringLiteral("http"));
        result.setHost(server_.serverAddress().toString());
        result.setPort(server_.serverPort());
        result.setPath(QStringLiteral("/generate_204"));
        return result;
    }

    ~Probe204Server()
    {
        for (QTcpSocket* socket : sockets_) {
            socket->disconnectFromHost();
        }
    }

private:
    QTcpServer server_;
    QList<QTcpSocket*> sockets_;
};
// One probe as SpeedTestUrlProbe::probeProxiedUrl performs it: a fresh manager,
// a local event loop, reply deleteLater(). The per-probe manager is the exact
// production shape, kept here so the soak measures what the app really does.
void probeOnce(const QUrl& url)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(5000);
    loop.exec();
    reply->deleteLater();
}

} // namespace

// A soak harness, not a unit test: every case runs one of the app's periodic
// workloads in a tight loop and reports the process-committed growth between the
// warmup and the end. Labelled "smoke" so the PR gate's `ctest -LE smoke` skips
// it; run it manually (a few minutes) when chasing a long-uptime memory report.
// The thresholds are deliberately generous -- they fail only on retention that
// scales with iterations, never on allocator noise.
class MemorySoakTests final : public QObject {
    Q_OBJECT

private slots:
    void logPanelChurn();
    void coreOutputBufferChurn();
    void serverTableRefreshChurn();
    void runtimeStateApplyChurn();
    void freshManagerProbeChurn();
    void sharedManagerProbeChurn();
    void configSerializationChurn();
    void subscriptionParseChurn();
    void themedIconChurn();
    void qprocessSpawnChurn();
    void backgroundThreadChurn();
    void timeoutAbortProbeChurn();
    void availabilityCheckChurn();
    void availabilityTimeoutChurn();
    void probeShapeBisect();

private:

};

void MemorySoakTests::logPanelChurn()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LogFileWriter writer(directory.path(), QStringLiteral("soak"), 3, 4 * 1024 * 1024);

    MainWindow window;
    window.setLogFileWriter(&writer);

    const int warmup = 2000;
    const int iterations = 200000;
    const QString line = QStringLiteral(
        "2024-01-01T00:00:00 INFO core listening socks inbound 127.0.0.1:10808 "
        "with a realistically long diagnostic sentence so the wrap path measures too");

    for (int i = 0; i < warmup; ++i) {
        window.appendLog(line);
    }
    qApp->processEvents();

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        window.appendLog(line);
        if (i % 1000 == 0) {
            qApp->processEvents();
        }
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "logPanelChurn:" << iterations << "lines," << formatDelta(before, after)
            << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("log panel grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::coreOutputBufferChurn()
{
    LogListModel model;
    model.setWrapConfiguration(QFont(QStringLiteral("Segoe UI")), 600);

    CoreProcessOutputBuffer buffer;
    const int warmup = 2000;
    const int iterations = 100000;
    QByteArray chunk;
    for (int i = 0; i < 64; ++i) {
        chunk += "2024-01-01T00:00:00 core says something about a connection attempt\n";
    }

    const auto sink = [&model](const QString& line) {
        model.appendLine(line);
    };

    for (int i = 0; i < warmup; ++i) {
        buffer.append(CoreProcessOutputBuffer::Channel::StandardOutput, chunk, sink);
    }

    const qint64 before = committedBytes();
    for (int i = 0; i < iterations; ++i) {
        buffer.append(CoreProcessOutputBuffer::Channel::StandardOutput, chunk, sink);
    }
    const qint64 after = committedBytes();

    qInfo() << "coreOutputBufferChurn:" << iterations << "chunks of 64 lines,"
            << formatDelta(before, after);
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("core output path grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::serverTableRefreshChurn()
{
    const Config config = soakConfig(600);
    MainWindow window;
    window.setConfig(config);

    const int warmup = 200;
    const int iterations = 5000;

    for (int i = 0; i < warmup; ++i) {
        window.setConfig(config);
    }
    qApp->processEvents();

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        // Same list, one field changed, the way a probe-result write lands.
        Config updated = config;
        updated.collectionState.servers[i % updated.collectionState.servers.size()].testResult = QStringLiteral("123 ms");
        window.setConfig(updated);
        if (i % 100 == 0) {
            qApp->processEvents();
        }
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "serverTableRefreshChurn:" << iterations << "config applies,"
            << formatDelta(before, after) << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 48 * 1024 * 1024,
             QStringLiteral("server table refresh grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::runtimeStateApplyChurn()
{
    MainWindow window;

    RuntimeStateSnapshot snapshot;
    snapshot.currentServerName = QStringLiteral("Soak server 7");
    snapshot.currentServerLocation = QStringLiteral("JP");
    snapshot.currentServerWarning = QStringLiteral("warning sentence");
    snapshot.routingSummary = QStringLiteral("Global");
    snapshot.listenSummary = QStringLiteral("127.0.0.1:10808");
    snapshot.proxyUiState = ProxyUiState::Active;
    snapshot.systemProxyApplied = true;

    const int warmup = 1000;
    const int iterations = 100000;
    for (int i = 0; i < warmup; ++i) {
        window.applyRuntimeState(snapshot);
    }
    qApp->processEvents();

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        snapshot.currentServerWarning = (i % 2 == 0)
            ? QStringLiteral("warning sentence %1").arg(i)
            : QString();
        window.applyRuntimeState(snapshot);
        if (i % 1000 == 0) {
            qApp->processEvents();
        }
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "runtimeStateApplyChurn:" << iterations << "snapshots,"
            << formatDelta(before, after) << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("runtime state apply grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::freshManagerProbeChurn()
{
    Probe204Server server;
    QVERIFY(server.start());
    const QUrl url = server.url();

    const int warmup = 100;
    const int iterations = 20000;
    for (int i = 0; i < warmup; ++i) {
        probeOnce(url);
    }
    qApp->processEvents();

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        probeOnce(url);
        if (i % 200 == 0) {
            qApp->processEvents();
        }
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "freshManagerProbeChurn:" << iterations << "probes (manager per probe),"
            << formatDelta(before, after) << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 64 * 1024 * 1024,
             QStringLiteral("per-probe manager grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::sharedManagerProbeChurn()
{
    Probe204Server server;
    QVERIFY(server.start());
    const QUrl url = server.url();

    QNetworkAccessManager manager;
    manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    const int warmup = 100;
    const int iterations = 20000;
    const auto probeOnceShared = [&manager](const QUrl& probeUrl) {
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QNetworkReply* reply = manager.get(QNetworkRequest(probeUrl));
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            reply->abort();
            loop.quit();
        });
        timeoutTimer.start(5000);
        loop.exec();
        reply->deleteLater();
    };

    for (int i = 0; i < warmup; ++i) {
        probeOnceShared(url);
    }
    qApp->processEvents();

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        probeOnceShared(url);
        if (i % 200 == 0) {
            qApp->processEvents();
        }
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "sharedManagerProbeChurn:" << iterations << "probes (one manager),"
            << formatDelta(before, after) << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 64 * 1024 * 1024,
             QStringLiteral("shared manager grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::configSerializationChurn()
{
    const Config config = soakConfig(600);

    QJsonObject root;
    JsonConfigSerialization::applyConfig(root, config);

    const int warmup = 100;
    const int iterations = 2000;
    for (int i = 0; i < warmup; ++i) {
        QJsonObject roundTrip = root;
        const Config parsed = JsonConfigSerialization::parseConfig(roundTrip);
        QJsonObject stateRoot;
        JsonConfigStateSerialization::write(stateRoot, parsed);
        QJsonDocument document(stateRoot);
        const QByteArray bytes = document.toJson(QJsonDocument::Compact);
        QVERIFY(!bytes.isEmpty());
    }

    const qint64 before = committedBytes();
    for (int i = 0; i < iterations; ++i) {
        QJsonObject roundTrip = root;
        const Config parsed = JsonConfigSerialization::parseConfig(roundTrip);
        QJsonObject stateRoot;
        JsonConfigStateSerialization::write(stateRoot, parsed);
        const QByteArray bytes = QJsonDocument(stateRoot).toJson(QJsonDocument::Compact);
        QVERIFY(!bytes.isEmpty());
    }
    const qint64 after = committedBytes();

    qInfo() << "configSerializationChurn:" << iterations << "round trips,"
            << formatDelta(before, after);
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("config serialization grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::subscriptionParseChurn()
{
    QStringList shareLines;
    for (int index = 0; index < 600; ++index) {
        const VmessItem item = soakServer(index);
        shareLines.append(QStringLiteral("trojan://%1@example.com:443?sni=example.com#%2")
                              .arg(item.id, item.remarks));
    }
    const QString content = shareLines.join(QLatin1Char('\n')).toUtf8().toBase64();

    const int warmup = 20;
    const int iterations = 500;
    for (int i = 0; i < warmup; ++i) {
        const QList<VmessItem> items = SubscriptionContentParser::parseMany(content);
        QVERIFY(!items.isEmpty());
    }

    const qint64 before = committedBytes();
    for (int i = 0; i < iterations; ++i) {
        const QList<VmessItem> items = SubscriptionContentParser::parseMany(content);
        QVERIFY(!items.isEmpty());
    }
    const qint64 after = committedBytes();

    qInfo() << "subscriptionParseChurn:" << iterations << "parses of 600 nodes,"
            << formatDelta(before, after);
    QVERIFY2(after - before < 48 * 1024 * 1024,
             QStringLiteral("subscription parse grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::themedIconChurn()
{
    const int warmup = 1000;
    const int iterations = 100000;
    const auto renderOnce = []() {
        // The two tinted lookups the toolbar and tray rebuild on theme and
        // status changes; the cache behind them is what this soak watches.
        const QIcon text = AppTheme::themedSvgIcon(QStringLiteral(":/app/logo.svg"));
        const QIcon accent = AppTheme::accentSvgIcon(QStringLiteral(":/app/logo.svg"));
        QVERIFY(!text.isNull() || !accent.isNull());
    };

    for (int i = 0; i < warmup; ++i) {
        renderOnce();
    }

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        renderOnce();
    }
    const qint64 after = committedBytes();

    qInfo() << "themedIconChurn:" << iterations << "icon lookups," << formatDelta(before, after)
            << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("themed icons grew: %1").arg(formatDelta(before, after)).toUtf8());
}

QTEST_MAIN(MemorySoakTests)
void MemorySoakTests::timeoutAbortProbeChurn()
{
    // The availability check's shape on an https speedPingTestUrl against a
    // dead node: a fresh QNetworkAccessManager, a request that never completes,
    // the timeout aborting it, reply deleteLater(). An unroutable address gives
    // exactly that without any network dependency.
    qInfo() << "TLS backend:" << QSslSocket::sslLibraryBuildVersionString()
            << "loaded:" << QSslSocket::sslLibraryVersionNumber();

    QUrl deadUrl;
    deadUrl.setScheme(QStringLiteral("https"));
    deadUrl.setHost(QStringLiteral("10.255.255.1"));
    deadUrl.setPort(81);
    deadUrl.setPath(QStringLiteral("/generate_204"));

    const auto probeOnceDead = [&deadUrl](int timeoutMs) {
        QNetworkAccessManager manager;
        QNetworkRequest request(deadUrl);

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QNetworkReply* reply = manager.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            reply->abort();
            loop.quit();
        });
        timeoutTimer.start(timeoutMs);
        loop.exec();
        reply->deleteLater();
    };

    const int warmup = 50;
    const int iterations = 4000;
    for (int i = 0; i < warmup; ++i) {
        probeOnceDead(150);
    }
    qApp->processEvents();

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        probeOnceDead(150);
        if (i % 100 == 0) {
            qApp->processEvents();
        }
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "timeoutAbortProbeChurn:" << iterations << "aborted probes," << formatDelta(before, after)
            << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 64 * 1024 * 1024,
             QStringLiteral("aborted probe path grew: %1").arg(formatDelta(before, after)).toUtf8());
}

namespace {

// The health watch's dispatch shape since the 24-hour memory fix: one pooled
// thread that never expires, with the check queued onto it and the GUI event
// loop pumped while it runs (the local servers live on the GUI thread). The
// per-check fresh thread this replaced retained memory at up to ~40 MB/hour
// against a dead node on the shipped static-Qt build; this helper keeps the
// soak measuring what ships.
class PooledProbeRunner final {
public:
    PooledProbeRunner()
    {
        pool_.setMaxThreadCount(1);
        pool_.setExpiryTimeout(-1);
    }

    ~PooledProbeRunner()
    {
        pool_.clear();
        if (!pool_.waitForDone(2000)) {
            // Abandon rather than block: the pool's thread expires on its own.
            pool_.setExpiryTimeout(100);
        }
    }

    void run(const std::function<void()>& task)
    {
        pool_.start(task);
    }

    void drain()
    {
        while (pool_.activeThreadCount() > 0) {
            qApp->processEvents();
            QThread::msleep(1);
        }
        qApp->processEvents();
    }

private:
    QThreadPool pool_;
};

// A TCP endpoint that accepts and swallows everything: the local stand-in for a
// core whose node is dead, the state the live soak measured growing.
class BlackholeProxyServer final {
public:
    bool start()
    {
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            return false;
        }
        QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this]() {
            while (server_.hasPendingConnections()) {
                QTcpSocket* socket = server_.nextPendingConnection();
                sockets_.append(socket);
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                    socket->readAll(); // swallow, never answer
                });
                // A real core closes its side when the client aborts; without
                // this the server would hoard every accepted socket and the
                // measurement would count the harness, not the app path.
                QObject::connect(socket, &QTcpSocket::disconnected, socket, [this, socket]() {
                    sockets_.removeOne(socket);
                    socket->deleteLater();
                });
            }
        });
        return true;
    }

    int port() const { return server_.serverPort(); }

    ~BlackholeProxyServer()
    {
        for (QTcpSocket* socket : sockets_) {
            socket->abort();
            socket->deleteLater();
        }
    }

private:
    QTcpServer server_;
    QList<QTcpSocket*> sockets_;
};

} // namespace

// The health watch loop, exactly as production dispatches it: one fresh worker
// thread per check, ProxyAvailabilityCheckService::check inside. The reply
// teardown inside that service is what this case pins -- a deleteLater on the
// dying thread retained ~19 KB per timed-out check (~55 MB/day against a dead
// node, the number that motivated this harness).
void MemorySoakTests::availabilityCheckChurn()
{
    Probe204Server server;
    QVERIFY(server.start());

    ProxyAvailabilityCheckConfig healthyConfig;
    healthyConfig.httpPort = server.url().port();
    healthyConfig.speedPingTestUrl = server.url().toString();
    healthyConfig.timeoutMs = 5000;

    const ProxyAvailabilityCheckService service;

    PooledProbeRunner runner;

    const int warmup = 100;
    const int iterations = 5000;
    for (int i = 0; i < warmup; ++i) {
        runner.run([&service, &healthyConfig]() {
            service.check(healthyConfig);
        });
        runner.drain();
    }

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        runner.run([&service, &healthyConfig]() {
            service.check(healthyConfig);
        });
        runner.drain();
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "availabilityCheckChurn:" << iterations << "checks on the pooled probe thread,"
            << formatDelta(before, after) << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("availability check path grew: %1").arg(formatDelta(before, after)).toUtf8());
}

// The same check against a core whose node never answers: every probe runs out
// its timeout and aborts. This is the failing shape the 24-hour report came
// from, and the one whose reply retention was largest.
void MemorySoakTests::availabilityTimeoutChurn()
{
    BlackholeProxyServer blackhole;
    QVERIFY(blackhole.start());

    ProxyAvailabilityCheckConfig deadConfig;
    deadConfig.httpPort = blackhole.port();
    deadConfig.speedPingTestUrl = QStringLiteral("https://www.google.com/generate_204");
    deadConfig.timeoutMs = 300;

    const ProxyAvailabilityCheckService service;

    PooledProbeRunner runner;

    const int warmup = 50;
    const int iterations = 3000;
    for (int i = 0; i < warmup; ++i) {
        runner.run([&service, &deadConfig]() {
            service.check(deadConfig);
        });
        runner.drain();
    }

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        runner.run([&service, &deadConfig]() {
            service.check(deadConfig);
        });
        runner.drain();
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "availabilityTimeoutChurn:" << iterations << "timed-out checks on the pooled probe thread,"
            << formatDelta(before, after) << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("timed-out availability check path grew: %1").arg(formatDelta(before, after)).toUtf8());
}


// Diagnostic, reports only: the probe pipeline with one variable changed per
// variant (event loop alone, bare connect, in-flight abort, real TLS), so a
// future retention report can be attributed to a layer instead of re-deriving
// the whole matrix. Everything runs on the pooled probe thread, the production
// dispatch shape; the GUI-thread control below exists because "grows off-thread
// but not on-thread" and "grows on both" point at different suspects.
void MemorySoakTests::probeShapeBisect()
{
    BlackholeProxyServer blackhole;
    QVERIFY(blackhole.start());

    const QUrl httpsUrl(QStringLiteral("https://www.google.com/generate_204"));
    const QUrl httpUrl(QStringLiteral("http://www.google.com/generate_204"));

    PooledProbeRunner runner;
    const auto runVariant = [this, &runner](const QString& name, int iterations, const std::function<void()>& probe) {
        for (int i = 0; i < 50; ++i) {
            runner.run(probe);
            runner.drain();
        }
        const qint64 before = committedBytes();
        QElapsedTimer elapsed;
        elapsed.start();
        for (int i = 0; i < iterations; ++i) {
            runner.run(probe);
            runner.drain();
        }
        const qint64 after = committedBytes();
        qInfo().noquote() << "probeShapeBisect/" << name << formatDelta(before, after)
                          << "in" << elapsed.elapsed() << "ms";
    };

    const auto qnamVariant = [](bool useProxy, const QUrl& url, int proxyPort) {
        return [useProxy, url, proxyPort]() {
            QNetworkAccessManager manager;
            if (useProxy) {
                // The blackhole accepts the TCP connection and swallows the
                // request, so the abort lands while the request is in flight --
                // the exact state the real check aborts in against a dead node.
                manager.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), proxyPort));
            }
            QNetworkRequest request(url);
            QEventLoop loop;
            QTimer timeoutTimer;
            timeoutTimer.setSingleShot(true);
            QNetworkReply* reply = manager.get(request);
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
                reply->abort();
                loop.quit();
            });
            timeoutTimer.start(150);
            loop.exec();
            QObject::disconnect(reply, nullptr, nullptr, nullptr);
            if (!reply->isFinished()) {
                reply->abort();
            }
            delete reply;
        };
    };

    const auto socketVariant = [port = blackhole.port()]() {
        QTcpSocket socket;
        socket.connectToHost(QStringLiteral("127.0.0.1"), port);
        if (socket.waitForConnected(150)) {
            socket.write(QByteArrayLiteral("GET /generate_204 HTTP/1.1\r\nHost: www.google.com\r\n\r\n"));
            socket.waitForBytesWritten(150);
            socket.waitForReadyRead(150);
            socket.abort();
        }
    };

    const auto loopOnlyVariant = []() {
        QEventLoop loop;
        QTimer::singleShot(0, &loop, &QEventLoop::quit);
        loop.exec();
    };

    const int port = blackhole.port();
    runVariant(QStringLiteral("pooledthread-eventloop-only"), 500, loopOnlyVariant);
    runVariant(QStringLiteral("pooledthread-rawsocket-inflight-abort"), 500, socketVariant);
    runVariant(QStringLiteral("pooledthread-qnam-proxy-http-inflight-abort"), 500,
        qnamVariant(true, httpUrl, port));
    runVariant(QStringLiteral("pooledthread-qnam-proxy-https-inflight-abort"), 500,
        qnamVariant(true, httpsUrl, port));

    // GUI-thread control: the same connect without any thread involvement.
    Probe204Server guiThreadEndpoint;
    QVERIFY(guiThreadEndpoint.start());
    {
        const int guiPort = guiThreadEndpoint.url().port();
        const auto guiConnectVariant = [guiPort]() {
            QTcpSocket socket;
            socket.connectToHost(QStringLiteral("127.0.0.1"), guiPort);
            socket.waitForConnected(150);
            socket.abort();
        };
        const qint64 before = committedBytes();
        for (int i = 0; i < 500; ++i) {
            guiConnectVariant();
        }
        const qint64 after = committedBytes();
        qInfo().noquote() << "probeShapeBisect/ guithread-connect-204" << formatDelta(before, after);
    }

    // The production probe's TLS shape against a real host (needs the network;
    // an inconclusive line is printed when it is unreachable so the soak stays
    // usable offline).
    const auto tlsVariant = []() {
        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl(QStringLiteral("https://www.gstatic.com/generate_204")));
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QNetworkReply* reply = manager.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            reply->abort();
            loop.quit();
        });
        timeoutTimer.start(8000);
        loop.exec();
        const bool ok = reply->error() == QNetworkReply::NoError;
        QObject::disconnect(reply, nullptr, nullptr, nullptr);
        delete reply;
        if (!ok) {
            qInfo() << "tls probe endpoint unreachable; variant inconclusive";
        }
    };
    runVariant(QStringLiteral("pooledthread-tls-handshake"), 200, tlsVariant);
    QVERIFY(true);
}

void MemorySoakTests::qprocessSpawnChurn()
{
    // The shape of a warm-probe round / speed-test batch core: spawn, wait for
    // start, poll briefly, kill, destroy. Never reading the merged output, as
    // runBatchedGroup does while the batch core is up.
    const int warmup = 20;
    const int iterations = 1000;

    const auto spawnOnce = []() {
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setProgram(QStringLiteral("cmd.exe"));
        process.setArguments({QStringLiteral("/c"), QStringLiteral("ping -n 3 127.0.0.1 > nul")});
        process.start();
        if (!process.waitForStarted(5000)) {
            QProcess::ProcessError error = process.error();
            QVERIFY2(error == QProcess::FailedToStart, "spawn failed unexpectedly");
            return;
        }
        QElapsedTimer dwell;
        dwell.start();
        while (dwell.elapsed() < 150 && process.state() != QProcess::NotRunning) {
            QThread::msleep(10);
        }
        process.kill();
        process.waitForFinished(2000);
    };

    for (int i = 0; i < warmup; ++i) {
        spawnOnce();
    }

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        spawnOnce();
        if (i % 50 == 0) {
            qApp->processEvents();
        }
    }
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "qprocessSpawnChurn:" << iterations << "spawn/kill cycles,"
            << formatDelta(before, after) << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("process spawn path grew: %1").arg(formatDelta(before, after)).toUtf8());
}

void MemorySoakTests::backgroundThreadChurn()
{
    // The health watch / outbound probe pattern: QThread::create, tracked with
    // deleteLater-on-finished, event loop pumped between rounds.
    const int warmup = 200;
    const int iterations = 20000;

    const auto runRound = []() {
        QThread* thread = QThread::create([]() {
            // A token of real work so the thread actually runs before finishing.
            qint64 total = 0;
            for (int i = 0; i < 1000; ++i) {
                total += i;
            }
            return total;
        });
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
    };

    for (int i = 0; i < warmup; ++i) {
        runRound();
        qApp->processEvents();
        QThread::msleep(1);
    }
    qApp->processEvents();
    QThread::msleep(50);
    qApp->processEvents();

    const qint64 before = committedBytes();
    QElapsedTimer elapsed;
    elapsed.start();
    for (int i = 0; i < iterations; ++i) {
        runRound();
        // Production never launches two of these back to back: the health watch
        // fires one every 30 s. Let each thread run to completion and be
        // deleted before the next, or the measurement counts live stacks that
        // simply have not been scheduled yet.
        qApp->processEvents();
        QThread::msleep(1);
    }
    qApp->processEvents();
    QThread::msleep(50);
    qApp->processEvents();
    const qint64 after = committedBytes();

    qInfo() << "backgroundThreadChurn:" << iterations << "tracked threads,"
            << formatDelta(before, after) << "in" << elapsed.elapsed() << "ms";
    QVERIFY2(after - before < 32 * 1024 * 1024,
             QStringLiteral("background thread path grew: %1").arg(formatDelta(before, after)).toUtf8());
}

#include "MemorySoakTests.moc"
