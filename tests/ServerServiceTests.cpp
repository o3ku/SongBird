#include <QtTest>

#include <QUuid>

#include "persistence/IConfigRepository.h"
#include "services/ServerService.h"

namespace {

class MockConfigRepository : public IConfigRepository {
public:
    Config load() override { return config_; }
    bool save(const Config& config) override { config_ = config; return saveSucceeds_; }

    Config config_;
    bool saveSucceeds_ = true;
};

VmessItem makeServer(const QString& indexId, const QString& address, int port,
                     ConfigType configType = ConfigType::VMess, int sort = 0)
{
    VmessItem item;
    item.indexId = indexId;
    item.configType = configType;
    item.address = address;
    item.port = port;
    item.remarks = QStringLiteral("%1:%2").arg(address).arg(port);
    item.sort = sort;
    return item;
}

Config makeConfigWithServers(std::initializer_list<VmessItem> servers)
{
    Config config;
    for (const VmessItem& s : servers) {
        config.servers.append(s);
    }
    if (!config.servers.isEmpty()) {
        config.currentIndexId = config.servers.first().indexId;
    }
    return config;
}

} // namespace

class ServerServiceTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    // addServer
    void addServerGeneratesIndexId();
    void addServerGeneratesAutoRemarksWhenEmpty();
    void addServerSetsSort();
    void addServerSetsDefaultWhenNoneSelected();
    void addServerDoesNotOverrideExistingDefault();
    void addServerRejectsEmptyAddress();
    void addServerRejectsInvalidPort();
    void addServerReturnsFailureWhenSaveFails();
    void addServerAppendsToList();

    // updateServer
    void updateServerModifiesExisting();
    void updateServerPreservesIndexId();
    void updateServerPreservesSort();
    void updateServerPreservesSubId();
    void updateServerPreservesTestResult();
    void updateServerGeneratesAutoRemarksWhenEmpty();
    void updateServerRejectsEmptyIndexId();
    void updateServerRejectsMissingServer();
    void updateServerRejectsEmptyAddress();
    void updateServerRejectsInvalidPort();
    void updateServerReturnsFailureWhenSaveFails();

    // duplicateServer
    void duplicateServerCreatesCopyWithNewIndexId();
    void duplicateServerClearsSubId();
    void duplicateServerClearsTestResult();
    void duplicateServerAppendsCopySuffix();
    void duplicateServerInsertsAfterOriginal();
    void duplicateServerRejectsEmptyIndexId();
    void duplicateServerRejectsMissingServer();
    void duplicateServerReturnsFailureWhenSaveFails();

    // removeServers
    void removeServerRemovesByIndexId();
    void removeServersRemovesMultiple();
    void removeServerUpdatesDefaultIfRemoved();
    void removeServerPicksFirstAsNewDefault();
    void removeServerClearsDefaultIfNoneLeft();
    void removeServerReturnsOkForEmptyList();
    void removeServerReturnsFailureWhenSaveFails();

    // setDefaultServer
    void setDefaultServerUpdatesCurrentIndexId();
    void setDefaultServerRejectsEmptyIndexId();
    void setDefaultServerRejectsMissingServer();
    void setDefaultServerReturnsFailureWhenSaveFails();

    // moveServers
    void moveServerUp();
    void moveServerDown();
    void moveServerTop();
    void moveServerBottom();
    void moveServerUpDoesNothingAtTop();
    void moveServerDownDoesNothingAtBottom();
    void moveServerReturnsOkForEmptyList();
    void moveServerReturnsOkForSingleServer();
    void moveServerReordersMultipleSelectedUp();
    void moveServerReordersMultipleSelectedDown();

    // reorderServers
    void reorderServersAppliesNewOrder();
    void reorderServersAppendsUnlistedAtEnd();
    void reorderServersReturnsOkForEmptyList();
    void reorderServersReturnsOkWhenOrderUnchanged();

    // updateTestResult
    void updateTestResultSetsTrimmedResult();
    void updateTestResultRejectsEmptyIndexId();
    void updateTestResultRejectsMissingServer();
    void updateTestResultReturnsFailureWhenSaveFails();

    // save
    void saveDelegatesToRepository();
    void saveReturnsFalseWhenRepositoryFails();

    // sort normalization
    void sortValuesNormalizedAfterDuplicate();
    void sortValuesNormalizedAfterMove();

    // list
    void listReturnsConfigServers();

private:
    std::unique_ptr<MockConfigRepository> mockOwner_;
    MockConfigRepository* mock_ = nullptr;
    std::unique_ptr<ServerService> service_;

    static const QString kAddr1;
    static const QString kAddr2;
    static const QString kAddr3;
    static const QString kAddr4;
    static const QString kId1;
    static const QString kId2;
    static const QString kId3;
    static const QString kId4;
};

const QString ServerServiceTests::kAddr1 = QStringLiteral("10.0.0.1");
const QString ServerServiceTests::kAddr2 = QStringLiteral("10.0.0.2");
const QString ServerServiceTests::kAddr3 = QStringLiteral("10.0.0.3");
const QString ServerServiceTests::kAddr4 = QStringLiteral("10.0.0.4");
const QString ServerServiceTests::kId1 = QStringLiteral("id-00000001");
const QString ServerServiceTests::kId2 = QStringLiteral("id-00000002");
const QString ServerServiceTests::kId3 = QStringLiteral("id-00000003");
const QString ServerServiceTests::kId4 = QStringLiteral("id-00000004");

void ServerServiceTests::initTestCase() {}
void ServerServiceTests::init()
{
    mockOwner_ = std::make_unique<MockConfigRepository>();
    mock_ = mockOwner_.get();
    service_ = std::make_unique<ServerService>(*mock_);
}

// ---------------------------------------------------------------------------
// addServer
// ---------------------------------------------------------------------------

void ServerServiceTests::addServerGeneratesIndexId()
{
    Config config;
    VmessItem item;
    item.address = kAddr1;
    item.port = 443;
    item.indexId.clear();

    const OperationResult result = service_->addServer(config, item);
    QVERIFY(result.success);
    QVERIFY(!config.servers.first().indexId.isEmpty());
    // Must be a valid UUID without braces
    const QUuid parsed(config.servers.first().indexId);
    QVERIFY(!parsed.isNull());
}

void ServerServiceTests::addServerGeneratesAutoRemarksWhenEmpty()
{
    Config config;
    VmessItem item;
    item.address = kAddr1;
    item.port = 443;
    item.remarks.clear();

    service_->addServer(config, item);
    QCOMPARE(config.servers.first().remarks, QStringLiteral("10.0.0.1:443"));
}

void ServerServiceTests::addServerSetsSort()
{
    Config config;
    VmessItem a;
    a.address = kAddr1;
    a.port = 100;
    service_->addServer(config, a);
    QCOMPARE(config.servers.first().sort, 10);

    VmessItem b;
    b.address = kAddr2;
    b.port = 200;
    service_->addServer(config, b);
    QCOMPARE(config.servers.last().sort, 20);
}

void ServerServiceTests::addServerSetsDefaultWhenNoneSelected()
{
    Config config;
    config.currentIndexId.clear();

    VmessItem item;
    item.address = kAddr1;
    item.port = 443;

    service_->addServer(config, item);
    QCOMPARE(config.currentIndexId, config.servers.first().indexId);
}

void ServerServiceTests::addServerDoesNotOverrideExistingDefault()
{
    Config config;
    config.currentIndexId = kId1;
    config.servers.append(makeServer(kId1, kAddr1, 100));

    VmessItem item;
    item.address = kAddr2;
    item.port = 200;

    service_->addServer(config, item);
    QCOMPARE(config.currentIndexId, kId1);
}

void ServerServiceTests::addServerRejectsEmptyAddress()
{
    Config config;
    VmessItem item;
    item.address = QString();
    item.port = 443;

    const OperationResult result = service_->addServer(config, item);
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("address")));
    QVERIFY(config.servers.isEmpty());
}

void ServerServiceTests::addServerRejectsInvalidPort()
{
    Config config;
    VmessItem item;
    item.address = kAddr1;
    item.port = 0;

    const OperationResult result = service_->addServer(config, item);
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("port")));
    QVERIFY(config.servers.isEmpty());
}

void ServerServiceTests::addServerReturnsFailureWhenSaveFails()
{
    mock_->saveSucceeds_ = false;
    Config config;
    VmessItem item;
    item.address = kAddr1;
    item.port = 443;

    const OperationResult result = service_->addServer(config, item);
    QVERIFY(!result.success);
}

void ServerServiceTests::addServerAppendsToList()
{
    Config config;
    VmessItem a;
    a.address = kAddr1;
    a.port = 100;
    service_->addServer(config, a);

    VmessItem b;
    b.address = kAddr2;
    b.port = 200;
    service_->addServer(config, b);

    QCOMPARE(config.servers.size(), 2);
    QCOMPARE(config.servers.first().address, kAddr1);
    QCOMPARE(config.servers.last().address, kAddr2);
}

// ---------------------------------------------------------------------------
// updateServer
// ---------------------------------------------------------------------------

void ServerServiceTests::updateServerModifiesExisting()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    VmessItem updated;
    updated.address = kAddr2;
    updated.port = 200;

    const OperationResult result = service_->updateServer(config, kId1, updated);
    QVERIFY(result.success);
    QCOMPARE(config.servers.first().address, kAddr2);
    QCOMPARE(config.servers.first().port, 200);
}

void ServerServiceTests::updateServerPreservesIndexId()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    VmessItem updated;
    updated.address = kAddr2;
    updated.port = 200;

    service_->updateServer(config, kId1, updated);
    QCOMPARE(config.servers.first().indexId, kId1);
}

void ServerServiceTests::updateServerPreservesSort()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100, ConfigType::VMess, 42),
    });

    VmessItem updated;
    updated.address = kAddr2;
    updated.port = 200;

    service_->updateServer(config, kId1, updated);
    QCOMPARE(config.servers.first().sort, 42);
}

void ServerServiceTests::updateServerPreservesSubId()
{
    VmessItem original = makeServer(kId1, kAddr1, 100);
    original.subId = QStringLiteral("sub-abc");
    Config config = makeConfigWithServers({original});

    VmessItem updated;
    updated.address = kAddr2;
    updated.port = 200;
    updated.subId = QStringLiteral("should-be-ignored");

    service_->updateServer(config, kId1, updated);
    QCOMPARE(config.servers.first().subId, QStringLiteral("sub-abc"));
}

void ServerServiceTests::updateServerPreservesTestResult()
{
    VmessItem original = makeServer(kId1, kAddr1, 100);
    original.testResult = QStringLiteral("150ms");
    Config config = makeConfigWithServers({original});

    VmessItem updated;
    updated.address = kAddr2;
    updated.port = 200;
    updated.testResult = QStringLiteral("should-be-ignored");

    service_->updateServer(config, kId1, updated);
    QCOMPARE(config.servers.first().testResult, QStringLiteral("150ms"));
}

void ServerServiceTests::updateServerGeneratesAutoRemarksWhenEmpty()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    VmessItem updated;
    updated.address = kAddr2;
    updated.port = 200;
    updated.remarks.clear();

    service_->updateServer(config, kId1, updated);
    QCOMPARE(config.servers.first().remarks, QStringLiteral("10.0.0.2:200"));
}

void ServerServiceTests::updateServerRejectsEmptyIndexId()
{
    Config config;
    VmessItem item;
    item.address = kAddr1;
    item.port = 443;

    const OperationResult result = service_->updateServer(config, QString(), item);
    QVERIFY(!result.success);
}

void ServerServiceTests::updateServerRejectsMissingServer()
{
    Config config;
    VmessItem item;
    item.address = kAddr1;
    item.port = 443;

    const OperationResult result = service_->updateServer(config, kId1, item);
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("does not exist")));
}

void ServerServiceTests::updateServerRejectsEmptyAddress()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    VmessItem updated;
    updated.address.clear();
    updated.port = 200;

    const OperationResult result = service_->updateServer(config, kId1, updated);
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("address")));
}

void ServerServiceTests::updateServerRejectsInvalidPort()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    VmessItem updated;
    updated.address = kAddr2;
    updated.port = -1;

    const OperationResult result = service_->updateServer(config, kId1, updated);
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("port")));
}

void ServerServiceTests::updateServerReturnsFailureWhenSaveFails()
{
    mock_->saveSucceeds_ = false;
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    VmessItem updated;
    updated.address = kAddr2;
    updated.port = 200;

    const OperationResult result = service_->updateServer(config, kId1, updated);
    QVERIFY(!result.success);
}

// ---------------------------------------------------------------------------
// duplicateServer
// ---------------------------------------------------------------------------

void ServerServiceTests::duplicateServerCreatesCopyWithNewIndexId()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->duplicateServer(config, kId1);
    QVERIFY(result.success);
    QCOMPARE(config.servers.size(), 2);

    const VmessItem& original = config.servers.first();
    const VmessItem& dup = config.servers.last();
    QVERIFY(original.indexId != dup.indexId);
    QVERIFY(!dup.indexId.isEmpty());
}

void ServerServiceTests::duplicateServerClearsSubId()
{
    VmessItem original = makeServer(kId1, kAddr1, 100);
    original.subId = QStringLiteral("sub-abc");
    Config config = makeConfigWithServers({original});

    service_->duplicateServer(config, kId1);
    QVERIFY(config.servers.last().subId.isEmpty());
}

void ServerServiceTests::duplicateServerClearsTestResult()
{
    VmessItem original = makeServer(kId1, kAddr1, 100);
    original.testResult = QStringLiteral("99ms");
    Config config = makeConfigWithServers({original});

    service_->duplicateServer(config, kId1);
    QVERIFY(config.servers.last().testResult.isEmpty());
}

void ServerServiceTests::duplicateServerAppendsCopySuffix()
{
    VmessItem original = makeServer(kId1, kAddr1, 100);
    original.remarks = QStringLiteral("my-server");
    Config config = makeConfigWithServers({original});

    service_->duplicateServer(config, kId1);
    QCOMPARE(config.servers.last().remarks, QStringLiteral("my-server copy"));
}

void ServerServiceTests::duplicateServerInsertsAfterOriginal()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
    });

    service_->duplicateServer(config, kId2);
    // Original order: 0=id1, 1=id2, 2=id3
    // After duplicate of id2: 0=id1, 1=id2, 2=dup(id2), 3=id3
    QCOMPARE(config.servers.size(), 4);
    QCOMPARE(config.servers.at(0).indexId, kId1);
    QCOMPARE(config.servers.at(1).indexId, kId2);
    QVERIFY(config.servers.at(2).indexId != kId2);
    QCOMPARE(config.servers.at(2).address, kAddr2); // same address as original
    QCOMPARE(config.servers.at(3).indexId, kId3);
}

void ServerServiceTests::duplicateServerRejectsEmptyIndexId()
{
    Config config;
    const OperationResult result = service_->duplicateServer(config, QString());
    QVERIFY(!result.success);
}

void ServerServiceTests::duplicateServerRejectsMissingServer()
{
    Config config;
    const OperationResult result = service_->duplicateServer(config, kId1);
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("does not exist")));
}

void ServerServiceTests::duplicateServerReturnsFailureWhenSaveFails()
{
    mock_->saveSucceeds_ = false;
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->duplicateServer(config, kId1);
    QVERIFY(!result.success);
}

// ---------------------------------------------------------------------------
// removeServers
// ---------------------------------------------------------------------------

void ServerServiceTests::removeServerRemovesByIndexId()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });

    const OperationResult result = service_->removeServers(config, {kId1});
    QVERIFY(result.success);
    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.first().indexId, kId2);
}

void ServerServiceTests::removeServersRemovesMultiple()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
    });

    const OperationResult result = service_->removeServers(config, {kId1, kId3});
    QVERIFY(result.success);
    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.first().indexId, kId2);
}

void ServerServiceTests::removeServerUpdatesDefaultIfRemoved()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });
    config.currentIndexId = kId1;

    service_->removeServers(config, {kId1});
    QCOMPARE(config.currentIndexId, kId2);
}

void ServerServiceTests::removeServerPicksFirstAsNewDefault()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
    });
    config.currentIndexId = kId1;

    service_->removeServers(config, {kId1});
    QCOMPARE(config.currentIndexId, kId2);
}

void ServerServiceTests::removeServerClearsDefaultIfNoneLeft()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });
    config.currentIndexId = kId1;

    service_->removeServers(config, {kId1});
    QVERIFY(config.servers.isEmpty());
    QVERIFY(config.currentIndexId.isEmpty());
}

void ServerServiceTests::removeServerReturnsOkForEmptyList()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->removeServers(config, {});
    QVERIFY(result.success);
    QCOMPARE(config.servers.size(), 1);
}

void ServerServiceTests::removeServerReturnsFailureWhenSaveFails()
{
    mock_->saveSucceeds_ = false;
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->removeServers(config, {kId1});
    QVERIFY(!result.success);
}

// ---------------------------------------------------------------------------
// setDefaultServer
// ---------------------------------------------------------------------------

void ServerServiceTests::setDefaultServerUpdatesCurrentIndexId()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });
    config.currentIndexId = kId1;

    const OperationResult result = service_->setDefaultServer(config, kId2);
    QVERIFY(result.success);
    QCOMPARE(config.currentIndexId, kId2);
}

void ServerServiceTests::setDefaultServerRejectsEmptyIndexId()
{
    Config config;
    const OperationResult result = service_->setDefaultServer(config, QString());
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("No server selected")));
}

void ServerServiceTests::setDefaultServerRejectsMissingServer()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->setDefaultServer(config, kId2);
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("does not exist")));
}

void ServerServiceTests::setDefaultServerReturnsFailureWhenSaveFails()
{
    mock_->saveSucceeds_ = false;
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });

    const OperationResult result = service_->setDefaultServer(config, kId2);
    QVERIFY(!result.success);
}

// ---------------------------------------------------------------------------
// moveServers
// ---------------------------------------------------------------------------

void ServerServiceTests::moveServerUp()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });

    const OperationResult result = service_->moveServers(config, {kId2}, ServerMoveOperation::Up);
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId2);
    QCOMPARE(config.servers.at(1).indexId, kId1);
}

void ServerServiceTests::moveServerDown()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });

    const OperationResult result = service_->moveServers(config, {kId1}, ServerMoveOperation::Down);
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId2);
    QCOMPARE(config.servers.at(1).indexId, kId1);
}

void ServerServiceTests::moveServerTop()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
    });

    const OperationResult result = service_->moveServers(config, {kId3}, ServerMoveOperation::Top);
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId3);
    QCOMPARE(config.servers.at(1).indexId, kId1);
    QCOMPARE(config.servers.at(2).indexId, kId2);
}

void ServerServiceTests::moveServerBottom()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
    });

    const OperationResult result = service_->moveServers(config, {kId1}, ServerMoveOperation::Bottom);
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId2);
    QCOMPARE(config.servers.at(1).indexId, kId3);
    QCOMPARE(config.servers.at(2).indexId, kId1);
}

void ServerServiceTests::moveServerUpDoesNothingAtTop()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });

    const OperationResult result = service_->moveServers(config, {kId1}, ServerMoveOperation::Up);
    // No move occurred: still reports success but order unchanged
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId1);
    QCOMPARE(config.servers.at(1).indexId, kId2);
}

void ServerServiceTests::moveServerDownDoesNothingAtBottom()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });

    const OperationResult result = service_->moveServers(config, {kId2}, ServerMoveOperation::Down);
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId1);
    QCOMPARE(config.servers.at(1).indexId, kId2);
}

void ServerServiceTests::moveServerReturnsOkForEmptyList()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->moveServers(config, {}, ServerMoveOperation::Up);
    QVERIFY(result.success);
}

void ServerServiceTests::moveServerReturnsOkForSingleServer()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->moveServers(config, {kId1}, ServerMoveOperation::Up);
    QVERIFY(result.success);
}

void ServerServiceTests::moveServerReordersMultipleSelectedUp()
{
    // Servers: A B C D; move C,D up
    // C swaps with B (B not selected):  A C B D
    // D swaps with B (B not selected):  A C D B
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
        makeServer(kId4, kAddr4, 400),
    });

    const OperationResult result = service_->moveServers(
        config, {kId3, kId4}, ServerMoveOperation::Up);
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId1);
    QCOMPARE(config.servers.at(1).indexId, kId3);
    QCOMPARE(config.servers.at(2).indexId, kId4);
    QCOMPARE(config.servers.at(3).indexId, kId2);
}

void ServerServiceTests::moveServerReordersMultipleSelectedDown()
{
    // Servers: A B C D; move A,B down
    // B swaps with C (C not selected):  A C B D
    // A swaps with C (C not selected):  C A B D
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
        makeServer(kId4, kAddr4, 400),
    });

    const OperationResult result = service_->moveServers(
        config, {kId1, kId2}, ServerMoveOperation::Down);
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId3);
    QCOMPARE(config.servers.at(1).indexId, kId1);
    QCOMPARE(config.servers.at(2).indexId, kId2);
    QCOMPARE(config.servers.at(3).indexId, kId4);
}

// ---------------------------------------------------------------------------
// reorderServers
// ---------------------------------------------------------------------------

void ServerServiceTests::reorderServersAppliesNewOrder()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
    });

    const OperationResult result = service_->reorderServers(config, {kId3, kId1, kId2});
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId3);
    QCOMPARE(config.servers.at(1).indexId, kId1);
    QCOMPARE(config.servers.at(2).indexId, kId2);
}

void ServerServiceTests::reorderServersAppendsUnlistedAtEnd()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
    });

    // Reorder only mentions kId3 and kId1; kId2 should be at end
    const OperationResult result = service_->reorderServers(config, {kId3, kId1});
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId3);
    QCOMPARE(config.servers.at(1).indexId, kId1);
    QCOMPARE(config.servers.at(2).indexId, kId2);
}

void ServerServiceTests::reorderServersReturnsOkForEmptyList()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->reorderServers(config, {});
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId1);
}

void ServerServiceTests::reorderServersReturnsOkWhenOrderUnchanged()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });

    const OperationResult result = service_->reorderServers(config, {kId1, kId2});
    QVERIFY(result.success);
    QCOMPARE(config.servers.at(0).indexId, kId1);
    QCOMPARE(config.servers.at(1).indexId, kId2);
}

// ---------------------------------------------------------------------------
// updateTestResult
// ---------------------------------------------------------------------------

void ServerServiceTests::updateTestResultSetsTrimmedResult()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->updateTestResult(
        config, kId1, QStringLiteral("  42ms  "));
    QVERIFY(result.success);
    QCOMPARE(config.servers.first().testResult, QStringLiteral("42ms"));
}

void ServerServiceTests::updateTestResultRejectsEmptyIndexId()
{
    Config config;
    const OperationResult result = service_->updateTestResult(config, QString(), QStringLiteral("ok"));
    QVERIFY(!result.success);
}

void ServerServiceTests::updateTestResultRejectsMissingServer()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->updateTestResult(config, kId2, QStringLiteral("ok"));
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("does not exist")));
}

void ServerServiceTests::updateTestResultReturnsFailureWhenSaveFails()
{
    mock_->saveSucceeds_ = false;
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const OperationResult result = service_->updateTestResult(config, kId1, QStringLiteral("ok"));
    QVERIFY(!result.success);
}

// ---------------------------------------------------------------------------
// save
// ---------------------------------------------------------------------------

void ServerServiceTests::saveDelegatesToRepository()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
    });

    const bool ok = service_->save(config);
    QVERIFY(ok);
    QCOMPARE(mock_->config_.servers.size(), 1);
    QCOMPARE(mock_->config_.servers.first().indexId, kId1);
}

void ServerServiceTests::saveReturnsFalseWhenRepositoryFails()
{
    mock_->saveSucceeds_ = false;
    Config config;
    QVERIFY(!service_->save(config));
}

// ---------------------------------------------------------------------------
// sort normalization
// ---------------------------------------------------------------------------

void ServerServiceTests::sortValuesNormalizedAfterDuplicate()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });
    config.servers[0].sort = 5;
    config.servers[1].sort = 100;

    service_->duplicateServer(config, kId1);
    QCOMPARE(config.servers.size(), 3);
    QCOMPARE(config.servers.at(0).sort, 10);
    QCOMPARE(config.servers.at(1).sort, 20);
    QCOMPARE(config.servers.at(2).sort, 30);
}

void ServerServiceTests::sortValuesNormalizedAfterMove()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
        makeServer(kId3, kAddr3, 300),
    });
    config.servers[0].sort = 7;
    config.servers[1].sort = 50;
    config.servers[2].sort = 999;

    service_->moveServers(config, {kId3}, ServerMoveOperation::Top);
    QCOMPARE(config.servers.at(0).indexId, kId3);
    QCOMPARE(config.servers.at(0).sort, 10);
    QCOMPARE(config.servers.at(1).sort, 20);
    QCOMPARE(config.servers.at(2).sort, 30);
}

// ---------------------------------------------------------------------------
// list
// ---------------------------------------------------------------------------

void ServerServiceTests::listReturnsConfigServers()
{
    Config config = makeConfigWithServers({
        makeServer(kId1, kAddr1, 100),
        makeServer(kId2, kAddr2, 200),
    });

    const QList<VmessItem> listed = service_->list(config);
    QCOMPARE(listed.size(), 2);
    QCOMPARE(listed.at(0).indexId, kId1);
    QCOMPARE(listed.at(1).indexId, kId2);
}

QTEST_MAIN(ServerServiceTests)
#include "ServerServiceTests.moc"
