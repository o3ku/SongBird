#include <QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>

#include <utility>

#include "appcore/OutboundLocationProbeService.h"

// Stands in for the core's local http inbound. A plain http:// request gets a body the parser
// cannot turn into a location, so that host reports a reason of its own within milliseconds. A
// CONNECT tunnel is completed and then closed, which fails the https hosts immediately as well --
// deliberately, because whether they even send a CONNECT depends on the build: a Qt without a TLS
// backend aborts them at initialization before the request leaves, while one with a TLS backend
// opens a tunnel here. Answering and hanging up makes both builds fail those hosts straight away,
// so the case below sees every host answer instead of seeing three of them sit on an open tunnel
// until the deadline. That difference is what turned this suite red on CI, where the tunnel stayed
// open and the probe reported a timeout it was never supposed to reach.
class ProbeProxyServer
{
public:
    explicit ProbeProxyServer(QByteArray payload, bool answersPlainRequests = true)
        : payload_(std::move(payload))
        , answersPlainRequests_(answersPlainRequests)
    {
        QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this]() {
            while (QTcpSocket* socket = server_.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    respond(socket);
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, [this, socket]() {
                    requests_.remove(socket);
                    answered_.remove(socket);
                });
            }
        });
    }

    ~ProbeProxyServer()
    {
        // Tear the sockets down here rather than letting ~QTcpServer do it after this destructor
        // body has run: deleting them now happens while the hash and set above are still alive, and
        // unhooking their signals first means none of the handlers above can run at all. The suite
        // has crashed before when a test ended with a request still unanswered, and a socket whose
        // destruction walks a half-torn-down container is exactly how.
        server_.close();
        const QList<QTcpSocket*> sockets = server_.findChildren<QTcpSocket*>();
        for (QTcpSocket* socket : sockets) {
            socket->disconnect();
            delete socket;
        }
    }

    ProbeProxyServer(const ProbeProxyServer&) = delete;
    ProbeProxyServer& operator=(const ProbeProxyServer&) = delete;

    bool listen()
    {
        return server_.listen(QHostAddress::LocalHost, 0);
    }

    int port() const
    {
        return server_.serverPort();
    }

private:
    void respond(QTcpSocket* socket)
    {
        if (answered_.contains(socket)) {
            return;
        }

        requests_[socket].append(socket->readAll());
        const QByteArray& request = requests_[socket];
        if (!request.contains("\r\n\r\n")) {
            return; // header not complete yet
        }
        answered_.insert(socket);

        if (!answersPlainRequests_) {
            return; // accepted and deliberately left unanswered
        }

        if (request.startsWith("CONNECT ")) {
            // Complete the tunnel, then hang up. The client's TLS handshake then fails against a
            // closed connection within milliseconds, instead of waiting on a tunnel nobody will
            // ever answer.
            socket->write("HTTP/1.1 200 Connection established\r\n\r\n");
            socket->flush();
            socket->disconnectFromHost();
            return;
        }

        QByteArray response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + QByteArray::number(payload_.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += payload_;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }

    // Declared before server_ on purpose: members are destroyed in reverse order, so server_ goes
    // first and the sockets it deletes still find a live hash when their disconnected handler runs.
    // The other way round the handler walks a destroyed QHash -- a crash this suite hit when a
    // test ended while a request was still unanswered.
    QByteArray payload_;
    bool answersPlainRequests_ = true;
    QHash<QTcpSocket*, QByteArray> requests_;
    QSet<QTcpSocket*> answered_;
    QTcpServer server_;
};

class OutboundLocationProbeServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void failureKeepsEveryHostReasonInsteadOfOnlyTheDeadline();
    void hostThatNeverAnswersDoesNotExtendTheProbePastItsBudget();
    void locationIsParsedFromASuccessfulProbe();
    void connectTunnelIsCompletedAndClosedSoHttpsHostsFailFast();

private:
    static LocationProbeTimings shortTimings();
};

// The production constants add up to twelve seconds before the deadline is reached, so the
// timeout branch is driven with a budget short enough to assert on.
LocationProbeTimings OutboundLocationProbeServiceTests::shortTimings()
{
    LocationProbeTimings timings;
    timings.perRequestTimeoutMs = 400;
    timings.retryDelayMs = 10;
    timings.totalTimeoutMs = 450;
    timings.maxRounds = 1;
    return timings;
}

void OutboundLocationProbeServiceTests::failureKeepsEveryHostReasonInsteadOfOnlyTheDeadline()
{
    ProbeProxyServer proxy(QByteArrayLiteral(R"({"status":"fail"})"));
    QVERIFY(proxy.listen());

    OutboundLocationProbeService probe;
    QElapsedTimer elapsed;
    elapsed.start();
    const OutboundLocationDetails details = probe.probeStructured(proxy.port(), shortTimings());
    const qint64 spent = elapsed.elapsed();

    QVERIFY(details.location.isEmpty());
    // Every host answered -- two with a body the parser rejects, the rest failing outright -- so
    // all of their reasons are known and none of them may be dropped in favour of one generic line.
    QVERIFY2(details.error.contains(QStringLiteral("ip-api.com")), qPrintable(details.error));
    QVERIFY2(details.error.contains(QStringLiteral("ipwho.is")), qPrintable(details.error));
    // Nothing was still in flight when those answers arrived, so nothing timed out: claiming a
    // timeout would be a lie, and sitting out the deadline to be able to tell it is pure delay.
    QVERIFY2(
        !details.error.contains(QCoreApplication::translate(
            "OutboundLocationProbeService", "Outbound location request timed out.")),
        qPrintable(details.error));
    QVERIFY2(spent < 300, qPrintable(QString::number(spent)));
}

// A host that accepts the connection and then says nothing is the case the deadline exists for.
// This proxy answers nothing at all, so every host hangs and the deadline is the only thing that
// can end the wait -- and it has to say so, otherwise a hung host reads as silence.
void OutboundLocationProbeServiceTests::hostThatNeverAnswersDoesNotExtendTheProbePastItsBudget()
{
    ProbeProxyServer proxy(QByteArrayLiteral(R"({"status":"success"})"), false);
    QVERIFY(proxy.listen());

    OutboundLocationProbeService probe;
    QElapsedTimer elapsed;
    elapsed.start();
    const OutboundLocationDetails details = probe.probeStructured(proxy.port(), shortTimings());
    const qint64 spent = elapsed.elapsed();

    QVERIFY(details.location.isEmpty());
    QVERIFY2(
        details.error.contains(QCoreApplication::translate(
            "OutboundLocationProbeService", "Outbound location request timed out.")),
        qPrintable(details.error));
    QVERIFY2(spent >= 300, qPrintable(QString::number(spent)));
    QVERIFY2(spent < 2000, qPrintable(QString::number(spent)));
}

void OutboundLocationProbeServiceTests::locationIsParsedFromASuccessfulProbe()
{
    ProbeProxyServer proxy(QByteArrayLiteral(
        R"({"status":"success","country":"Japan","countryCode":"JP","city":"Tokyo","query":"203.0.113.9"})"));
    QVERIFY(proxy.listen());

    OutboundLocationProbeService probe;
    const OutboundLocationDetails details = probe.probeStructured(proxy.port(), shortTimings());

    QCOMPARE(details.countryCode, QStringLiteral("JP"));
    QVERIFY2(details.location.contains(QStringLiteral("Tokyo")), qPrintable(details.location));
    QVERIFY(details.error.isEmpty());
}

// The https hosts are the ones whose behaviour depends on the build: with a TLS backend they send a
// CONNECT, without one they abort before sending anything. So the tunnel handling above cannot be
// reached through the probe on a Qt without a TLS backend, and this is the only place it can be
// checked there -- by driving the proxy directly.
void OutboundLocationProbeServiceTests::connectTunnelIsCompletedAndClosedSoHttpsHostsFailFast()
{
    ProbeProxyServer proxy(QByteArrayLiteral(R"({"status":"fail"})"));
    QVERIFY(proxy.listen());

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(proxy.port()));
    QVERIFY(client.waitForConnected(2000));

    client.write("CONNECT example.com:443 HTTP/1.1\r\nHost: example.com:443\r\n\r\n");

    // The proxy lives on this thread, so its readyRead is delivered by the event loop -- a blocking
    // wait on the client socket alone would never let the proxy answer, and the client would sit
    // there until the timeout. These spin the event loop instead.
    QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 2000);
    const QByteArray reply = client.readAll();
    QVERIFY2(reply.startsWith("HTTP/1.1 200"), reply.constData());

    // Closed rather than left open: an open tunnel is exactly what made the https hosts sit there
    // until the deadline on CI.
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 2000);
}

QTEST_MAIN(OutboundLocationProbeServiceTests)

#include "OutboundLocationProbeServiceTests.moc"
