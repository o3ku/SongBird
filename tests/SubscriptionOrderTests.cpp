#include <QtTest>

#include "common/SubscriptionOrder.h"
#include "persistence/IConfigRepository.h"
#include "services/SubscriptionService.h"

namespace {

class MockConfigRepository : public IConfigRepository {
public:
    Config load() override { return config_; }
    bool save(const Config& config) override
    {
        if (!saveSucceeds_) {
            return false;
        }
        config_ = config;
        ++saveCount;
        return true;
    }
    QString lastLoadError() const override { return {}; }
    QString lastSaveError() const override { return {}; }

    Config config_;
    bool saveSucceeds_ = true;
    int saveCount = 0;
};

SubItem makeSub(const QString& id, bool enabled = true)
{
    SubItem sub;
    sub.id = id;
    sub.remarks = id;
    sub.url = QStringLiteral("https://example.com/%1").arg(id);
    sub.enabled = enabled;
    return sub;
}

QStringList ids(const QList<SubItem>& items)
{
    QStringList result;
    for (const SubItem& item : items) {
        result.append(item.id);
    }
    return result;
}

QList<SubItem> makeList(const QStringList& subscriptionIds)
{
    QList<SubItem> items;
    for (const QString& id : subscriptionIds) {
        items.append(makeSub(id));
    }
    return items;
}

} // namespace

class SubscriptionOrderTests : public QObject {
    Q_OBJECT

private slots:
    // Pure reorder rules.
    void moveUpMovesSingleSelectionOneStep();
    void moveUpLeavesFirstSelectionAlone();
    void moveDownMovesSingleSelectionOneStep();
    void moveDownLeavesLastSelectionAlone();
    void moveTopAndBottomKeepRelativeOrderOfSelection();
    void moveEverySelectionIsNotAReorder();
    void moveWithUnknownIdChangesNothing();

    // Service: persistence and no-op handling.
    void serviceMovePersistsOrder();
    void serviceMoveAtEdgeDoesNotSave();
    void serviceMoveReportsSaveFailure();
    void serviceReorderAppliesWholeOrder();
    void serviceReorderKeepsUnmentionedSubscriptions();
    void serviceReorderIgnoresUnknownIds();
    void serviceReorderWithSameOrderDoesNotSave();
};

// -- Pure reorder rules --

void SubscriptionOrderTests::moveUpMovesSingleSelectionOneStep()
{
    QList<SubItem> items = makeList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
    QVERIFY(SubscriptionOrder::moveSubscriptions(items, {QStringLiteral("b")}, SubscriptionOrder::MoveOperation::Up));
    QCOMPARE(ids(items), (QStringList{QStringLiteral("b"), QStringLiteral("a"), QStringLiteral("c")}));
}

void SubscriptionOrderTests::moveUpLeavesFirstSelectionAlone()
{
    QList<SubItem> items = makeList({QStringLiteral("a"), QStringLiteral("b")});
    QVERIFY(!SubscriptionOrder::moveSubscriptions(items, {QStringLiteral("a")}, SubscriptionOrder::MoveOperation::Up));
    QCOMPARE(ids(items), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
}

void SubscriptionOrderTests::moveDownMovesSingleSelectionOneStep()
{
    QList<SubItem> items = makeList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
    QVERIFY(SubscriptionOrder::moveSubscriptions(items, {QStringLiteral("b")}, SubscriptionOrder::MoveOperation::Down));
    QCOMPARE(ids(items), (QStringList{QStringLiteral("a"), QStringLiteral("c"), QStringLiteral("b")}));
}

void SubscriptionOrderTests::moveDownLeavesLastSelectionAlone()
{
    QList<SubItem> items = makeList({QStringLiteral("a"), QStringLiteral("b")});
    QVERIFY(!SubscriptionOrder::moveSubscriptions(items, {QStringLiteral("b")}, SubscriptionOrder::MoveOperation::Down));
    QCOMPARE(ids(items), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
}

void SubscriptionOrderTests::moveTopAndBottomKeepRelativeOrderOfSelection()
{
    // A scattered selection must not be reversed or interleaved with itself: "move to top" means
    // the two chosen subscriptions go up there, in the order they already had.
    QList<SubItem> items = makeList({
        QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
        QStringLiteral("d"), QStringLiteral("e")});
    const QStringList selection{QStringLiteral("d"), QStringLiteral("b")};

    QList<SubItem> toTop = items;
    QVERIFY(SubscriptionOrder::moveSubscriptions(toTop, selection, SubscriptionOrder::MoveOperation::Top));
    QCOMPARE(ids(toTop), (QStringList{
        QStringLiteral("b"), QStringLiteral("d"),
        QStringLiteral("a"), QStringLiteral("c"), QStringLiteral("e")}));

    QList<SubItem> toBottom = items;
    QVERIFY(SubscriptionOrder::moveSubscriptions(toBottom, selection, SubscriptionOrder::MoveOperation::Bottom));
    QCOMPARE(ids(toBottom), (QStringList{
        QStringLiteral("a"), QStringLiteral("c"), QStringLiteral("e"),
        QStringLiteral("b"), QStringLiteral("d")}));
}

void SubscriptionOrderTests::moveEverySelectionIsNotAReorder()
{
    QList<SubItem> items = makeList({QStringLiteral("a"), QStringLiteral("b")});
    const QStringList everything{QStringLiteral("a"), QStringLiteral("b")};
    QVERIFY(!SubscriptionOrder::moveSubscriptions(items, everything, SubscriptionOrder::MoveOperation::Top));
    QVERIFY(!SubscriptionOrder::moveSubscriptions(items, everything, SubscriptionOrder::MoveOperation::Bottom));
    QCOMPARE(ids(items), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
}

void SubscriptionOrderTests::moveWithUnknownIdChangesNothing()
{
    QList<SubItem> items = makeList({QStringLiteral("a"), QStringLiteral("b")});
    QVERIFY(!SubscriptionOrder::moveSubscriptions(items, {QStringLiteral("missing")}, SubscriptionOrder::MoveOperation::Up));
    QVERIFY(!SubscriptionOrder::moveSubscriptions(items, {}, SubscriptionOrder::MoveOperation::Bottom));
    QCOMPARE(ids(items), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
}

// -- Service --

void SubscriptionOrderTests::serviceMovePersistsOrder()
{
    MockConfigRepository repository;
    SubscriptionService service(repository);
    Config config;
    config.collection().subscriptions = makeList({
        QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});

    const OperationResult result = service.moveSubscriptions(
        config, {QStringLiteral("c")}, SubscriptionOrder::MoveOperation::Top);
    QVERIFY(result.success);
    QCOMPARE(ids(config.collection().subscriptions),
             (QStringList{QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(ids(repository.config_.collection().subscriptions),
             (QStringList{QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(repository.saveCount, 1);
}

void SubscriptionOrderTests::serviceMoveAtEdgeDoesNotSave()
{
    MockConfigRepository repository;
    SubscriptionService service(repository);
    Config config;
    config.collection().subscriptions = makeList({QStringLiteral("a"), QStringLiteral("b")});

    const OperationResult result = service.moveSubscriptions(
        config, {QStringLiteral("a")}, SubscriptionOrder::MoveOperation::Up);
    QVERIFY(result.success);
    // Reporting failure for "already at the top" would put an error in the log for a no-op.
    QCOMPARE(repository.saveCount, 0);
}

void SubscriptionOrderTests::serviceMoveReportsSaveFailure()
{
    MockConfigRepository repository;
    repository.saveSucceeds_ = false;
    SubscriptionService service(repository);
    Config config;
    config.collection().subscriptions = makeList({QStringLiteral("a"), QStringLiteral("b")});

    const OperationResult result = service.moveSubscriptions(
        config, {QStringLiteral("b")}, SubscriptionOrder::MoveOperation::Up);
    QVERIFY(!result.success);
}

void SubscriptionOrderTests::serviceReorderAppliesWholeOrder()
{
    MockConfigRepository repository;
    SubscriptionService service(repository);
    Config config;
    config.collection().subscriptions = makeList({
        QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});

    const OperationResult result = service.reorderSubscriptions(
        config, {QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")});
    QVERIFY(result.success);
    QCOMPARE(ids(config.collection().subscriptions),
             (QStringList{QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(repository.saveCount, 1);
}

void SubscriptionOrderTests::serviceReorderKeepsUnmentionedSubscriptions()
{
    // A hidden subscription has no tab, so it cannot be dragged. Dropping it from the list because
    // the order never mentioned it would delete the user's subscription on the next save.
    MockConfigRepository repository;
    SubscriptionService service(repository);
    Config config;
    config.collection().subscriptions = makeList({
        QStringLiteral("a"), QStringLiteral("hidden"), QStringLiteral("b")});
    config.collection().subscriptions[1].enabled = false;

    const OperationResult result = service.reorderSubscriptions(
        config, {QStringLiteral("b"), QStringLiteral("a")});
    QVERIFY(result.success);
    QCOMPARE(config.collection().subscriptions.size(), 3);
    QCOMPARE(ids(config.collection().subscriptions),
             (QStringList{QStringLiteral("b"), QStringLiteral("a"), QStringLiteral("hidden")}));
}

void SubscriptionOrderTests::serviceReorderIgnoresUnknownIds()
{
    MockConfigRepository repository;
    SubscriptionService service(repository);
    Config config;
    config.collection().subscriptions = makeList({QStringLiteral("a"), QStringLiteral("b")});

    const OperationResult result = service.reorderSubscriptions(
        config, {QStringLiteral("ghost"), QStringLiteral("b"), QStringLiteral("a")});
    QVERIFY(result.success);
    QCOMPARE(ids(config.collection().subscriptions),
             (QStringList{QStringLiteral("b"), QStringLiteral("a")}));
}

void SubscriptionOrderTests::serviceReorderWithSameOrderDoesNotSave()
{
    MockConfigRepository repository;
    SubscriptionService service(repository);
    Config config;
    config.collection().subscriptions = makeList({QStringLiteral("a"), QStringLiteral("b")});

    const OperationResult result = service.reorderSubscriptions(
        config, {QStringLiteral("a"), QStringLiteral("b")});
    QVERIFY(result.success);
    QCOMPARE(repository.saveCount, 0);
}

QTEST_MAIN(SubscriptionOrderTests)
#include "SubscriptionOrderTests.moc"
