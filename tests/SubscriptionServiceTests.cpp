#include <QtTest>

#include "persistence/IConfigRepository.h"
#include "services/SubscriptionService.h"

namespace {
class MockConfigRepository : public IConfigRepository {
public:
    Config load() override { return config_; }
    bool save(const Config& config) override
    {
        config_ = config;
        saveCount++;
        return true;
    }

    Config config_;
    int saveCount = 0;
};

SubItem makeSub(const QString& id, const QString& remarks, const QString& url,
                bool enabled = true)
{
    SubItem sub;
    sub.id = id;
    sub.remarks = remarks;
    sub.url = url;
    sub.enabled = enabled;
    return sub;
}

VmessItem makeServer(const QString& indexId, const QString& subId,
                     const QString& uuid)
{
    VmessItem server;
    server.indexId = indexId;
    server.subId = subId;
    server.id = uuid;
    return server;
}

VmessItem makeVmessServer(const QString& indexId, const QString& subId,
                          const QString& address, int port, const QString& uuid)
{
    VmessItem server = makeServer(indexId, subId, uuid);
    server.configType = ConfigType::VMess;
    server.address = address;
    server.port = port;
    return server;
}
} // namespace

class SubscriptionServiceTests : public QObject {
    Q_OBJECT

private slots:
    void testAddSubscription();
    void testSaveSubscriptionsAutoAssignsEmptyIds();
    void testUpdateSubscriptionById();
    void testRemoveSubscriptionCascadesServers();
    void testRemoveSubscriptionClearsCurrentIndexId();
    void testRemoveSubscriptionClearsMainSelectedSubId();
    void testRemoveSubscriptionNotFound();
    void testRemoveSubscriptionEmptyId();
    void testEnableSubscription();
    void testDisableSubscriptionClearsMainSelectedSubId();
    void testEnableSubscriptionNotFound();
    void testEnableSubscriptionEmptyId();
    void testReplaceSubscriptionServersDeduplicatesByUuid();
    void testReplaceSubscriptionServersClearsOldServers();
    void testReplaceSubscriptionServersReusesIndexIdForMatchingServer();
    void testReplaceSubscriptionServersKeepsCurrentIndexIdWhenMatchingServerStillExists();
    void testReplaceSubscriptionServersFallsBackCurrentIndexIdToFirstUpdatedServerWhenActiveServerRemoved();
    void testReplaceSubscriptionServersFallsBackCurrentIndexIdToFirstGlobalServerWhenSubscriptionBecomesEmpty();
    void testReplaceSubscriptionServersEmptyId();
    void testListSubscriptions();
};

// -- Add subscription via saveSubscriptions --

void SubscriptionServiceTests::testAddSubscription()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("My Sub"),
        QStringLiteral("https://example.com/sub")));

    auto result = service.saveSubscriptions(config, config.subscriptions);

    QVERIFY(result.success);
    QCOMPARE(repo.saveCount, 1);
    QCOMPARE(repo.config_.subscriptions.size(), 1);
    QCOMPARE(repo.config_.subscriptions[0].id, QStringLiteral("sub-1"));
    QCOMPARE(repo.config_.subscriptions[0].remarks, QStringLiteral("My Sub"));
}

// -- saveSubscriptions auto-assigns UUID to items with empty id --

void SubscriptionServiceTests::testSaveSubscriptionsAutoAssignsEmptyIds()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    QList<SubItem> items;
    items.append(makeSub(QString(), QStringLiteral("NoId1"),
                         QStringLiteral("https://a.com")));
    items.append(makeSub(QStringLiteral("existing-id"),
                         QStringLiteral("HasId"),
                         QStringLiteral("https://b.com")));
    items.append(makeSub(QStringLiteral("  "), QStringLiteral("WhitespaceId"),
                          QStringLiteral("https://c.com")));

    auto result = service.saveSubscriptions(config, items);

    QVERIFY(result.success);
    QCOMPARE(repo.config_.subscriptions.size(), 3);

    // First item: empty id should have been replaced with a UUID
    QVERIFY(!repo.config_.subscriptions[0].id.isEmpty());
    QVERIFY(repo.config_.subscriptions[0].id != QString());

    // Second item: existing id preserved
    QCOMPARE(repo.config_.subscriptions[1].id, QStringLiteral("existing-id"));

    // Third item: whitespace-only id replaced with UUID
    QVERIFY(!repo.config_.subscriptions[2].id.trimmed().isEmpty());
}

// -- Update a subscription by id (via saveSubscriptions replaces the full list) --

void SubscriptionServiceTests::testUpdateSubscriptionById()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Original"),
        QStringLiteral("https://old.com")));
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-2"), QStringLiteral("Second"),
        QStringLiteral("https://second.com")));

    // Update sub-1 remarks
    config.subscriptions[0].remarks = QStringLiteral("Updated");
    config.subscriptions[0].url = QStringLiteral("https://new.com");

    auto result = service.saveSubscriptions(config, config.subscriptions);

    QVERIFY(result.success);
    QCOMPARE(repo.config_.subscriptions.size(), 2);
    QCOMPARE(repo.config_.subscriptions[0].remarks, QStringLiteral("Updated"));
    QCOMPARE(repo.config_.subscriptions[0].url, QStringLiteral("https://new.com"));
    QCOMPARE(repo.config_.subscriptions[1].remarks, QStringLiteral("Second"));
}

// -- Remove subscription cascades to linked servers --

void SubscriptionServiceTests::testRemoveSubscriptionCascadesServers()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-2"), QStringLiteral("Sub 2"),
        QStringLiteral("https://b.com")));

    // Servers belonging to sub-1
    config.servers.append(makeServer(QStringLiteral("idx-a"), QStringLiteral("sub-1"),
                                     QStringLiteral("uuid-1")));
    config.servers.append(makeServer(QStringLiteral("idx-b"), QStringLiteral("sub-1"),
                                     QStringLiteral("uuid-2")));
    // Server belonging to sub-2 -- should survive
    config.servers.append(makeServer(QStringLiteral("idx-c"), QStringLiteral("sub-2"),
                                     QStringLiteral("uuid-3")));

    auto result = service.removeSubscription(config, QStringLiteral("sub-1"));

    QVERIFY(result.success);
    QCOMPARE(config.subscriptions.size(), 1);
    QCOMPARE(config.subscriptions[0].id, QStringLiteral("sub-2"));
    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers[0].indexId, QStringLiteral("idx-c"));
    QCOMPARE(config.servers[0].subId, QStringLiteral("sub-2"));
}

// -- Remove subscription resets currentIndexId when it pointed to a removed server --

void SubscriptionServiceTests::testRemoveSubscriptionClearsCurrentIndexId()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-2"), QStringLiteral("Sub 2"),
        QStringLiteral("https://b.com")));

    config.servers.append(makeServer(QStringLiteral("idx-a"), QStringLiteral("sub-1"),
                                     QStringLiteral("uuid-1")));
    config.servers.append(makeServer(QStringLiteral("idx-b"), QStringLiteral("sub-2"),
                                     QStringLiteral("uuid-2")));

    // Current selection points to a sub-1 server
    config.currentIndexId = QStringLiteral("idx-a");

    service.removeSubscription(config, QStringLiteral("sub-1"));

    // currentIndexId should be reset to the first remaining server
    QCOMPARE(config.currentIndexId, QStringLiteral("idx-b"));
}

// -- Remove subscription clears mainSelectedSubId when it matched --

void SubscriptionServiceTests::testRemoveSubscriptionClearsMainSelectedSubId()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.mainSelectedSubId = QStringLiteral("sub-1");

    service.removeSubscription(config, QStringLiteral("sub-1"));

    QVERIFY(config.mainSelectedSubId.isEmpty());
}

// -- Remove subscription fails for non-existent id --

void SubscriptionServiceTests::testRemoveSubscriptionNotFound()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    auto result = service.removeSubscription(config, QStringLiteral("nonexistent"));

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("does not exist")));
}

// -- Remove subscription fails for empty id --

void SubscriptionServiceTests::testRemoveSubscriptionEmptyId()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    auto result = service.removeSubscription(config, QString());

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("required")));
}

// -- Enable a subscription --

void SubscriptionServiceTests::testEnableSubscription()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    SubItem sub = makeSub(QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
                          QStringLiteral("https://a.com"));
    sub.enabled = false;
    config.subscriptions.append(sub);

    auto result = service.setSubscriptionEnabled(
        config, QStringLiteral("sub-1"), true);

    QVERIFY(result.success);
    QVERIFY(config.subscriptions[0].enabled);
    QVERIFY(result.message.contains(QStringLiteral("enabled")));
}

// -- Disable subscription clears mainSelectedSubId --

void SubscriptionServiceTests::testDisableSubscriptionClearsMainSelectedSubId()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.mainSelectedSubId = QStringLiteral("sub-1");

    auto result = service.setSubscriptionEnabled(
        config, QStringLiteral("sub-1"), false);

    QVERIFY(result.success);
    QVERIFY(!config.subscriptions[0].enabled);
    QVERIFY(config.mainSelectedSubId.isEmpty());
    QVERIFY(result.message.contains(QStringLiteral("hidden")));
}

// -- Enable fails for non-existent subscription --

void SubscriptionServiceTests::testEnableSubscriptionNotFound()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    auto result = service.setSubscriptionEnabled(
        config, QStringLiteral("ghost"), true);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("does not exist")));
}

// -- Enable fails for empty id --

void SubscriptionServiceTests::testEnableSubscriptionEmptyId()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    auto result = service.setSubscriptionEnabled(config, QString(), true);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("required")));
}

// -- Replace subscription servers deduplicates by UUID (case-insensitive, trimmed) --

void SubscriptionServiceTests::testReplaceSubscriptionServersDeduplicatesByUuid()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));

    QList<VmessItem> newServers;
    newServers.append(makeServer(QString(), QString(), QStringLiteral("UUID-ABC")));
    newServers.append(makeServer(QString(), QString(), QStringLiteral("uuid-abc"))); // duplicate (case-insensitive)
    newServers.append(makeServer(QString(), QString(), QStringLiteral("uuid-def"))); // unique
    newServers.append(makeServer(QString(), QString(), QString()));                   // empty uuid kept

    auto result = service.replaceSubscriptionServers(
        config, QStringLiteral("sub-1"), newServers);

    QVERIFY(result.success);
    // 3 servers: uuid-abc (first), uuid-def, empty-uuid one
    int sub1Count = 0;
    for (const auto& s : config.servers) {
        if (s.subId == QStringLiteral("sub-1"))
            ++sub1Count;
    }
    QCOMPARE(sub1Count, 3);

    // Each server should have been assigned a unique indexId
    QSet<QString> indexIds;
    for (const auto& s : config.servers) {
        if (s.subId == QStringLiteral("sub-1")) {
            QVERIFY(!s.indexId.isEmpty());
            QVERIFY(!indexIds.contains(s.indexId));
            indexIds.insert(s.indexId);
        }
    }
    QCOMPARE(indexIds.size(), 3);
}

// -- Replace subscription servers removes old servers for that sub --

void SubscriptionServiceTests::testReplaceSubscriptionServersClearsOldServers()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-2"), QStringLiteral("Sub 2"),
        QStringLiteral("https://b.com")));

    // Pre-existing servers for sub-1
    config.servers.append(makeServer(QStringLiteral("old-1"), QStringLiteral("sub-1"),
                                     QStringLiteral("uuid-old")));
    // Server for sub-2 should be untouched
    config.servers.append(makeServer(QStringLiteral("keep-1"), QStringLiteral("sub-2"),
                                     QStringLiteral("uuid-other")));

    QList<VmessItem> replacement;
    replacement.append(makeServer(QString(), QString(), QStringLiteral("uuid-new")));

    service.replaceSubscriptionServers(
        config, QStringLiteral("sub-1"), replacement);

    QCOMPARE(config.servers.size(), 2);
    // One belongs to sub-2
    bool foundSub2 = false;
    bool foundNewSub1 = false;
    for (const auto& s : config.servers) {
        if (s.subId == QStringLiteral("sub-2")) {
            foundSub2 = true;
            QCOMPARE(s.indexId, QStringLiteral("keep-1"));
        }
        if (s.subId == QStringLiteral("sub-1")) {
            foundNewSub1 = true;
            QCOMPARE(s.id, QStringLiteral("uuid-new"));
            QVERIFY(s.indexId != QStringLiteral("old-1"));
        }
    }
    QVERIFY(foundSub2);
    QVERIFY(foundNewSub1);
}

void SubscriptionServiceTests::testReplaceSubscriptionServersReusesIndexIdForMatchingServer()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.servers.append(makeVmessServer(
        QStringLiteral("old-1"), QStringLiteral("sub-1"),
        QStringLiteral("example.com"), 443, QStringLiteral("uuid-1")));

    QList<VmessItem> replacement;
    replacement.append(makeVmessServer(
        QString(), QString(),
        QStringLiteral("example.com"), 443, QStringLiteral("uuid-1")));

    auto result = service.replaceSubscriptionServers(
        config, QStringLiteral("sub-1"), replacement);

    QVERIFY(result.success);
    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers[0].indexId, QStringLiteral("old-1"));
}

void SubscriptionServiceTests::testReplaceSubscriptionServersKeepsCurrentIndexIdWhenMatchingServerStillExists()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.servers.append(makeVmessServer(
        QStringLiteral("old-1"), QStringLiteral("sub-1"),
        QStringLiteral("example.com"), 443, QStringLiteral("uuid-1")));
    config.currentIndexId = QStringLiteral("old-1");

    QList<VmessItem> replacement;
    replacement.append(makeVmessServer(
        QString(), QString(),
        QStringLiteral("example.com"), 443, QStringLiteral("uuid-1")));

    auto result = service.replaceSubscriptionServers(
        config, QStringLiteral("sub-1"), replacement);

    QVERIFY(result.success);
    QCOMPARE(config.currentIndexId, QStringLiteral("old-1"));
}

void SubscriptionServiceTests::testReplaceSubscriptionServersFallsBackCurrentIndexIdToFirstUpdatedServerWhenActiveServerRemoved()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.servers.append(makeVmessServer(
        QStringLiteral("old-1"), QStringLiteral("sub-1"),
        QStringLiteral("old.example.com"), 443, QStringLiteral("uuid-1")));
    config.currentIndexId = QStringLiteral("old-1");

    QList<VmessItem> replacement;
    replacement.append(makeVmessServer(
        QString(), QString(),
        QStringLiteral("new-a.example.com"), 443, QStringLiteral("uuid-a")));
    replacement.append(makeVmessServer(
        QString(), QString(),
        QStringLiteral("new-b.example.com"), 8443, QStringLiteral("uuid-b")));

    auto result = service.replaceSubscriptionServers(
        config, QStringLiteral("sub-1"), replacement);

    QVERIFY(result.success);
    QCOMPARE(config.currentIndexId, config.servers[0].indexId);
    QCOMPARE(config.servers[0].subId, QStringLiteral("sub-1"));
    QCOMPARE(config.servers[0].address, QStringLiteral("new-a.example.com"));
}

void SubscriptionServiceTests::testReplaceSubscriptionServersFallsBackCurrentIndexIdToFirstGlobalServerWhenSubscriptionBecomesEmpty()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("Sub 1"),
        QStringLiteral("https://a.com")));
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-2"), QStringLiteral("Sub 2"),
        QStringLiteral("https://b.com")));
    config.servers.append(makeVmessServer(
        QStringLiteral("old-1"), QStringLiteral("sub-1"),
        QStringLiteral("old.example.com"), 443, QStringLiteral("uuid-1")));
    config.servers.append(makeVmessServer(
        QStringLiteral("keep-1"), QStringLiteral("sub-2"),
        QStringLiteral("keep.example.com"), 443, QStringLiteral("uuid-2")));
    config.currentIndexId = QStringLiteral("old-1");

    auto result = service.replaceSubscriptionServers(
        config, QStringLiteral("sub-1"), QList<VmessItem>());

    QVERIFY(result.success);
    QCOMPARE(config.currentIndexId, QStringLiteral("keep-1"));
    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers[0].subId, QStringLiteral("sub-2"));
}

// -- Replace subscription servers fails for empty subscription id --

void SubscriptionServiceTests::testReplaceSubscriptionServersEmptyId()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    auto result = service.replaceSubscriptionServers(
        config, QString(), QList<VmessItem>());

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("required")));
}

// -- List subscriptions returns the config's subscription list --

void SubscriptionServiceTests::testListSubscriptions()
{
    MockConfigRepository repo;
    SubscriptionService service(repo);

    Config config;
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-1"), QStringLiteral("First"),
        QStringLiteral("https://a.com")));
    config.subscriptions.append(makeSub(
        QStringLiteral("sub-2"), QStringLiteral("Second"),
        QStringLiteral("https://b.com")));

    QList<SubItem> listed = service.list(config);

    QCOMPARE(listed.size(), 2);
    QCOMPARE(listed[0].id, QStringLiteral("sub-1"));
    QCOMPARE(listed[1].id, QStringLiteral("sub-2"));
}

QTEST_MAIN(SubscriptionServiceTests)
#include "SubscriptionServiceTests.moc"
