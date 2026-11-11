#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

#include "services/ProxyGroupStatusPoller.h"

namespace {

// A one-endpoint fake of the core's Clash API: answers GET /proxies/proxy with a scripted
// group object. Server-side sockets are cleaned up on disconnect -- the discipline the memory
// investigation pinned (a harness that hoards sockets turns any poll test into a false
// positive).
class FakeClashApi final : public QObject
{
    Q_OBJECT

public:
    explicit FakeClashApi(QObject* parent = nullptr)
        : QObject(parent)
    {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* connection = server_.nextPendingConnection()) {
                connect(connection, &QTcpSocket::disconnected, connection, &QTcpSocket::deleteLater);
                connect(connection, &QTcpSocket::readyRead, this, [this, connection]() {
                    if (!connection->canReadLine()) {
                        return;
                    }
                    while (connection->canReadLine()) {
                        connection->readLine();
                    }
                    const QByteArray body = QJsonDocument(QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("URLTest")},
                        {QStringLiteral("now"), scriptedNow_},
                    }).toJson();
                    const QByteArray response = QByteArrayLiteral("HTTP/1.1 200 OK\r\n")
                        + QByteArrayLiteral("Content-Type: application/json\r\n")
                        + QByteArray("Content-Length: " + QByteArray::number(body.size()) + "\r\n")
                        + QByteArrayLiteral("Connection: close\r\n\r\n")
                        + body;
                    connection->write(response);
                    connection->disconnectFromHost();
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

    void setScriptedNow(const QString& now)
    {
        scriptedNow_ = now;
    }

private:
    QTcpServer server_;
    QString scriptedNow_ = QStringLiteral("member-1");
};

} // namespace

class ProxyGroupStatusPollerTests : public QObject
{
    Q_OBJECT

private slots:
    void mapsTheGroupsNowTagBackToTheMemberId();
    void ignoresAnswersOutsideTheMemberList();
    void stopClearsTheCurrentMember();

private:
    QTemporaryDir keepAlive_;
};

void ProxyGroupStatusPollerTests::mapsTheGroupsNowTagBackToTheMemberId()
{
    FakeClashApi api;
    QVERIFY(api.listen());

    ProxyGroupStatusPoller poller;
    QSignalSpy changedSpy(&poller, SIGNAL(currentMemberChanged(QString)));

    poller.start(api.port(), QStringLiteral("s3cret"), {QStringLiteral("id-a"), QStringLiteral("id-b")});
    QVERIFY(changedSpy.wait(10000));
    QCOMPARE(poller.currentMemberId(), QStringLiteral("id-a"));
    QCOMPARE(changedSpy.at(0).at(0).toString(), QStringLiteral("id-a"));

    // The core switches to member-2: one poll later the id follows, without a restart. The
    // generous bound covers the poll interval plus the in-flight request.
    api.setScriptedNow(QStringLiteral("member-2"));
    QVERIFY(changedSpy.wait(15000));
    QCOMPARE(poller.currentMemberId(), QStringLiteral("id-b"));
}

void ProxyGroupStatusPollerTests::ignoresAnswersOutsideTheMemberList()
{
    FakeClashApi api;
    QVERIFY(api.listen());
    api.setScriptedNow(QStringLiteral("not-a-member"));

    ProxyGroupStatusPoller poller;
    QSignalSpy changedSpy(&poller, SIGNAL(currentMemberChanged(QString)));

    poller.start(api.port(), QStringLiteral("s3cret"), {QStringLiteral("id-a")});
    // The first in-flight poll answers the unknown tag; the current member must stay empty and
    // no signal may fire for it. The wait proves an event cycle ran without a delivery.
    QTest::qWait(200);
    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(poller.currentMemberId().isEmpty());
    poller.stop();
}

void ProxyGroupStatusPollerTests::stopClearsTheCurrentMember()
{
    FakeClashApi api;
    QVERIFY(api.listen());

    ProxyGroupStatusPoller poller;
    QSignalSpy changedSpy(&poller, SIGNAL(currentMemberChanged(QString)));

    poller.start(api.port(), QStringLiteral("s3cret"), {QStringLiteral("id-a")});
    QVERIFY(changedSpy.wait(10000));
    QCOMPARE(poller.currentMemberId(), QStringLiteral("id-a"));

    poller.stop();
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(changedSpy.at(1).at(0).toString(), QString());
    QVERIFY(poller.currentMemberId().isEmpty());
    QVERIFY(!poller.isActive());

    // Starting with no members is a stop, not a start.
    poller.start(api.port(), QStringLiteral("s3cret"), {});
    QVERIFY(!poller.isActive());
}

QTEST_MAIN(ProxyGroupStatusPollerTests)
#include "ProxyGroupStatusPollerTests.moc"
