#include <QtTest>

#include "domain/models/Config.h"
#include "persistence/IConfigRepository.h"
#include "services/RoutingService.h"

namespace {

class MockConfigRepository : public IConfigRepository {
public:
    Config load() override { return config_; }
    bool save(const Config& config) override
    {
        config_ = config;
        return saveSucceeds_;
    }

    Config config_;
    bool saveSucceeds_ = true;
};

RoutingItem makeItem(const QString& remarks, const QList<RoutingRule>& rules = {})
{
    RoutingItem item;
    item.remarks = remarks;
    item.enabled = true;
    item.locked = false;
    item.rules = rules;
    return item;
}

RoutingRule makeRule(const QString& outboundTag,
    const QStringList& domain = {},
    const QStringList& ip = {},
    const QStringList& process = {})
{
    RoutingRule rule;
    rule.outboundTag = outboundTag;
    rule.domain = domain;
    rule.ip = ip;
    rule.process = process;
    rule.enabled = true;
    return rule;
}

} // namespace

class RoutingServiceTests : public QObject {
    Q_OBJECT

private slots:
    void saveRoutingTrimsRemarksAndRemovesEmptyRules();
    void saveRoutingDeduplicatesValues();
    void saveRoutingRemovesUnmeaningfulRules();
    void saveRoutingLocksSelectedIndex();
    void saveRoutingClampsInvalidSelectedIndex();
    void saveRoutingReturnsFailWhenRepositorySaveFails();
    void setRoutingModeBasicUnlocksAllItems();
    void setRoutingModeAdvancedLocksSelectedIndex();
    void setRoutingModeAdvancedClampsNegativeIndex();
    void selectRoutingLocksOnlySelectedIndex();
    void selectRoutingFailsWithNoItems();
    void normalizeValuesRemovesEmptyAndDuplicates();
    void isMeaningfulRuleRejectsEmptyRule();
    void isMeaningfulRuleAcceptsRuleWithOnlyOutboundTag();
};

void RoutingServiceTests::saveRoutingTrimsRemarksAndRemovesEmptyRules()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("  proxy  ")));
    items[0].rules.append(makeRule(QStringLiteral("proxy"), {QStringLiteral("  google.com  ")}));
    items[0].rules.append(RoutingRule{});

    const OperationResult result = service.saveRouting(config, items, true, 0, {}, {});
    QVERIFY(result.success);
    QCOMPARE(mock.config_.routingItems[0].remarks, QStringLiteral("proxy"));
    QCOMPARE(mock.config_.routingItems[0].rules.size(), 1);
    QCOMPARE(mock.config_.routingItems[0].rules[0].domain, QStringList{QStringLiteral("google.com")});
}

void RoutingServiceTests::saveRoutingDeduplicatesValues()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("test")));
    items[0].rules.append(makeRule(QStringLiteral("direct"),
        {QStringLiteral("a.com"), QStringLiteral("a.com"), QStringLiteral("b.com")}));

    const OperationResult result = service.saveRouting(config, items, true, 0, {}, {});
    QVERIFY(result.success);
    const QStringList expected = {QStringLiteral("a.com"), QStringLiteral("b.com")};
    QCOMPARE(mock.config_.routingItems[0].rules[0].domain, expected);
}

void RoutingServiceTests::saveRoutingRemovesUnmeaningfulRules()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("test")));
    RoutingRule emptyRule;
    emptyRule.type = QStringLiteral("  ");
    items[0].rules.append(emptyRule);

    const OperationResult result = service.saveRouting(config, items, true, 0, {}, {});
    QVERIFY(result.success);
    QVERIFY(mock.config_.routingItems[0].rules.isEmpty());
}

void RoutingServiceTests::saveRoutingLocksSelectedIndex()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("A")));
    items.append(makeItem(QStringLiteral("B")));
    items.append(makeItem(QStringLiteral("C")));

    const OperationResult result = service.saveRouting(config, items, true, 1, {}, {});
    QVERIFY(result.success);
    QCOMPARE(mock.config_.routingItems[0].locked, false);
    QCOMPARE(mock.config_.routingItems[1].locked, true);
    QCOMPARE(mock.config_.routingItems[2].locked, false);
    QCOMPARE(mock.config_.routingIndex, 1);
}

void RoutingServiceTests::saveRoutingClampsInvalidSelectedIndex()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("A")));
    items.append(makeItem(QStringLiteral("B")));

    const OperationResult result = service.saveRouting(config, items, true, 99, {}, {});
    QVERIFY(result.success);
    QCOMPARE(mock.config_.routingItems[0].locked, true);
    QCOMPARE(mock.config_.routingItems[1].locked, false);
    QCOMPARE(mock.config_.routingIndex, 0);
}

void RoutingServiceTests::saveRoutingReturnsFailWhenRepositorySaveFails()
{
    MockConfigRepository mock;
    mock.saveSucceeds_ = false;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("A")));

    const OperationResult result = service.saveRouting(config, items, true, 0, {}, {});
    QVERIFY(!result.success);
}

void RoutingServiceTests::setRoutingModeBasicUnlocksAllItems()
{
    MockConfigRepository mock;
    mock.config_.routingItems = {makeItem(QStringLiteral("A")), makeItem(QStringLiteral("B"))};
    mock.config_.routingItems[0].locked = true;
    RoutingService service(mock);

    Config config = mock.config_;
    const OperationResult result = service.setRoutingMode(config, false, 0);
    QVERIFY(result.success);
    QCOMPARE(config.enableRoutingAdvanced, false);
    QCOMPARE(config.routingIndex, 0);
    QVERIFY(!config.routingItems[0].locked);
    QVERIFY(!config.routingItems[1].locked);
}

void RoutingServiceTests::setRoutingModeAdvancedLocksSelectedIndex()
{
    MockConfigRepository mock;
    mock.config_.routingItems = {makeItem(QStringLiteral("A")), makeItem(QStringLiteral("B"))};
    RoutingService service(mock);

    Config config = mock.config_;
    const OperationResult result = service.setRoutingMode(config, true, 1);
    QVERIFY(result.success);
    QCOMPARE(config.enableRoutingAdvanced, true);
    QCOMPARE(config.routingIndex, 1);
    QVERIFY(!config.routingItems[0].locked);
    QVERIFY(config.routingItems[1].locked);
}

void RoutingServiceTests::setRoutingModeAdvancedClampsNegativeIndex()
{
    MockConfigRepository mock;
    mock.config_.routingItems = {makeItem(QStringLiteral("A"))};
    RoutingService service(mock);

    Config config = mock.config_;
    const OperationResult result = service.setRoutingMode(config, true, -5);
    QVERIFY(result.success);
    QCOMPARE(config.routingIndex, 0);
    QVERIFY(config.routingItems[0].locked);
}

void RoutingServiceTests::selectRoutingLocksOnlySelectedIndex()
{
    MockConfigRepository mock;
    mock.config_.routingItems = {makeItem(QStringLiteral("A")), makeItem(QStringLiteral("B")), makeItem(QStringLiteral("C"))};
    mock.config_.routingItems[0].locked = true;
    RoutingService service(mock);

    Config config = mock.config_;
    const OperationResult result = service.selectRouting(config, 2);
    QVERIFY(result.success);
    QVERIFY(!config.routingItems[0].locked);
    QVERIFY(!config.routingItems[1].locked);
    QVERIFY(config.routingItems[2].locked);
    QCOMPARE(config.routingIndex, 2);
}

void RoutingServiceTests::selectRoutingFailsWithNoItems()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    const OperationResult result = service.selectRouting(config, 0);
    QVERIFY(!result.success);
}

void RoutingServiceTests::normalizeValuesRemovesEmptyAndDuplicates()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("test")));
    items[0].rules.append(makeRule(QStringLiteral("tag"),
        {QStringLiteral(""), QStringLiteral("  "), QStringLiteral("a.com"), QStringLiteral("a.com")}));

    service.saveRouting(config, items, true, 0, {}, {});
    QCOMPARE(mock.config_.routingItems[0].rules[0].domain, QStringList{QStringLiteral("a.com")});
}

void RoutingServiceTests::isMeaningfulRuleRejectsEmptyRule()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("test")));
    RoutingRule rule;
    rule.enabled = true;
    items[0].rules.append(rule);

    service.saveRouting(config, items, true, 0, {}, {});
    QVERIFY(mock.config_.routingItems[0].rules.isEmpty());
}

void RoutingServiceTests::isMeaningfulRuleAcceptsRuleWithOnlyOutboundTag()
{
    MockConfigRepository mock;
    RoutingService service(mock);

    Config config;
    QList<RoutingItem> items;
    items.append(makeItem(QStringLiteral("test")));
    RoutingRule rule;
    rule.outboundTag = QStringLiteral("direct");
    rule.enabled = true;
    items[0].rules.append(rule);

    service.saveRouting(config, items, true, 0, {}, {});
    QCOMPARE(mock.config_.routingItems[0].rules.size(), 1);
}

QTEST_MAIN(RoutingServiceTests)
#include "RoutingServiceTests.moc"
