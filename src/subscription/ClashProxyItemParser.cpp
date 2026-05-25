#include "subscription/ClashProxyItemParser.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include "common/PortValidator.h"
#include "subscription/SubscriptionJsonSupport.h"

using SubscriptionJsonSupport::firstNonEmpty;
using SubscriptionJsonSupport::joinStringList;
using SubscriptionJsonSupport::objectField;
using SubscriptionJsonSupport::parsePortValue;
using SubscriptionJsonSupport::stringListValue;

namespace {

QString firstHeaderValue(const QJsonObject& headers)
{
    return firstNonEmpty(headers, {"Host", "host"});
}

void applyTransportOptions(const QJsonObject& proxy, VmessItem* item)
{
    if (item == nullptr) {
        return;
    }

    const QJsonObject wsOptions = objectField(proxy, {"ws-opts", "ws_opts"});
    if (!wsOptions.isEmpty()) {
        item->network = QStringLiteral("ws");
        if (item->path.isEmpty()) {
            item->path = firstNonEmpty(wsOptions, {"path"});
        }
        if (item->requestHost.isEmpty()) {
            item->requestHost = firstHeaderValue(wsOptions.value(QStringLiteral("headers")).toObject());
        }
    }

    const QJsonObject grpcOptions = objectField(proxy, {"grpc-opts", "grpc_opts"});
    if (!grpcOptions.isEmpty()) {
        item->network = QStringLiteral("grpc");
        if (item->path.isEmpty()) {
            item->path = firstNonEmpty(grpcOptions, {"grpc-service-name", "service-name"});
        }
    }

    const QJsonObject h2Options = objectField(proxy, {"h2-opts", "h2_opts"});
    if (!h2Options.isEmpty()) {
        item->network = QStringLiteral("h2");
        if (item->path.isEmpty()) {
            item->path = firstNonEmpty(h2Options, {"path"});
        }
        if (item->requestHost.isEmpty()) {
            item->requestHost = joinStringList(stringListValue(h2Options.value(QStringLiteral("host"))));
        }
    }

    const QJsonObject httpOptions = objectField(proxy, {"http-opts", "http_opts"});
    if (!httpOptions.isEmpty()) {
        item->network = QStringLiteral("http");
        if (item->path.isEmpty()) {
            const QStringList paths = stringListValue(httpOptions.value(QStringLiteral("path")));
            if (!paths.isEmpty()) {
                item->path = paths.constFirst();
            }
        }
        if (item->requestHost.isEmpty()) {
            item->requestHost = joinStringList(stringListValue(httpOptions.value(QStringLiteral("host"))));
        }
    }

    const QJsonObject realityOptions = objectField(proxy, {"reality-opts", "reality_opts", "realityOpts"});
    if (!realityOptions.isEmpty()) {
        item->streamSecurity = QStringLiteral("reality");
        if (item->publicKey.isEmpty()) {
            item->publicKey = firstNonEmpty(realityOptions, {"public-key", "public_key", "publicKey"});
        }
        if (item->shortId.isEmpty()) {
            item->shortId = firstNonEmpty(realityOptions, {"short-id", "short_id", "shortId"});
        }
    }
}

void applyVmessOptions(const QJsonObject& proxy, VmessItem* item)
{
    item->configType = ConfigType::VMess;
    item->id = firstNonEmpty(proxy, {"uuid"});
    item->alterId = firstNonEmpty(proxy, {"alterId"}).toInt();
    item->security = firstNonEmpty(proxy, {"cipher"});
    if (item->security.isEmpty()) {
        item->security = QStringLiteral("auto");
    }
    item->streamSecurity = firstNonEmpty(proxy, {"tls"});
    if (item->streamSecurity == QStringLiteral("true")) {
        item->streamSecurity = QStringLiteral("tls");
    } else if (item->streamSecurity == QStringLiteral("false")) {
        item->streamSecurity.clear();
    }
    item->sni = firstNonEmpty(proxy, {"sni", "servername"});
    item->allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
    item->fingerprint = firstNonEmpty(proxy, {"client-fingerprint"});
    item->alpn = stringListValue(proxy.value(QStringLiteral("alpn")));

    const QString wsPath = firstNonEmpty(proxy, {"ws-path"});
    if (!wsPath.isEmpty()) {
        item->path = wsPath;
    }
    const QString httpPath = firstNonEmpty(proxy, {"h2-path"});
    if (item->path.isEmpty() && !httpPath.isEmpty()) {
        item->path = httpPath;
    }
}

void applyVlessOptions(const QJsonObject& proxy, VmessItem* item)
{
    item->configType = ConfigType::VLESS;
    item->id = firstNonEmpty(proxy, {"uuid"});
    item->flow = firstNonEmpty(proxy, {"flow"});
    item->security = QStringLiteral("none");
    item->streamSecurity = firstNonEmpty(proxy, {"tls"}) == QStringLiteral("true")
        ? QStringLiteral("tls")
        : QString();
    item->sni = firstNonEmpty(proxy, {"sni", "servername"});
    item->allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
    item->fingerprint = firstNonEmpty(proxy, {"client-fingerprint"});
    item->alpn = stringListValue(proxy.value(QStringLiteral("alpn")));
}

void applyTrojanOptions(const QJsonObject& proxy, VmessItem* item)
{
    item->configType = ConfigType::Trojan;
    item->id = firstNonEmpty(proxy, {"password"});
    item->streamSecurity = QStringLiteral("tls");
    item->sni = firstNonEmpty(proxy, {"sni", "servername"});
    item->allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
    item->fingerprint = firstNonEmpty(proxy, {"client-fingerprint"});
    item->alpn = stringListValue(proxy.value(QStringLiteral("alpn")));
}

void applySocksOrHttpOptions(const QJsonObject& proxy, VmessItem* item, ConfigType type)
{
    item->configType = type;
    item->id = firstNonEmpty(proxy, {"username"});
    item->security = firstNonEmpty(proxy, {"password"});
    item->streamSecurity = firstNonEmpty(proxy, {"tls"}) == QStringLiteral("true")
        ? QStringLiteral("tls")
        : QString();
}

void applyHysteria2Options(const QJsonObject& proxy, VmessItem* item)
{
    item->configType = ConfigType::Hysteria2;
    item->id = firstNonEmpty(proxy, {"password"});
    item->streamSecurity = QStringLiteral("tls");
    item->sni = firstNonEmpty(proxy, {"sni"});
    item->allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
    item->obfsPassword = firstNonEmpty(proxy, {"obfs-password"});
}

void applyTuicOptions(const QJsonObject& proxy, VmessItem* item)
{
    item->configType = ConfigType::TUIC;
    item->id = firstNonEmpty(proxy, {"uuid"});
    item->security = firstNonEmpty(proxy, {"password"});
    item->streamSecurity = QStringLiteral("tls");
    item->sni = firstNonEmpty(proxy, {"sni"});
    item->allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
    item->alpn = stringListValue(proxy.value(QStringLiteral("alpn")));
}

bool applyProtocolOptions(const QJsonObject& proxy, const QString& type, VmessItem* item)
{
    if (type == QStringLiteral("vmess")) {
        applyVmessOptions(proxy, item);
    } else if (type == QStringLiteral("vless")) {
        applyVlessOptions(proxy, item);
    } else if (type == QStringLiteral("trojan")) {
        applyTrojanOptions(proxy, item);
    } else if (type == QStringLiteral("ss")) {
        item->configType = ConfigType::Shadowsocks;
        item->id = firstNonEmpty(proxy, {"password"});
        item->security = firstNonEmpty(proxy, {"cipher"});
    } else if (type == QStringLiteral("socks5") || type == QStringLiteral("socks")) {
        applySocksOrHttpOptions(proxy, item, ConfigType::Socks);
    } else if (type == QStringLiteral("http")) {
        applySocksOrHttpOptions(proxy, item, ConfigType::HTTP);
        item->sni = firstNonEmpty(proxy, {"sni", "servername"});
    } else if (type == QStringLiteral("hysteria2")) {
        applyHysteria2Options(proxy, item);
    } else if (type == QStringLiteral("tuic")) {
        applyTuicOptions(proxy, item);
    } else {
        return false;
    }

    return true;
}

VmessItem baseItemFromProxy(const QJsonObject& proxy)
{
    VmessItem item;
    item.remarks = firstNonEmpty(proxy, {"name"});
    item.address = firstNonEmpty(proxy, {"server"});
    parsePortValue(proxy.value(QStringLiteral("port")), &item.port);
    item.network = firstNonEmpty(proxy, {"network"});
    if (item.network.isEmpty()) {
        item.network = QStringLiteral("tcp");
    }
    item.headerType = QStringLiteral("none");
    item.requestHost = firstNonEmpty(proxy, {"host"});
    item.path = firstNonEmpty(proxy, {"path"});
    return item;
}

} // namespace

namespace ClashProxyItemParser {

QList<VmessItem> parseProxyArray(const QJsonArray& proxies)
{
    QList<VmessItem> items;
    for (const QJsonValue& value : proxies) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject proxy = value.toObject();
        VmessItem item = baseItemFromProxy(proxy);
        const QString type = firstNonEmpty(proxy, {"type"}).toLower();
        if (!applyProtocolOptions(proxy, type, &item)) {
            continue;
        }

        applyTransportOptions(proxy, &item);
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
        if (!item.address.isEmpty() && isValidTcpPort(item.port)) {
            items.append(item);
        }
    }

    return items;
}

} // namespace ClashProxyItemParser
