#include <QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>

#include <utility>

#include "appcore/OutboundLocationProbeService.h"

// Stands in for the core's local http inbound. A plain http:// request gets a body the parser
// cannot turn into a location, so that host reports a reason of its own within milliseconds; a
// CONNECT tunnel is accepted and then deliberately left unanswered, so on a build with a working
// TLS backend the probe is still waiting on those hosts when its deadline arrives. Either way the
// state under test is the same and is the one that matters: hosts have already said why they
// failed, and the deadline is about to fire.
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
                    socket->deleteLater();
                });
            }
        });
    }

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

        if (request.startsWith("CONNECT ") || !answersPlainRequests_) {
            return; // accepted and deliberately left unanswered
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
// It cannot be isolated to the deadline message alone on every build -- the https hosts abort at
// TLS initialization on a Qt without a TLS backend -- so what is pinned here is that the wait
// ends at the budget instead of running on.
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
    // The plain http hosts accepted the connection and never answered, so the deadline is the only
    // thing that can end the wait -- and it has to say so, otherwise a hung host reads as silence.
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

QTEST_MAIN(OutboundLocationProbeServiceTests)

#include "OutboundLocationProbeServiceTests.moc"
