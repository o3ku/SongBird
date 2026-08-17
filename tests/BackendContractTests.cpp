#include <QtTest>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/CoreCatalog.h"
#include "runtime/core/CoreDescriptor.h"
#include "runtime/core/ICoreBackend.h"

namespace {

// Populated broadly so every protocol finds the fields it needs; the contract
// under test is "does the backend implement this protocol", not field mapping.
VmessItem minimalServerFor(ConfigType configType)
{
    VmessItem server;
    server.configType = configType;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("aes-128-gcm");
    server.streamSecurity = QStringLiteral("tls");
    server.sni = QStringLiteral("example.com");
    server.username = QStringLiteral("user");
    server.privateKey = QStringLiteral("cHJpdmF0ZS1rZXk=");
    server.peerPublicKey = QStringLiteral("cHVibGljLWtleQ==");
    server.localAddress = QStringLiteral("10.0.0.2");
    return server;
}

// Xray and sing-box both tag the proxy outbound "proxy" but name the protocol
// field differently; Clash-family cores emit a proxies array instead.
QString wireProtocolOf(const QJsonObject& root)
{
    const QJsonArray outbounds = root.value(QStringLiteral("outbounds")).toArray();
    for (const QJsonValue& value : outbounds) {
        const QJsonObject outbound = value.toObject();
        if (outbound.value(QStringLiteral("tag")).toString() != QStringLiteral("proxy")) {
            continue;
        }
        const QString xrayProtocol = outbound.value(QStringLiteral("protocol")).toString();
        return xrayProtocol.isEmpty()
            ? outbound.value(QStringLiteral("type")).toString()
            : xrayProtocol;
    }

    const QJsonArray proxies = root.value(QStringLiteral("proxies")).toArray();
    if (!proxies.isEmpty()) {
        return proxies.at(0).toObject().value(QStringLiteral("type")).toString();
    }

    return {};
}

QString describe(CoreType coreType, ConfigType configType)
{
    return QStringLiteral("%1 / %2")
        .arg(coreTypeDisplayName(coreType))
        .arg(configTypeDisplayName(configType));
}

} // namespace

class BackendContractTests : public QObject {
    Q_OBJECT

private slots:
    void everyRegisteredCoreHasBackend();
    void everyDeclaredProtocolProducesConfig();
    void declaredProtocolsMapToDistinctWireProtocols();
};

void BackendContractTests::everyRegisteredCoreHasBackend()
{
    const QList<CoreDescriptor> descriptors = coreDescriptors();
    QVERIFY(!descriptors.isEmpty());

    for (const CoreDescriptor& descriptor : descriptors) {
        QVERIFY2(
            coreBackend(descriptor.type) != nullptr,
            qPrintable(QStringLiteral("%1 is registered but has no backend")
                           .arg(coreTypeDisplayName(descriptor.type))));
    }
}

void BackendContractTests::everyDeclaredProtocolProducesConfig()
{
    const Config config;

    for (const CoreDescriptor& descriptor : coreDescriptors()) {
        const ICoreBackend* backend = coreBackend(descriptor.type);
        QVERIFY(backend != nullptr);

        for (const ConfigType configType : descriptor.supportedConfigTypes) {
            // Custom configs are copied verbatim rather than generated.
            if (configType == ConfigType::Custom) {
                continue;
            }

            const QJsonObject root = backend->buildClientRoot(config, minimalServerFor(configType));
            QVERIFY2(
                !root.isEmpty(),
                qPrintable(QStringLiteral("%1: declared as supported but produced no config")
                               .arg(describe(descriptor.type, configType))));
            QVERIFY2(
                !wireProtocolOf(root).isEmpty(),
                qPrintable(QStringLiteral("%1: config carries no proxy protocol")
                               .arg(describe(descriptor.type, configType))));
        }
    }
}

void BackendContractTests::declaredProtocolsMapToDistinctWireProtocols()
{
    const Config config;

    for (const CoreDescriptor& descriptor : coreDescriptors()) {
        const ICoreBackend* backend = coreBackend(descriptor.type);
        QVERIFY(backend != nullptr);

        QHash<QString, ConfigType> seenWireProtocols;
        for (const ConfigType configType : descriptor.supportedConfigTypes) {
            if (configType == ConfigType::Custom) {
                continue;
            }

            const QJsonObject root = backend->buildClientRoot(config, minimalServerFor(configType));
            const QString wireProtocol = wireProtocolOf(root);
            if (wireProtocol.isEmpty()) {
                continue; // reported by everyDeclaredProtocolProducesConfig
            }

            if (seenWireProtocols.contains(wireProtocol)) {
                QFAIL(qPrintable(
                    QStringLiteral("%1 maps both %2 and %3 to wire protocol '%4'; one of them has no "
                                   "implementation and fell through to a default branch")
                        .arg(coreTypeDisplayName(descriptor.type))
                        .arg(configTypeDisplayName(seenWireProtocols.value(wireProtocol)))
                        .arg(configTypeDisplayName(configType))
                        .arg(wireProtocol)));
            }
            seenWireProtocols.insert(wireProtocol, configType);
        }
    }
}

QTEST_MAIN(BackendContractTests)
#include "BackendContractTests.moc"
