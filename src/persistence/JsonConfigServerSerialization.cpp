#include "persistence/JsonConfigServerSerialization.h"

#include <QJsonObject>
#include <QJsonValue>

#include "persistence/JsonConfigUtils.h"

namespace {

using namespace JsonConfigUtils;

ConfigType parseConfigType(const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull()) {
        return ConfigType::VMess;
    }

    if (!value.isDouble()) {
        return ConfigType::Unknown;
    }

    switch (value.toInt()) {
    case 1:
        return ConfigType::VMess;
    case 2:
        return ConfigType::Custom;
    case 3:
        return ConfigType::Shadowsocks;
    case 4:
        return ConfigType::Socks;
    case 5:
        return ConfigType::VLESS;
    case 6:
        return ConfigType::Trojan;
    case 7:
        return ConfigType::Hysteria2;
    case 8:
        return ConfigType::TUIC;
    case 9:
        return ConfigType::WireGuard;
    case 11:
        return ConfigType::HTTP;
    case 13:
        return ConfigType::AnyTLS;
    case 14:
        return ConfigType::Naive;
    default:
        return ConfigType::Unknown;
    }
}

CoreType parseCoreType(const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull()) {
        return CoreType::SingBox;
    }

    if (!value.isDouble()) {
        return CoreType::SingBox;
    }

    switch (value.toInt()) {
    case 1:
    case 2:
        return CoreType::Xray;
    case 3:
    case 4:
    case 11:
    case 12:
    case 21:
    case 22:
    case 24:
        return CoreType::SingBox;
    default:
        return CoreType::SingBox;
    }
}

int toConfigTypeValue(ConfigType type)
{
    return static_cast<int>(type);
}

int toCoreTypeValue(CoreType type)
{
    return static_cast<int>(type);
}

} // namespace

namespace JsonConfigServerSerialization {

QList<VmessItem> readServers(const QJsonArray& array)
{
    QList<VmessItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        VmessItem item;
        item.indexId = object.value(QStringLiteral("indexId")).toString();
        item.configType = parseConfigType(object.value(QStringLiteral("configType")));
        item.coreType = parseCoreType(object.value(QStringLiteral("coreType")));
        item.address = object.value(QStringLiteral("address")).toString();
        item.port = object.value(QStringLiteral("port")).toInt(0);
        item.id = object.value(QStringLiteral("id")).toString();
        item.alterId = object.value(QStringLiteral("alterId")).toInt(0);
        item.security = object.value(QStringLiteral("security")).toString();
        item.network = object.value(QStringLiteral("network")).toString();
        item.remarks = object.value(QStringLiteral("remarks")).toString();
        item.headerType = object.value(QStringLiteral("headerType")).toString();
        item.requestHost = object.value(QStringLiteral("requestHost")).toString();
        item.path = object.value(QStringLiteral("path")).toString();
        item.streamSecurity = object.value(QStringLiteral("streamSecurity")).toString();
        item.allowInsecure = object.value(QStringLiteral("allowInsecure")).toString();
        item.testResult = object.value(QStringLiteral("testResult")).toString();
        item.flow = object.value(QStringLiteral("flow")).toString();
        item.packetEncoding = object.value(QStringLiteral("packetEncoding")).toString();
        item.sni = object.value(QStringLiteral("sni")).toString();
        item.preSocksPort = object.value(QStringLiteral("preSocksPort")).toInt(0);
        item.fingerprint = object.value(QStringLiteral("fingerprint")).toString();
        item.publicKey = object.value(QStringLiteral("publicKey")).toString();
        item.shortId = object.value(QStringLiteral("shortId")).toString();
        item.spiderX = object.value(QStringLiteral("spiderX")).toString();
        item.mldsa65Verify = object.value(QStringLiteral("mldsa65Verify")).toString();
        item.echConfigList = object.value(QStringLiteral("echConfigList")).toString();
        item.echForceQuery = object.value(QStringLiteral("echForceQuery")).toString();
        item.cert = object.value(QStringLiteral("cert")).toString();
        item.certSha = object.value(QStringLiteral("certSha")).toString();
        if (object.contains(QStringLiteral("muxEnabled")) && object.value(QStringLiteral("muxEnabled")).isBool()) {
            item.muxEnabled = object.value(QStringLiteral("muxEnabled")).toBool();
        }
        item.extra = object.value(QStringLiteral("extra")).toString();
        item.userAgent = object.value(QStringLiteral("userAgent")).toString();
        item.finalmask = object.value(QStringLiteral("finalmask")).toString();
        item.obfsPassword = object.value(QStringLiteral("obfsPassword")).toString();
        item.upMbps = object.value(QStringLiteral("upMbps")).toString();
        item.downMbps = object.value(QStringLiteral("downMbps")).toString();
        item.congestionControl = object.value(QStringLiteral("congestionControl")).toString();
        item.udpRelayMode = object.value(QStringLiteral("udpRelayMode")).toString();
        item.zeroRttHandshake = object.value(QStringLiteral("zeroRttHandshake")).toBool(false);
        item.privateKey = object.value(QStringLiteral("privateKey")).toString();
        item.peerPublicKey = object.value(QStringLiteral("peerPublicKey")).toString();
        item.localAddress = object.value(QStringLiteral("localAddress")).toString();
        item.wireguardMtu = object.value(QStringLiteral("wireguardMtu")).toInt(0);
        item.reserved = object.value(QStringLiteral("reserved")).toString();
        item.username = object.value(QStringLiteral("username")).toString();
        item.naiveQuic = object.value(QStringLiteral("naiveQuic")).toBool(false);
        item.insecureConcurrency = object.value(QStringLiteral("insecureConcurrency")).toInt(0);
        item.idleSessionCheckInterval = object.value(QStringLiteral("idleSessionCheckInterval")).toString();
        item.idleSessionTimeout = object.value(QStringLiteral("idleSessionTimeout")).toString();
        item.minIdleSession = object.value(QStringLiteral("minIdleSession")).toString();
        item.subId = object.value(QStringLiteral("subscriptionId")).toString();

        const QJsonArray alpnArray = object.value(QStringLiteral("alpn")).toArray();
        for (const QJsonValue& alpnValue : alpnArray) {
            item.alpn.append(alpnValue.toString());
        }

        items.append(item);
    }

    return items;
}

QJsonArray writeServers(const QList<VmessItem>& items)
{
    QJsonArray array;
    for (const VmessItem& item : items) {
        QJsonObject object;
        writeIfNotEmpty(object, QStringLiteral("indexId"), item.indexId);
        if (item.configType != ConfigType::VMess) {
            object.insert(QStringLiteral("configType"), toConfigTypeValue(item.configType));
        }
        if (item.coreType != CoreType::SingBox) {
            object.insert(QStringLiteral("coreType"), toCoreTypeValue(item.coreType));
        }
        writeIfNotEmpty(object, QStringLiteral("address"), item.address);
        writeIfNotDefault(object, QStringLiteral("port"), item.port, 0);
        writeIfNotEmpty(object, QStringLiteral("id"), item.id);
        writeIfNotDefault(object, QStringLiteral("alterId"), item.alterId, 0);
        writeIfNotEmpty(object, QStringLiteral("security"), item.security);
        writeIfNotEmpty(object, QStringLiteral("network"), item.network);
        writeIfNotEmpty(object, QStringLiteral("remarks"), item.remarks);
        writeIfNotEmpty(object, QStringLiteral("headerType"), item.headerType);
        writeIfNotEmpty(object, QStringLiteral("requestHost"), item.requestHost);
        writeIfNotEmpty(object, QStringLiteral("path"), item.path);
        writeIfNotEmpty(object, QStringLiteral("streamSecurity"), item.streamSecurity);
        writeIfNotEmpty(object, QStringLiteral("allowInsecure"), item.allowInsecure);
        writeIfNotEmpty(object, QStringLiteral("flow"), item.flow);
        writeIfNotEmpty(object, QStringLiteral("packetEncoding"), item.packetEncoding);
        writeIfNotEmpty(object, QStringLiteral("sni"), item.sni);
        writeIfNotDefault(object, QStringLiteral("preSocksPort"), item.preSocksPort, 0);
        writeIfNotEmpty(object, QStringLiteral("fingerprint"), item.fingerprint);
        writeIfNotEmpty(object, QStringLiteral("publicKey"), item.publicKey);
        writeIfNotEmpty(object, QStringLiteral("shortId"), item.shortId);
        writeIfNotEmpty(object, QStringLiteral("spiderX"), item.spiderX);
        writeIfNotEmpty(object, QStringLiteral("mldsa65Verify"), item.mldsa65Verify);
        writeIfNotEmpty(object, QStringLiteral("echConfigList"), item.echConfigList);
        writeIfNotEmpty(object, QStringLiteral("echForceQuery"), item.echForceQuery);
        writeIfNotEmpty(object, QStringLiteral("cert"), item.cert);
        writeIfNotEmpty(object, QStringLiteral("certSha"), item.certSha);
        if (item.muxEnabled.has_value()) {
            object.insert(QStringLiteral("muxEnabled"), item.muxEnabled.value());
        }
        writeIfNotEmpty(object, QStringLiteral("extra"), item.extra);
        writeIfNotEmpty(object, QStringLiteral("userAgent"), item.userAgent);
        writeIfNotEmpty(object, QStringLiteral("finalmask"), item.finalmask);
        writeIfNotEmpty(object, QStringLiteral("obfsPassword"), item.obfsPassword);
        writeIfNotEmpty(object, QStringLiteral("upMbps"), item.upMbps);
        writeIfNotEmpty(object, QStringLiteral("downMbps"), item.downMbps);
        writeIfNotEmpty(object, QStringLiteral("congestionControl"), item.congestionControl);
        writeIfNotEmpty(object, QStringLiteral("udpRelayMode"), item.udpRelayMode);
        writeIfTrue(object, QStringLiteral("zeroRttHandshake"), item.zeroRttHandshake);
        writeIfNotEmpty(object, QStringLiteral("privateKey"), item.privateKey);
        writeIfNotEmpty(object, QStringLiteral("peerPublicKey"), item.peerPublicKey);
        writeIfNotEmpty(object, QStringLiteral("localAddress"), item.localAddress);
        writeIfNotDefault(object, QStringLiteral("wireguardMtu"), item.wireguardMtu, 0);
        writeIfNotEmpty(object, QStringLiteral("reserved"), item.reserved);
        writeIfNotEmpty(object, QStringLiteral("username"), item.username);
        writeIfTrue(object, QStringLiteral("naiveQuic"), item.naiveQuic);
        writeIfNotDefault(object, QStringLiteral("insecureConcurrency"), item.insecureConcurrency, 0);
        writeIfNotEmpty(object, QStringLiteral("idleSessionCheckInterval"), item.idleSessionCheckInterval);
        writeIfNotEmpty(object, QStringLiteral("idleSessionTimeout"), item.idleSessionTimeout);
        writeIfNotEmpty(object, QStringLiteral("minIdleSession"), item.minIdleSession);
        writeIfNotEmpty(object, QStringLiteral("subscriptionId"), item.subId);
        writeStringListIfNotEmpty(object, QStringLiteral("alpn"), item.alpn);

        array.append(object);
    }

    return array;
}

} // namespace JsonConfigServerSerialization
