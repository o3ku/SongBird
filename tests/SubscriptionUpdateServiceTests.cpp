#include <QtTest>

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "domain/models/Config.h"
#include "persistence/IConfigRepository.h"
#include "services/SubscriptionService.h"
#include "services/SubscriptionUpdateService.h"

// ---------------------------------------------------------------------------
// Minimal stub HTTP/1.1 server: each request gets a canned response.
// The stub hands out one response per connection; callers configure the next
// response before the next request arrives.
// ---------------------------------------------------------------------------
class StubHttpServer final : public QObject
{
    Q_OBJECT
public:
    explicit StubHttpServer(QObject* parent = nullptr)
        : QObject(parent)
    {
        connect(&server_, &QTcpServer::newConnection, this, &StubHttpServer::onNewConnection);
    }

    bool listen()
    {
        return server_.listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const
    {
        return server_.serverPort();
    }

    // Configure what the server sends for the next request.
    void setNextResponse(const QByteArray& response)
    {
        nextResponse_ = response;
    }

    // Returns the raw bytes of the last request received (headers + body up to \r\n\r\n).
    QByteArray lastRequestHeaders() const
    {
        return lastRequestHeaders_;
    }

    int requestCount() const
    {
        return requestCount_;
    }

    // Block until at least one more request has been fully received, or timeout fires.
    bool waitForRequest(int timeoutMs = 5000)
    {
        if (requestCount_ > lastWaitedCount_) {
            lastWaitedCount_ = requestCount_;
            return true;
        }
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(this, &StubHttpServer::requestReceived, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();
        const bool ok = requestCount_ > lastWaitedCount_;
        lastWaitedCount_ = requestCount_;
        return ok;
    }

signals:
    void requestReceived();

private slots:
    void onNewConnection()
    {
        while (server_.hasPendingConnections()) {
            QTcpSocket* socket = server_.nextPendingConnection();
            socket->setParent(this);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                buffers_[socket].append(socket->readAll());
                if (!buffers_.value(socket).contains("\r\n\r\n")) {
                    return;
                }
                lastRequestHeaders_ = buffers_.take(socket);
                ++requestCount_;
                socket->write(nextResponse_);
                socket->disconnectFromHost();
                emit requestReceived();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        }
    }

private:
    QTcpServer server_;
    QByteArray nextResponse_;
    QByteArray lastRequestHeaders_;
    QHash<QTcpSocket*, QByteArray> buffers_;
    int requestCount_ = 0;
    int lastWaitedCount_ = 0;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

// A valid single-server VMess subscription payload that the parser accepts.
const QByteArray kVmessPayload =
    "vmess://eyJhZGQiOiIxMjcuMC4wLjEiLCJwb3J0IjoiMTIzNDUiLCJpZCI6IjAwMDAwMDAwLTAwMDAt"
    "MDAwMC0wMDAwLTAwMDAwMDAwMDAwMSIsImFsdGVyaWQiOiIwIiwibmV0IjoidGNwIiwidHlwZSI6Im5v"
    "bmUiLCJob3N0IjoiIiwicGF0aCI6IiIsInRscyI6IiIsInBzIjoidGVzdCIsInYiOiIyIn0=";

QByteArray makeHttpResponse(int statusCode, const QByteArray& body,
                            const QByteArray& extraHeaders = {})
{
    const QByteArray statusText = statusCode == 200   ? "OK"
        : statusCode == 304                           ? "Not Modified"
        : statusCode == 503                           ? "Service Unavailable"
                                                      : "Unknown";
    return "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText + "\r\n"
        + "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
        + extraHeaders
        + "Connection: close\r\n\r\n"
        + body;
}

class MockRepo : public IConfigRepository
{
public:
    Config load() override { return config_; }
    bool save(const Config& c) override
    {
        config_ = c;
        ++saveCount;
        return true;
    }
    QString lastLoadError() const override { return {}; }
    QString lastSaveError() const override { return {}; }

    Config config_;
    int saveCount = 0;
};

SubItem makeSub(const QString& id, const QString& url)
{
    SubItem s;
    s.id = id;
    s.remarks = id;
    s.url = url;
    s.enabled = true;
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// Test suite
// ---------------------------------------------------------------------------
class SubscriptionUpdateServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void successfulDownloadImportsServersAndSavesEtag();
    void notModified304ShortCircuitsWithoutReplacingServers();
    void notModifiedSavesRefreshedEtag();
    void retriesOnTransientFailureThenSucceeds();
    void exhaustedRetriesIncrementFailStreak();
    void unparseablePayloadDoesNotCountAsAnUnreachableSubscription();
    void failStreakResetOnSuccess();
    void etagSentOnSubsequentRequest();
    void lastModifiedSentOnSubsequentRequest();
    void invalidUrlFailsImmediately();
};

// ---- helpers shared across slots ----------------------------------------
namespace {

struct Harness {
    StubHttpServer server;
    MockRepo repo;
    QNetworkAccessManager nam;
    SubscriptionService subService{repo};
    SubscriptionUpdateService updateService{repo, subService, nam};

    QString subId  = QStringLiteral("sub-1");
    QString subUrl;

    explicit Harness()
    {
        const bool ok = server.listen();
        Q_ASSERT(ok);
        subUrl = QStringLiteral("http://127.0.0.1:%1/sub").arg(server.port());

        SubItem item = makeSub(subId, subUrl);
        repo.config_.collection().subscriptions.append(item);
    }

    // Run updateByIds on the worker thread that SubscriptionUpdateService expects.
    // (It uses QEventLoop internally, so we must run it from a thread that has an
    // event loop -- Qt Test's main thread qualifies.)
    OperationResult update()
    {
        return updateService.updateByIds(repo.config_, {subId});
    }
};

} // namespace

void SubscriptionUpdateServiceTests::successfulDownloadImportsServersAndSavesEtag()
{
    Harness h;
    h.server.setNextResponse(makeHttpResponse(200, kVmessPayload,
        "ETag: \"abc123\"\r\nLast-Modified: Mon, 01 Jan 2024 00:00:00 GMT\r\n"));

    const OperationResult result = h.update();
    QVERIFY2(result.success, qPrintable(result.message));

    // At least one server imported.
    bool hasServer = false;
    for (const VmessItem& sv : h.repo.config_.collection().servers) {
        if (sv.subId == h.subId) {
            hasServer = true;
            break;
        }
    }
    QVERIFY(hasServer);

    // ETag and Last-Modified persisted on the subscription.
    bool found = false;
    for (const SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            QCOMPARE(s.etag, QStringLiteral("\"abc123\""));
            QCOMPARE(s.lastModified, QStringLiteral("Mon, 01 Jan 2024 00:00:00 GMT"));
            QCOMPARE(s.failStreak, 0);
            found = true;
        }
    }
    QVERIFY(found);
}

void SubscriptionUpdateServiceTests::notModified304ShortCircuitsWithoutReplacingServers()
{
    Harness h;
    // Pre-populate a server so we can check it is NOT removed after 304.
    VmessItem existing;
    existing.indexId = QStringLiteral("pre-existing");
    existing.subId = h.subId;
    existing.configType = ConfigType::VMess;
    h.repo.config_.collection().servers.append(existing);

    h.server.setNextResponse(makeHttpResponse(304, {}));
    const OperationResult result = h.update();

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(result.message.contains(QStringLiteral("not modified"), Qt::CaseInsensitive));

    // Server list unchanged.
    int count = 0;
    for (const VmessItem& sv : h.repo.config_.collection().servers) {
        if (sv.subId == h.subId) ++count;
    }
    QCOMPARE(count, 1);
}

void SubscriptionUpdateServiceTests::notModifiedSavesRefreshedEtag()
{
    Harness h;
    // Seed existing cached headers.
    for (SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            s.etag = QStringLiteral("\"old\"");
            s.lastModified = QStringLiteral("Mon, 01 Jan 2024 00:00:00 GMT");
        }
    }

    // Server sends 304 with a refreshed ETag.
    h.server.setNextResponse(makeHttpResponse(304, {},
        "ETag: \"new\"\r\n"));
    const OperationResult result = h.update();
    QVERIFY2(result.success, qPrintable(result.message));

    for (const SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            // Refreshed ETag must be saved; Last-Modified falls back to the old value.
            QCOMPARE(s.etag, QStringLiteral("\"new\""));
            QCOMPARE(s.failStreak, 0);
        }
    }
}

void SubscriptionUpdateServiceTests::retriesOnTransientFailureThenSucceeds()
{
    Harness h;

    // First two requests fail, third succeeds.
    int callCount = 0;
    const auto failResponse = makeHttpResponse(503, "down");
    const auto okResponse   = makeHttpResponse(200, kVmessPayload);

    // The stub serves one response per connection, so we queue them via a counter.
    // We overwrite nextResponse before each request by observing requestReceived.
    QObject::connect(&h.server, &StubHttpServer::requestReceived, &h.server, [&]() {
        callCount++;
        if (callCount < 3) {
            h.server.setNextResponse(failResponse);
        }
        // No-op on third call; okResponse was already set before the first.
    });

    h.server.setNextResponse(failResponse); // first call
    // Pre-load second failure; overwrite to ok before third.
    // The timer-driven retry approach: between each failure the service sleeps
    // kRetryIntervalMs[n]. The test must let that sleep fire, which happens
    // naturally because Qt Test's main event loop stays alive.

    // We need the second response to be failure and third to be ok.
    // Simplest: a separate server per attempt. But that would require 3 ports.
    // Instead: chain off requestReceived.
    QObject::connect(&h.server, &StubHttpServer::requestReceived, this, [&]() {
        if (h.server.requestCount() == 1) {
            h.server.setNextResponse(failResponse);
        } else if (h.server.requestCount() == 2) {
            h.server.setNextResponse(okResponse);
        }
    });

    h.server.setNextResponse(failResponse);

    const OperationResult result = h.update();
    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(h.server.requestCount() >= 2);
}

void SubscriptionUpdateServiceTests::exhaustedRetriesIncrementFailStreak()
{
    Harness h;
    for (SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) s.failStreak = 2;
    }

    // All attempts will receive 503.
    QObject::connect(&h.server, &StubHttpServer::requestReceived, &h.server, [&]() {
        h.server.setNextResponse(makeHttpResponse(503, "down"));
    });
    h.server.setNextResponse(makeHttpResponse(503, "down"));

    const OperationResult result = h.update();
    QVERIFY(!result.success);

    for (const SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            // failStreak must have been incremented (was 2, now at least 3).
            QVERIFY(s.failStreak > 2);
        }
    }
    QVERIFY(h.server.requestCount() >= 3);
}

void SubscriptionUpdateServiceTests::unparseablePayloadDoesNotCountAsAnUnreachableSubscription()
{
    // The subscription answered in full and the document is simply not one this build
    // can read: retrying the same URL returns the same bytes, so this is not evidence
    // that the URL is unstable. Counting it marked a reachable subscription as
    // failing after three updates, which is a different claim from "no supported
    // servers" and hides the real one.
    Harness h;
    for (SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) s.failStreak = 2;
    }

    // A document the parsers reject, not a transport error: the URL answered 200.
    const QByteArray body = QByteArrayLiteral("{\"unrelated\": true}");
    h.server.setNextResponse(makeHttpResponse(200, body));
    const OperationResult result = h.update();
    QVERIFY2(!result.success, qPrintable(result.message));

    for (const SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            QCOMPARE(s.failStreak, 0);
        }
    }
    // One download, not a retry storm: the transport said 200 the first time.
    QCOMPARE(h.server.requestCount(), 1);
}

void SubscriptionUpdateServiceTests::failStreakResetOnSuccess()
{
    Harness h;
    for (SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) s.failStreak = 5;
    }

    h.server.setNextResponse(makeHttpResponse(200, kVmessPayload));
    const OperationResult result = h.update();
    QVERIFY2(result.success, qPrintable(result.message));

    for (const SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            QCOMPARE(s.failStreak, 0);
        }
    }
}

void SubscriptionUpdateServiceTests::etagSentOnSubsequentRequest()
{
    Harness h;
    for (SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            s.etag = QStringLiteral("\"v1\"");
        }
    }

    h.server.setNextResponse(makeHttpResponse(304, {}));
    h.update();

    const QString headers = QString::fromUtf8(h.server.lastRequestHeaders());
    QVERIFY2(headers.contains(QStringLiteral("If-None-Match: \"v1\"")),
             qPrintable(QStringLiteral("Headers did not contain If-None-Match: ") + headers));
}

void SubscriptionUpdateServiceTests::lastModifiedSentOnSubsequentRequest()
{
    Harness h;
    for (SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            s.lastModified = QStringLiteral("Mon, 01 Jan 2024 00:00:00 GMT");
        }
    }

    h.server.setNextResponse(makeHttpResponse(304, {}));
    h.update();

    const QString headers = QString::fromUtf8(h.server.lastRequestHeaders());
    QVERIFY2(headers.contains(QStringLiteral("If-Modified-Since: Mon, 01 Jan 2024 00:00:00 GMT")),
             qPrintable(QStringLiteral("Headers did not contain If-Modified-Since: ") + headers));
}

void SubscriptionUpdateServiceTests::invalidUrlFailsImmediately()
{
    MockRepo repo;
    QNetworkAccessManager nam;
    SubscriptionService subService(repo);
    SubscriptionUpdateService updateService(repo, subService, nam);

    SubItem item = makeSub(QStringLiteral("bad"), QStringLiteral("not-a-url://??"));
    repo.config_.collection().subscriptions.append(item);

    const OperationResult result = updateService.updateByIds(
        repo.config_, {QStringLiteral("bad")});
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("invalid"), Qt::CaseInsensitive)
            || result.message.contains(QStringLiteral("failed"), Qt::CaseInsensitive));
}

QTEST_MAIN(SubscriptionUpdateServiceTests)
#include "SubscriptionUpdateServiceTests.moc"
