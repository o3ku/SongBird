#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class ClientConfigWriter {
public:
    struct GeneratedConfig {
        QString fileName;
        QJsonObject root;
    };

    struct GeneratedConfigSet {
        GeneratedConfig primary;
        QList<GeneratedConfig> auxiliary;
    };

    explicit ClientConfigWriter(QString customConfigDirectory = {});

    OperationResult writeClientConfig(
        const Config& config,
        const VmessItem& server,
        const QString& filePath,
        int statisticsPort = 0) const;
    OperationResult writeClientConfigs(
        const Config& config,
        const VmessItem& server,
        const QString& filePath,
        int statisticsPort,
        QStringList* auxiliaryPaths) const;

    GeneratedConfigSet generateClientConfigs(
        const Config& config,
        const VmessItem& server,
        int statisticsPort = 0) const;

private:
    OperationResult writeCustomConfig(const VmessItem& server, const QString& filePath) const;
    OperationResult validateServer(const VmessItem& server) const;
    OperationResult writeGeneratedConfig(const GeneratedConfig& generatedConfig, const QString& filePath) const;
    QJsonObject buildRoot(const Config& config, const VmessItem& server, int statisticsPort) const;
    QJsonObject buildLegacyRoot(const Config& config, const VmessItem& server, int statisticsPort) const;
    QJsonObject buildSingBoxRoot(const Config& config, const VmessItem& server, int statisticsPort) const;
    QJsonObject buildTunCompatSingBoxRoot(const Config& config) const;
    QJsonObject buildLegacyDns(const Config& config) const;
    QJsonObject buildLegacyRouting(const Config& config) const;
    QJsonObject buildLegacyApi() const;
    QJsonObject buildLegacyPolicy() const;
    QJsonObject buildSingBoxExperimental(const Config& config, int statisticsPort) const;
    QJsonObject buildTunCompatSingBoxDns() const;
    QJsonObject buildTunCompatSingBoxRoute(const Config& config) const;
    QJsonArray buildTunCompatSingBoxOutbounds(const Config& config) const;
    QJsonArray buildTunCompatRejectRules() const;
    void appendTunCompatProcessRules(QJsonArray& rules) const;
    void appendTunIcmpRouteRule(QJsonArray& rules, const TunModeItem& tun) const;
    void appendSingBoxSniffRules(QJsonArray& rules, const Config& config) const;
    void appendLegacyStatisticsInbound(QJsonArray& inbounds, int statisticsPort) const;
    void appendLegacyStatisticsRouteRule(QJsonObject& routing) const;
    QJsonObject buildSingBoxDns(const Config& config) const;
    QJsonObject buildSingBoxRoute(const Config& config) const;
    QJsonObject buildLog(const Config& config) const;
    QJsonObject buildSingBoxLog(const Config& config) const;
    QJsonArray buildInbounds(const Config& config) const;
    QJsonArray buildSingBoxInbounds(const Config& config) const;
    QJsonArray buildOutbounds(const Config& config, const VmessItem& server) const;
    QJsonArray buildSingBoxOutbounds(const Config& config, const VmessItem& server) const;
    static QJsonObject buildLegacyFragmentOutbound();
    QJsonObject buildPrimaryOutbound(const Config& config, const VmessItem& server) const;
    QJsonObject buildSingBoxPrimaryOutbound(const Config& config, const VmessItem& server) const;
    QJsonObject buildStreamSettings(const Config& config, const VmessItem& server) const;
    QJsonObject buildSingBoxTls(const Config& config, const VmessItem& server) const;
    QJsonObject buildSingBoxTransport(const VmessItem& server) const;

    static QJsonObject buildSocksInbound(const Config& config, bool allowLan, int offset);
    static QJsonObject buildHttpInbound(const Config& config, bool allowLan, int offset);
    static QJsonObject buildSniffing(const Config& config);
    static QJsonObject buildDirectOutbound(const Config& config);
    static QJsonObject buildBlackholeOutbound();
    static QJsonObject buildSingBoxSocksInbound(const Config& config, bool allowLan, int offset);
    static QJsonObject buildSingBoxHttpInbound(const Config& config, bool allowLan, int offset);
    static QJsonObject buildSingBoxDirectOutbound();
    static QJsonObject buildSingBoxBlockOutbound();
    static QJsonObject buildSingBoxTunInbound(const Config& config);
    static QJsonObject buildSingBoxSocksOutbound(
        const QString& tag,
        const QString& server,
        int port,
        bool udpOverTcp = false);
    static const RoutingItem* resolveSelectedRouting(const Config& config);
    static void appendLegacyRoutingRule(QJsonArray& rules, const RoutingRule& source);
    static void appendSingBoxRoutingRule(QJsonArray& rules, const RoutingRule& source);
    static void appendSingBoxRoutingIpRule(QJsonArray& rules, const RoutingRule& source);
    static QJsonObject createLegacyRoutingRule(
        const RoutingRule& source,
        const QStringList& inboundTags,
        const QString& port,
        const QStringList& domains,
        const QStringList& ips,
        const QStringList& protocols,
        const QStringList& processes);
    static QJsonObject createSingBoxRoutingRule(
        const RoutingRule& source,
        const QStringList& inboundTags,
        const QString& port,
        const QStringList& domains,
        const QStringList& ips,
        const QStringList& protocols);
    static void populateSingBoxPortFields(QJsonObject& rule, const QString& port);
    static void populateSingBoxNetworkFields(QJsonObject& rule, const QString& network);
    static void populateSingBoxDomainFields(QJsonObject& rule, const QStringList& domains);
    static void populateSingBoxIpFields(QJsonObject& rule, const QStringList& ips);
    static bool containsLegacyApiInbound(const QJsonArray& inbounds);
    static bool containsLegacyApiRoute(const QJsonArray& rules);
    static QJsonArray buildSingBoxStatisticsInboundTags(const Config& config);
    static QJsonArray buildSingBoxStatisticsOutboundTags();
    static QStringList normalizeRuleValues(
        const QStringList& values,
        bool replaceCommaPlaceholder = false,
        bool removeCommentEntries = false);
    static QString resolveSingBoxNetwork(const VmessItem& server);
    static QString resolveSingBoxOutboundType(ConfigType type);
    static QString normalizeSingBoxLogLevel(const QString& level);
    static QString resolvePrimaryPath(const QString& value);
    static QStringList splitCsv(const QString& value);
    static QString resolveServerName(const VmessItem& server);
    static QString resolveFingerprint(const Config& config, const VmessItem& server);
    static QString resolveRealityFingerprint(const Config& config, const VmessItem& server);
    static QStringList buildTunCompatDnsProcessNames();
    static QStringList buildTunCompatDirectProcessNames();
    static void migrateGeoToRuleSet(QJsonObject& root);
    static bool isRealityTransport(const QString& value);
    static bool isValidRealityShortId(const QString& value);
    static bool resolveAllowInsecure(const QString& value, bool fallbackValue);

    QString resolveCustomConfigPath(const QString& address) const;

    QString customConfigDirectory_;
};
