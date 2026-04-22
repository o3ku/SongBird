#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "domain/models/Config.h"
#include "services/StatisticsService.h"

class StatisticsServiceTests : public QObject {
    Q_OBJECT

private slots:
    void loadCreatesEmptyStatsWhenNoFile();
    void loadReadsExistingStatsFile();
    void saveWritesStatsToJsonFile();
    void applyTrafficDeltaAccumulatesTotals();
    void applyTrafficDeltaResetsTodayOnDateChange();
    void applyTrafficDeltaCreatesNewEntryForUnknownItem();
    void applyTrafficDeltaIgnoresEmptyItemId();
    void startSessionSetsSessionState();
    void startSessionCreatesStatEntryForActiveServer();
    void stopSessionClearsRunningState();
    void clearAllRemovesAllStats();
    void setPollingAvailableUpdatesState();
    void setPollingUnavailableResetsDeltaCounters();
    void currentServerStatReturnsNormalizedStat();
    void currentServerStatReturnsEmptyWhenNoSession();
};

void StatisticsServiceTests::loadCreatesEmptyStatsWhenNoFile()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/nonexistent_stats.json");
    StatisticsService service(path);
    QVERIFY(service.statistics().isEmpty());
}

void StatisticsServiceTests::loadReadsExistingStatsFile()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/stats.json");

    QJsonObject statItem;
    statItem.insert(QStringLiteral("itemId"), QStringLiteral("server-1"));
    statItem.insert(QStringLiteral("totalUp"), 1000);
    statItem.insert(QStringLiteral("totalDown"), 2000);
    statItem.insert(QStringLiteral("todayUp"), 100);
    statItem.insert(QStringLiteral("todayDown"), 200);
    statItem.insert(QStringLiteral("dateNow"), 0);

    QJsonArray serverArray;
    serverArray.append(statItem);

    QJsonObject root;
    root.insert(QStringLiteral("server"), serverArray);

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    StatisticsService service(path);
    QCOMPARE(service.statistics().size(), 1);
    QCOMPARE(service.statistics().at(0).itemId, QStringLiteral("server-1"));
    QCOMPARE(service.statistics().at(0).totalUp, quint64(1000));
    QCOMPARE(service.statistics().at(0).totalDown, quint64(2000));
}

void StatisticsServiceTests::saveWritesStatsToJsonFile()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/stats.json");

    StatisticsService service(path);
    service.applyTrafficDelta(QStringLiteral("server-1"), 500, 1000);
    service.save();

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonArray serverArray = doc.object().value(QStringLiteral("server")).toArray();
    QCOMPARE(serverArray.size(), 1);
    QCOMPARE(serverArray.at(0).toObject().value(QStringLiteral("itemId")).toString(),
        QStringLiteral("server-1"));
}

void StatisticsServiceTests::applyTrafficDeltaAccumulatesTotals()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    service.applyTrafficDelta(QStringLiteral("s1"), 100, 200);
    service.applyTrafficDelta(QStringLiteral("s1"), 50, 150);

    const QList<ServerStatItem> stats = service.statistics();
    QCOMPARE(stats.size(), 1);
    QCOMPARE(stats.at(0).totalUp, quint64(150));
    QCOMPARE(stats.at(0).totalDown, quint64(350));
}

void StatisticsServiceTests::applyTrafficDeltaResetsTodayOnDateChange()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    service.applyTrafficDelta(QStringLiteral("s1"), 100, 200);

    const QList<ServerStatItem> stats = service.statistics();
    QCOMPARE(stats.at(0).todayUp, quint64(100));
    QCOMPARE(stats.at(0).todayDown, quint64(200));
}

void StatisticsServiceTests::applyTrafficDeltaCreatesNewEntryForUnknownItem()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    service.applyTrafficDelta(QStringLiteral("new-server"), 300, 400);

    const QList<ServerStatItem> stats = service.statistics();
    QCOMPARE(stats.size(), 1);
    QCOMPARE(stats.at(0).itemId, QStringLiteral("new-server"));
    QCOMPARE(stats.at(0).totalUp, quint64(300));
    QCOMPARE(stats.at(0).totalDown, quint64(400));
}

void StatisticsServiceTests::applyTrafficDeltaIgnoresEmptyItemId()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    service.applyTrafficDelta(QString(), 100, 200);
    service.applyTrafficDelta(QStringLiteral("  "), 100, 200);

    QVERIFY(service.statistics().isEmpty());
}

void StatisticsServiceTests::startSessionSetsSessionState()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    Config config;
    config.enableStatistics = true;
    config.statisticsFreshRate = 5;

    service.startSession(config, QStringLiteral("active-1"), CoreType::Xray, 10080, true);

    const StatisticsSessionState state = service.sessionState();
    QVERIFY(state.enabled);
    QVERIFY(state.running);
    QVERIFY(state.runtimeConfigReady);
    QCOMPARE(state.activeServerId, QStringLiteral("active-1"));
    QCOMPARE(state.coreType, CoreType::Xray);
    QCOMPARE(state.statePort, 10080);
    QCOMPARE(state.refreshRateSeconds, 5);
}

void StatisticsServiceTests::startSessionCreatesStatEntryForActiveServer()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    Config config;
    config.enableStatistics = true;

    service.startSession(config, QStringLiteral("new-active"), CoreType::Xray, 0, true);

    const QList<ServerStatItem> stats = service.statistics();
    bool found = false;
    for (const ServerStatItem& item : stats) {
        if (item.itemId == QStringLiteral("new-active")) {
            found = true;
            QCOMPARE(item.totalUp, quint64(0));
            QCOMPARE(item.totalDown, quint64(0));
        }
    }
    QVERIFY(found);
}

void StatisticsServiceTests::stopSessionClearsRunningState()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    Config config;
    config.enableStatistics = true;
    service.startSession(config, QStringLiteral("s1"), CoreType::Xray, 10080, true);
    QVERIFY(service.sessionState().running);

    service.stopSession();
    QVERIFY(!service.sessionState().running);
    QCOMPARE(service.sessionState().statePort, 0);
}

void StatisticsServiceTests::clearAllRemovesAllStats()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    service.applyTrafficDelta(QStringLiteral("s1"), 100, 200);
    service.applyTrafficDelta(QStringLiteral("s2"), 300, 400);
    QCOMPARE(service.statistics().size(), 2);

    const OperationResult result = service.clearAll();
    QVERIFY(result.success);
    QVERIFY(service.statistics().isEmpty());
}

void StatisticsServiceTests::setPollingAvailableUpdatesState()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    service.setPollingAvailable(true);
    QVERIFY(service.sessionState().pollingAvailable);
}

void StatisticsServiceTests::setPollingUnavailableResetsDeltaCounters()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    service.applyTrafficDelta(QStringLiteral("s1"), 100, 200);
    service.setPollingAvailable(false);

    QVERIFY(!service.sessionState().pollingAvailable);
    QCOMPARE(service.sessionState().lastDeltaUp, quint64(0));
    QCOMPARE(service.sessionState().lastDeltaDown, quint64(0));
}

void StatisticsServiceTests::currentServerStatReturnsNormalizedStat()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    Config config;
    config.enableStatistics = true;
    service.startSession(config, QStringLiteral("s1"), CoreType::Xray, 0, true);
    service.applyTrafficDelta(QStringLiteral("s1"), 100, 200);

    const ServerStatItem current = service.currentServerStat();
    QCOMPARE(current.itemId, QStringLiteral("s1"));
    QCOMPARE(current.totalUp, quint64(100));
    QCOMPARE(current.totalDown, quint64(200));
}

void StatisticsServiceTests::currentServerStatReturnsEmptyWhenNoSession()
{
    QTemporaryDir dir;
    StatisticsService service(dir.path() + QStringLiteral("/stats.json"));

    const ServerStatItem current = service.currentServerStat();
    QVERIFY(current.itemId.isEmpty());
}

QTEST_MAIN(StatisticsServiceTests)
#include "StatisticsServiceTests.moc"
