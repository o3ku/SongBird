#include <QtTest>

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/ProxyGroupTags.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"

namespace {

Config groupBaseConfig()
{
    Config config;
    config.localPort = 10808;
    config.collection().routingModeId = QStringLiteral("custom:1");
    return config;
}

void setProtocolCore(Config& config, ConfigType configType, CoreType coreType)
{
    config.policy().coreTypeItems.append(CoreTypeItem{
        static_cast<int>(configType),
        static_cast<int>(coreType)});
}

VmessItem groupMember(ConfigType configType, const QString& address, const QString& remarks)
{
    VmessItem item;
    item.configType = configType;
    item.address = address;
    item.port = 443;
    item.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    item.security = QStringLiteral("auto");
    item.streamSecurity = QStringLiteral("tls");
    item.sni = address;
    item.remarks = remarks;
    return item;
}

QJsonObject outboundByTag(const QJsonObject& root, const QString& tag)
{
    const QJsonArray outbounds = root.value(QStringLiteral("outbounds")).toArray();
    for (const QJsonValue& value : outbounds) {
        const QJsonObject outbound = value.toObject();
        if (outbound.value(QStringLiteral("tag")).toString() == tag) {
            return outbound;
        }
    }
    return {};
}

} // namespace

class ProxyGroupConfigWriterTests : public QObject {
    Q_OBJECT

private slots:
    void singBoxGroupRootBuildsUrlTestGroupOverMembers();
    void mihomoGroupRootBuildsUrlTestGroupOverMembers();
    void xrayGroupRootIsEmpty();
    void generateClientGroupConfigsDropsForeignCoreAndCustomMembers();
    void generateClientGroupConfigsFailsWhenNoMemberCanRunOnTheLaunchCore();
    void writeClientGroupConfigsWritesPrimaryAndReportsDroppedMembers();
    void writeClientGroupConfigsFlagsGenerationFailuresAsTargetUnusable();
    void generateClientGroupConfigsReAnchorsWhenTheAnchorCoreHasNoGroup();
    void writeClientGroupConfigsLeavesPlumbingFailuresTargetUsable();
};

void ProxyGroupConfigWriterTests::singBoxGroupRootBuildsUrlTestGroupOverMembers()
{
    const Config config = groupBaseConfig();
    const ICoreBackend* backend = coreBackend(CoreType::SingBox);
    QVERIFY(backend != nullptr);

    const QList<VmessItem> members{
        groupMember(ConfigType::VMess, QStringLiteral("a.example.com"), QStringLiteral("a")),
        groupMember(ConfigType::Trojan, QStringLiteral("b.example.com"), QStringLiteral("b")),
    };
    const QJsonObject root = backend->buildClientGroupRoot(config, members);
    QVERIFY(!root.isEmpty());

    // The group takes over the "proxy" tag the route already names.
    const QJsonObject group = outboundByTag(root, QStringLiteral("proxy"));
    QCOMPARE(group.value(QStringLiteral("type")).toString(), QStringLiteral("urltest"));
    const QJsonArray groupMembers = group.value(QStringLiteral("outbounds")).toArray();
    QCOMPARE(groupMembers.size(), 2);
    QCOMPARE(groupMembers.at(0).toString(), proxyGroupMemberTag(1));
    QCOMPARE(groupMembers.at(1).toString(), proxyGroupMemberTag(2));
    // The probe knobs stay unset so the core defaults (generate_204 / 3m / 50ms) apply.
    QVERIFY(!group.contains(QStringLiteral("url")));
    QVERIFY(!group.contains(QStringLiteral("interval")));
    QVERIFY(!group.contains(QStringLiteral("tolerance")));

    QCOMPARE(
        outboundByTag(root, proxyGroupMemberTag(1)).value(QStringLiteral("server")).toString(),
        QStringLiteral("a.example.com"));
    QCOMPARE(
        outboundByTag(root, proxyGroupMemberTag(2)).value(QStringLiteral("server")).toString(),
        QStringLiteral("b.example.com"));
    QVERIFY(!outboundByTag(root, QStringLiteral("direct")).isEmpty());
    QVERIFY(!outboundByTag(root, QStringLiteral("block")).isEmpty());

    QCOMPARE(root.value(QStringLiteral("route")).toObject().value(QStringLiteral("final")).toString(),
             QStringLiteral("proxy"));
}

void ProxyGroupConfigWriterTests::mihomoGroupRootBuildsUrlTestGroupOverMembers()
{
    const Config config = groupBaseConfig();
    const ICoreBackend* backend = coreBackend(CoreType::Mihomo);
    QVERIFY(backend != nullptr);

    const QList<VmessItem> members{
        groupMember(ConfigType::VMess, QStringLiteral("a.example.com"), QStringLiteral("a")),
        groupMember(ConfigType::VMess, QStringLiteral("b.example.com"), QStringLiteral("b")),
    };
    const QJsonObject root = backend->buildClientGroupRoot(config, members);
    QVERIFY(!root.isEmpty());

    // The url-test group takes over the "proxy" group name the rules already reference.
    const QJsonArray groups = root.value(QStringLiteral("proxy-groups")).toArray();
    QCOMPARE(groups.size(), 1);
    const QJsonObject group = groups.at(0).toObject();
    QCOMPARE(group.value(QStringLiteral("name")).toString(), QStringLiteral("proxy"));
    QCOMPARE(group.value(QStringLiteral("type")).toString(), QStringLiteral("url-test"));
    const QJsonArray groupMembers = group.value(QStringLiteral("proxies")).toArray();
    QCOMPARE(groupMembers.size(), 2);
    QCOMPARE(groupMembers.at(0).toString(), proxyGroupMemberTag(1));
    QCOMPARE(groupMembers.at(1).toString(), proxyGroupMemberTag(2));

    const QJsonArray proxies = root.value(QStringLiteral("proxies")).toArray();
    QCOMPARE(proxies.size(), 2);
    QCOMPARE(
        proxies.at(0).toObject().value(QStringLiteral("name")).toString(),
        proxyGroupMemberTag(1));
    QCOMPARE(
        proxies.at(1).toObject().value(QStringLiteral("server")).toString(),
        QStringLiteral("b.example.com"));
}

void ProxyGroupConfigWriterTests::xrayGroupRootIsEmpty()
{
    const Config config = groupBaseConfig();
    const ICoreBackend* backend = coreBackend(CoreType::Xray);
    QVERIFY(backend != nullptr);

    const QList<VmessItem> members{groupMember(ConfigType::VMess, QStringLiteral("a.example.com"), QStringLiteral("a"))};
    // Xray has no native auto-select group; the default reports "unsupported" as an empty root.
    QVERIFY(backend->buildClientGroupRoot(config, members).isEmpty());
}

void ProxyGroupConfigWriterTests::generateClientGroupConfigsDropsForeignCoreAndCustomMembers()
{
    Config config = groupBaseConfig();
    setProtocolCore(config, ConfigType::VMess, CoreType::SingBox);
    setProtocolCore(config, ConfigType::Hysteria2, CoreType::Xray);

    VmessItem customMember = groupMember(ConfigType::Custom, QStringLiteral("custom.json"), QStringLiteral("custom"));
    const QList<VmessItem> members{
        groupMember(ConfigType::VMess, QStringLiteral("a.example.com"), QStringLiteral("a")),
        groupMember(ConfigType::Hysteria2, QStringLiteral("h.example.com"), QStringLiteral("h")),
        customMember,
        groupMember(ConfigType::VMess, QStringLiteral("b.example.com"), QStringLiteral("b")),
    };

    ClientConfigWriter writer;
    const ClientConfigWriter::GroupGeneration generation = writer.generateClientGroupConfigs(config, members);
    QVERIFY(generation.generated.result.success);

    QCOMPARE(generation.embeddedMembers.size(), 2);
    QCOMPARE(generation.embeddedMembers.first().address, QStringLiteral("a.example.com"));
    QCOMPARE(generation.embeddedMembers.last().address, QStringLiteral("b.example.com"));
    const QStringList expectedDropped{QStringLiteral("h"), QStringLiteral("custom")};
    QCOMPARE(generation.droppedMemberNames, expectedDropped);
    QVERIFY(!generation.generated.primary.root.isEmpty());
}

void ProxyGroupConfigWriterTests::generateClientGroupConfigsFailsWhenNoMemberCanRunOnTheLaunchCore()
{
    // Every protocol is pinned to Xray, whose generator has no auto-select group.
    Config config = groupBaseConfig();
    setProtocolCore(config, ConfigType::VMess, CoreType::Xray);

    const QList<VmessItem> members{
        groupMember(ConfigType::VMess, QStringLiteral("a.example.com"), QStringLiteral("a")),
        groupMember(ConfigType::VMess, QStringLiteral("b.example.com"), QStringLiteral("b")),
    };

    ClientConfigWriter writer;
    const ClientConfigWriter::GroupGeneration generation = writer.generateClientGroupConfigs(config, members);
    QVERIFY(!generation.generated.result.success);
    QVERIFY(generation.generated.primary.root.isEmpty());
    QVERIFY2(generation.generated.result.message.contains(QStringLiteral("Xray")),
             qPrintable(generation.generated.result.message));
}

// A mixed-core group anchored on the core without a native auto-select group (Xray): the first
// member that can run on a group-capable core takes over the launch core instead of failing the
// whole group; the foreign anchor is dropped with the other foreign members.
void ProxyGroupConfigWriterTests::generateClientGroupConfigsReAnchorsWhenTheAnchorCoreHasNoGroup()
{
    Config config = groupBaseConfig();
    setProtocolCore(config, ConfigType::VMess, CoreType::Xray);
    setProtocolCore(config, ConfigType::Trojan, CoreType::SingBox);

    const QList<VmessItem> members{
        groupMember(ConfigType::VMess, QStringLiteral("xray-anchor.example.com"), QStringLiteral("xray-anchor")),
        groupMember(ConfigType::Trojan, QStringLiteral("t.example.com"), QStringLiteral("t")),
    };

    ClientConfigWriter writer;
    const ClientConfigWriter::GroupGeneration generation = writer.generateClientGroupConfigs(config, members);
    QVERIFY2(generation.generated.result.success, qPrintable(generation.generated.result.message));

    QCOMPARE(generation.embeddedMembers.size(), 1);
    QCOMPARE(generation.embeddedMembers.first().address, QStringLiteral("t.example.com"));
    QCOMPARE(generation.droppedMemberNames, QStringList{QStringLiteral("xray-anchor")});

    const QJsonArray outbounds = generation.generated.primary.root.value(QStringLiteral("outbounds")).toArray();
    bool sawGroup = false;
    for (const QJsonValue& value : outbounds) {
        const QJsonObject outbound = value.toObject();
        if (outbound.value(QStringLiteral("tag")).toString() == QStringLiteral("proxy")) {
            sawGroup = true;
            QCOMPARE(outbound.value(QStringLiteral("type")).toString(), QStringLiteral("urltest"));
            QCOMPARE(outbound.value(QStringLiteral("outbounds")).toArray().size(), 1);
        }
    }
    QVERIFY(sawGroup);
}

void ProxyGroupConfigWriterTests::writeClientGroupConfigsWritesPrimaryAndReportsDroppedMembers()
{
    Config config = groupBaseConfig();
    setProtocolCore(config, ConfigType::VMess, CoreType::SingBox);
    setProtocolCore(config, ConfigType::Hysteria2, CoreType::Xray);

    const QList<VmessItem> members{
        groupMember(ConfigType::VMess, QStringLiteral("a.example.com"), QStringLiteral("a")),
        groupMember(ConfigType::Hysteria2, QStringLiteral("h.example.com"), QStringLiteral("h")),
    };

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("config.generated.json"));

    ClientConfigWriter writer;
    QList<VmessItem> embeddedMembers;
    QStringList auxiliaryPaths;
    const OperationResult result = writer.writeClientGroupConfigs(
        config, members, filePath, &auxiliaryPaths, &embeddedMembers);
    QVERIFY2(result.success, qPrintable(result.message));

    QVERIFY(QFile::exists(filePath));
    QCOMPARE(embeddedMembers.size(), 1);
    QCOMPARE(embeddedMembers.first().address, QStringLiteral("a.example.com"));
    QVERIFY2(result.message.contains(QStringLiteral("Skipped 1 member(s)")),
             qPrintable(result.message));
    QVERIFY2(result.message.contains(QStringLiteral("h")), qPrintable(result.message));
}

// The unusable-target flag: a generation failure (the member set itself cannot run as a group)
// marks the target spent so the session can offer leaving group mode; a plumbing failure (path,
// disk) does not, because a single-node write on the same disk would fail identically.
void ProxyGroupConfigWriterTests::writeClientGroupConfigsFlagsGenerationFailuresAsTargetUnusable()
{
    Config config = groupBaseConfig();
    setProtocolCore(config, ConfigType::VMess, CoreType::Xray);

    const QList<VmessItem> members{
        groupMember(ConfigType::VMess, QStringLiteral("a.example.com"), QStringLiteral("a")),
    };

    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ClientConfigWriter writer;
    bool targetUnusable = false;
    const OperationResult result = writer.writeClientGroupConfigs(
        config, members, directory.filePath(QStringLiteral("config.generated.json")),
        nullptr, nullptr, &targetUnusable);
    QVERIFY(!result.success);
    QVERIFY(targetUnusable);
}

void ProxyGroupConfigWriterTests::writeClientGroupConfigsLeavesPlumbingFailuresTargetUsable()
{
    Config config = groupBaseConfig();
    setProtocolCore(config, ConfigType::VMess, CoreType::SingBox);

    const QList<VmessItem> members{
        groupMember(ConfigType::VMess, QStringLiteral("a.example.com"), QStringLiteral("a")),
    };

    ClientConfigWriter writer;
    bool targetUnusable = false;
    // An empty runtime path fails before generation is ever consulted.
    const OperationResult result = writer.writeClientGroupConfigs(
        config, members, QString(), nullptr, nullptr, &targetUnusable);
    QVERIFY(!result.success);
    QVERIFY(!targetUnusable);
}

QTEST_MAIN(ProxyGroupConfigWriterTests)
#include "ProxyGroupConfigWriterTests.moc"
