#include <QtTest/QtTest>

#include "persistence/IConfigRepository.h"
#include "services/SubscriptionService.h"
#include "subscription/SubscriptionUrlImportService.h"

class MemoryConfigRepository final : public IConfigRepository {
public:
    Config load() override { return config; }

    bool save(const Config& value) override
    {
        config = value;
        return true;
    }

    Config config;
};

class SubscriptionUrlImportServiceTests : public QObject {
    Q_OBJECT

private slots:
    void prepareImportAddsUniqueSubscriptionUrls();
    void prepareImportReusesExistingSubscriptionsCaseInsensitively();
};

void SubscriptionUrlImportServiceTests::prepareImportAddsUniqueSubscriptionUrls()
{
    MemoryConfigRepository repository;
    SubscriptionService subscriptionService(repository);
    SubscriptionUrlImportService importService(
        repository,
        subscriptionService,
        [](Config&, const QStringList&) {
            return OperationResult::ok();
        });

    Config config;
    const SubscriptionUrlImportService::ImportPlan plan = importService.prepareImport(
        config,
        QStringLiteral("https://example.com/sub\nHTTPS://EXAMPLE.com/sub\nnot a url\nhttps://other.test/path"));

    QCOMPARE(plan.subscriptionIds.size(), 2);
    QCOMPARE(plan.subscriptions.size(), 2);
    QCOMPARE(plan.subscriptions.at(0).url, QStringLiteral("https://example.com/sub"));
    QCOMPARE(plan.subscriptions.at(0).remarks, QStringLiteral("example.com"));
    QCOMPARE(plan.subscriptions.at(1).url, QStringLiteral("https://other.test/path"));
    QCOMPARE(plan.subscriptions.at(1).remarks, QStringLiteral("other.test"));
    QCOMPARE(plan.lastSubscriptionId, plan.subscriptionIds.constLast());
}

void SubscriptionUrlImportServiceTests::prepareImportReusesExistingSubscriptionsCaseInsensitively()
{
    MemoryConfigRepository repository;
    SubscriptionService subscriptionService(repository);
    SubscriptionUrlImportService importService(
        repository,
        subscriptionService,
        [](Config&, const QStringList&) {
            return OperationResult::ok();
        });

    Config config;
    SubItem existing;
    existing.url = QStringLiteral("https://example.com/sub");
    existing.enabled = false;
    config.collection().subscriptions.append(existing);

    const SubscriptionUrlImportService::ImportPlan plan = importService.prepareImport(
        config,
        QStringLiteral("HTTPS://EXAMPLE.COM/sub"));

    QCOMPARE(plan.subscriptionIds.size(), 1);
    QCOMPARE(plan.subscriptions.size(), 1);
    QVERIFY(!plan.subscriptions.at(0).id.trimmed().isEmpty());
    QCOMPARE(plan.subscriptionIds.constFirst(), plan.subscriptions.at(0).id);
    QCOMPARE(plan.subscriptions.at(0).remarks, QStringLiteral("example.com"));
    QVERIFY(plan.subscriptions.at(0).enabled);
}

QTEST_MAIN(SubscriptionUrlImportServiceTests)
#include "SubscriptionUrlImportServiceTests.moc"
