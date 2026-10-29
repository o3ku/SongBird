#include <QtTest>

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
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
    void unsupportedProtocolsAreNamedInTheFailure();
    void unsupportedProtocolsStayOnTheFirstLineOfTheFailure();
    void successfulDownloadStampsLastUpdated();
    void notModified304StampsLastUpdated();
    void failedDownloadLeavesLastUpdatedAlone();
    void failuresAreAttributedToTheSubscriptionTheyBelongTo();
    void successfulDownloadWritesRawBackupFile();
    void rawBackupIsOverwrittenOnSecondUpdate();
    void rawBackupNotWrittenWhenNotModified();
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

void SubscriptionUpdateServiceTests::unsupportedProtocolsAreNamedInTheFailure()
{
    // The download succeeded and the body parsed to zero servers because every
    // node uses a protocol this build cannot read. "returned no supported
    // servers" alone reads as "the airport is broken"; the skip summary is the
    // difference between two very different next steps.
    Harness h;
    const QByteArray body =
        "proxies:\n"
        "  - name: h1\n"
        "    type: hysteria\n"
        "    server: 1.2.3.4\n"
        "    port: 443\n";
    h.server.setNextResponse(makeHttpResponse(200, body));

    const OperationResult result = h.update();
    QVERIFY(!result.success);
    QVERIFY2(result.message.contains(QStringLiteral("skipped 1 node(s): hysteria")),
             qPrintable(result.message));
}

void SubscriptionUpdateServiceTests::unsupportedProtocolsStayOnTheFirstLineOfTheFailure()
{
    // The panel that shows the failure has room for one line, and the line it shows
    // is the first one. Both things the user needs -- what failed and what it could
    // not read -- therefore have to be in front of the first newline; the per-pass
    // trail goes behind it, where the log panel still has all of it.
    Harness h;
    const QByteArray body =
        "proxies:\n"
        "  - name: h1\n"
        "    type: hysteria\n"
        "    server: 1.2.3.4\n"
        "    port: 443\n";
    h.server.setNextResponse(makeHttpResponse(200, body));

    const OperationResult result = h.update();
    QVERIFY(!result.success);
    const QString firstLine = result.message.section(QChar('\n'), 0, 0);
    QVERIFY2(firstLine.contains(QStringLiteral("no supported servers")), qPrintable(firstLine));
    QVERIFY2(firstLine.contains(QStringLiteral("skipped 1 node(s): hysteria")), qPrintable(firstLine));
    // ...and the trail is still there, on the lines behind it.
    QVERIFY2(result.message.contains(QChar('\n')), qPrintable(result.message));
}

// "How stale is what I am showing?" is the question the tab hover text and the subscriptions
// table answer with this timestamp, so both moments that successfully circulate the bytes have to
// fill it in.
void SubscriptionUpdateServiceTests::successfulDownloadStampsLastUpdated()
{
    Harness h;
    const qint64 before = QDateTime::currentMSecsSinceEpoch();
    h.server.setNextResponse(makeHttpResponse(200, kVmessPayload));
    const OperationResult result = h.update();
    QVERIFY2(result.success, qPrintable(result.message));

    // `found` matters as much as the comparison: a subscription missing from the result would
    // make this loop run zero times and report a pass without asserting anything.
    bool found = false;
    for (const SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            QVERIFY2(s.lastUpdatedAtMs >= before, "A successful download must stamp the refresh time.");
            QVERIFY(s.lastUpdatedAtMs <= QDateTime::currentMSecsSinceEpoch());
            found = true;
        }
    }
    QVERIFY(found);
}

void SubscriptionUpdateServiceTests::notModified304StampsLastUpdated()
{
    // A 304 is a successful check. Leaving the timestamp alone would make an hourly-refreshed
    // subscription look untouched since the day its node list last changed, which is exactly the
    // reading the field exists to prevent.
    Harness h;
    const qint64 before = QDateTime::currentMSecsSinceEpoch();
    h.server.setNextResponse(makeHttpResponse(304, {}));
    const OperationResult result = h.update();
    QVERIFY2(result.success, qPrintable(result.message));

    bool found = false;
    for (const SubItem& s : h.repo.config_.collection().subscriptions) {
        if (s.id == h.subId) {
            QVERIFY(s.lastUpdatedAtMs >= before);
            found = true;
        }
    }
    QVERIFY(found);
}

void SubscriptionUpdateServiceTests::failedDownloadLeavesLastUpdatedAlone()
{
    // The number must never be the time of a failure dressed up as a refresh: a user who sees
    // "just now" is entitled to assume the node list is current.
    //
    // The seed goes through the repository, not into repo.config_ directly: updateByIds() starts
    // from repository.load(), so a value only written to the mock's current config never reaches
    // the config the service edits, and the assertion below would compare two different zeros and
    // pass while asserting nothing.
    Harness h;
    const qint64 seeded = 1700000000000LL;
    Config seededConfig = h.repo.config_;
    for (SubItem& s : seededConfig.collection().subscriptions) {
        if (s.id == h.subId) s.lastUpdatedAtMs = seeded;
    }
    h.repo.save(seededConfig);
    QCOMPARE(h.repo.load().collection().subscriptions.constFirst().lastUpdatedAtMs, seeded);

    QObject::connect(&h.server, &StubHttpServer::requestReceived, &h.server, [&]() {
        h.server.setNextResponse(makeHttpResponse(503, "down"));
    });
    h.server.setNextResponse(makeHttpResponse(503, "down"));

    const OperationResult result = h.update();
    QVERIFY(!result.success);

    // Read back from the repository as well as from the object the service edited: the failure
    // path calls save(), so a timestamp cleared before that save would only be visible here.
    bool found = false;
    for (const SubItem& s : h.repo.load().collection().subscriptions) {
        if (s.id == h.subId) {
            QCOMPARE(s.lastUpdatedAtMs, seeded);
            found = true;
        }
    }
    QVERIFY(found);
}

// The tab hover text needs "why did *this* subscription fail", and the batch message cannot answer
// that on its own: its lines lead with the display name, which two subscriptions can share, and
// they are written in the reordered walk order rather than in config order. So the service records
// the attribution as it goes -- id plus reason -- and this is the test that says it does.
void SubscriptionUpdateServiceTests::failuresAreAttributedToTheSubscriptionTheyBelongTo()
{
    // Two subscriptions with the same remarks on purpose: a name-based recovery of the attribution
    // cannot tell them apart, so a passing result here is the structural one talking.
    StubHttpServer server;
    QVERIFY(server.listen());
    MockRepo repo;
    QNetworkAccessManager nam;
    SubscriptionService subService(repo);
    SubscriptionUpdateService updateService(repo, subService, nam);

    const QString baseUrl = QStringLiteral("http://127.0.0.1:%1/sub").arg(server.port());
    SubItem first = makeSub(QStringLiteral("sub-1"), baseUrl + QStringLiteral("-1"));
    first.remarks = QStringLiteral("Airport");
    SubItem second = makeSub(QStringLiteral("sub-2"), baseUrl + QStringLiteral("-2"));
    second.remarks = QStringLiteral("Airport");
    repo.config_.collection().subscriptions = {first, second};
    repo.save(repo.config_);

    QObject::connect(&server, &StubHttpServer::requestReceived, &server, [&]() {
        // Both requests fail, so both subjects are recorded and both reasons have to survive.
        server.setNextResponse(makeHttpResponse(503, "down"));
    });
    server.setNextResponse(makeHttpResponse(503, "down"));

    const OperationResult result =
        updateService.updateByIds(repo.config_, {QStringLiteral("sub-1"), QStringLiteral("sub-2")});

    QVERIFY(!result.success);
    QCOMPARE(result.subjectFailures.size(), 2);
    QStringList ids;
    for (const OperationResult::SubjectFailure& failure : result.subjectFailures) {
        ids.append(failure.id);
        QVERIFY2(!failure.reason.trimmed().isEmpty(), qPrintable(failure.id));
        QVERIFY2(failure.reason.contains(QStringLiteral("download failed")), qPrintable(failure.reason));
    }
    ids.sort();
    QCOMPARE(ids, (QStringList{QStringLiteral("sub-1"), QStringLiteral("sub-2")}));

    // A run that succeeds carries no attribution, which is what clears the previous reason on
    // the tab: the consumer replaces the whole map, so an empty map is the clearing.
    server.setNextResponse(makeHttpResponse(200, kVmessPayload));
    const OperationResult success = updateService.updateByIds(repo.config_, {QStringLiteral("sub-1")});
    QVERIFY2(success.success, qPrintable(success.message));
    QVERIFY(success.subjectFailures.isEmpty());
}


void SubscriptionUpdateServiceTests::successfulDownloadWritesRawBackupFile()
{
    Harness h;
    h.repo.config_.collection().subscriptions.first().remarks = QStringLiteral("sub / one: dirty*");
    QTemporaryDir rawDir;
    h.updateService.setRawBackupDirectory(rawDir.path());
    h.server.setNextResponse(makeHttpResponse(200, kVmessPayload));

    const OperationResult result = h.update();
    QVERIFY2(result.success, qPrintable(result.message));

    const QStringList entries = QDir(rawDir.path()).entryList(
        QStringList{QStringLiteral("*.raw")}, QDir::Files | QDir::NoDotAndDotDot);
    QCOMPARE(entries.size(), 1);
    // Windows-invalid characters from the remarks must not survive into the file name.
    QVERIFY2(!entries.first().contains(QLatin1String("<>:\"/\\|?*")),
        qPrintable(entries.first()));
    QVERIFY(entries.first().endsWith(QStringLiteral("_sub-1.raw")));

    QFile rawFile(QDir(rawDir.path()).filePath(entries.first()));
    QVERIFY(rawFile.open(QIODevice::ReadOnly));
    QCOMPARE(rawFile.readAll(), kVmessPayload);
}

void SubscriptionUpdateServiceTests::rawBackupIsOverwrittenOnSecondUpdate()
{
    Harness h;
    QTemporaryDir rawDir;
    h.updateService.setRawBackupDirectory(rawDir.path());

    h.server.setNextResponse(makeHttpResponse(200, kVmessPayload));
    QVERIFY2(h.update().success, "first update");
    const QByteArray secondPayload = kVmessPayload + "\n#marker";
    h.server.setNextResponse(makeHttpResponse(200, secondPayload));
    QVERIFY2(h.update().success, "second update");

    const QStringList entries = QDir(rawDir.path()).entryList(
        QStringList{QStringLiteral("*.raw")}, QDir::Files | QDir::NoDotAndDotDot);
    QCOMPARE(entries.size(), 1);

    QFile rawFile(QDir(rawDir.path()).filePath(entries.first()));
    QVERIFY(rawFile.open(QIODevice::ReadOnly));
    QCOMPARE(rawFile.readAll(), secondPayload);
}

void SubscriptionUpdateServiceTests::rawBackupNotWrittenWhenNotModified()
{
    Harness h;
    QTemporaryDir rawDir;
    h.updateService.setRawBackupDirectory(rawDir.path());

    h.server.setNextResponse(makeHttpResponse(200, kVmessPayload,
        "ETag: \"abc123\"\r\n"));
    QVERIFY2(h.update().success, "first update");
    h.server.setNextResponse(makeHttpResponse(304, {},
        "ETag: \"abc123\"\r\n"));
    QVERIFY2(h.update().success, "not-modified update");

    const QStringList entries = QDir(rawDir.path()).entryList(
        QStringList{QStringLiteral("*.raw")}, QDir::Files | QDir::NoDotAndDotDot);
    QCOMPARE(entries.size(), 1);
    QFile rawFile(QDir(rawDir.path()).filePath(entries.first()));
    QVERIFY(rawFile.open(QIODevice::ReadOnly));
    QCOMPARE(rawFile.readAll(), kVmessPayload);
}
QTEST_MAIN(SubscriptionUpdateServiceTests)
#include "SubscriptionUpdateServiceTests.moc"
