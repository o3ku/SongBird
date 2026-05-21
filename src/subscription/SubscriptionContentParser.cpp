#include "subscription/SubscriptionContentParser.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include "subscription/ShareUrlParser.h"

namespace {

QString scalarText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (value.isDouble()) {
        return QString::number(value.toInt());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return {};
}

QString firstNonEmpty(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* rawKey : keys) {
        const QString key = QString::fromLatin1(rawKey);
        const QString value = scalarText(object.value(key));
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QStringList stringListValue(const QJsonValue& value)
{
    QStringList values;
    if (value.isString()) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
        return values;
    }

    if (!value.isArray()) {
        return values;
    }

    const QJsonArray array = value.toArray();
    values.reserve(array.size());
    for (const QJsonValue& entry : array) {
        const QString text = scalarText(entry);
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    return values;
}

QString joinStringList(const QStringList& values)
{
    QStringList trimmed;
    trimmed.reserve(values.size());
    for (const QString& value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty()) {
            trimmed.append(normalized);
        }
    }
    return trimmed.join(QStringLiteral(","));
}

QString compactJson(const QJsonValue& value)
{
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return {};
}

bool parsePortValue(const QJsonValue& value, int* port)
{
    if (port == nullptr) {
        return false;
    }

    if (value.isDouble()) {
        *port = value.toInt();
        return *port > 0;
    }

    if (!value.isString()) {
        return false;
    }

    bool ok = false;
    const int parsed = value.toString().trimmed().toInt(&ok);
    if (!ok || parsed <= 0) {
        return false;
    }

    *port = parsed;
    return true;
}

QString yamlScalar(const QString& text)
{
    QString value = text.trimmed();
    if ((value.startsWith(QChar('"')) && value.endsWith(QChar('"')))
        || (value.startsWith(QChar('\'')) && value.endsWith(QChar('\'')))) {
        value = value.mid(1, value.size() - 2);
    }
    return value.trimmed();
}

bool yamlIsListItem(const QString& line)
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith(QStringLiteral("- "));
}

int yamlIndent(const QString& line)
{
    int count = 0;
    while (count < line.size() && line.at(count).isSpace()) {
        ++count;
    }
    return count;
}

bool yamlSplitKeyValue(const QString& text, QString* key, QString* value)
{
    const int separator = text.indexOf(QChar(':'));
    if (separator <= 0) {
        return false;
    }

    if (key != nullptr) {
        *key = text.left(separator).trimmed();
    }
    if (value != nullptr) {
        *value = yamlScalar(text.mid(separator + 1));
    }
    return true;
}

void skipFlowWhitespace(const QString& text, int* position)
{
    if (position == nullptr) {
        return;
    }
    while (*position < text.size() && text.at(*position).isSpace()) {
        ++(*position);
    }
}

QString parseFlowQuotedString(const QString& text, int* position, bool* ok)
{
    if (ok != nullptr) {
        *ok = false;
    }
    if (position == nullptr || *position >= text.size()) {
        return {};
    }

    const QChar quote = text.at(*position);
    if (quote != QChar('"') && quote != QChar('\'')) {
        return {};
    }

    ++(*position);
    QString result;
    while (*position < text.size()) {
        const QChar current = text.at(*position);
        if (current == quote) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return result;
        }

        if (current == QChar('\\') && *position + 1 < text.size()) {
            ++(*position);
            result.append(text.at(*position));
            ++(*position);
            continue;
        }

        result.append(current);
        ++(*position);
    }

    return {};
}

QJsonValue parseFlowValue(const QString& text, int* position, bool* ok);

QJsonArray parseFlowArray(const QString& text, int* position, bool* ok)
{
    QJsonArray array;
    if (ok != nullptr) {
        *ok = false;
    }
    if (position == nullptr || *position >= text.size() || text.at(*position) != QChar('[')) {
        return array;
    }

    ++(*position);
    while (*position < text.size()) {
        skipFlowWhitespace(text, position);
        if (*position < text.size() && text.at(*position) == QChar(']')) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return array;
        }

        bool valueOk = false;
        const QJsonValue value = parseFlowValue(text, position, &valueOk);
        if (!valueOk) {
            return {};
        }
        array.append(value);

        skipFlowWhitespace(text, position);
        if (*position >= text.size()) {
            return {};
        }
        if (text.at(*position) == QChar(',')) {
            ++(*position);
            continue;
        }
        if (text.at(*position) == QChar(']')) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return array;
        }
        return {};
    }

    return {};
}

QJsonObject parseFlowObject(const QString& text, int* position, bool* ok)
{
    QJsonObject object;
    if (ok != nullptr) {
        *ok = false;
    }
    if (position == nullptr || *position >= text.size() || text.at(*position) != QChar('{')) {
        return object;
    }

    ++(*position);
    while (*position < text.size()) {
        skipFlowWhitespace(text, position);
        if (*position < text.size() && text.at(*position) == QChar('}')) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return object;
        }

        QString key;
        if (*position < text.size() && (text.at(*position) == QChar('"') || text.at(*position) == QChar('\''))) {
            bool keyOk = false;
            key = parseFlowQuotedString(text, position, &keyOk);
            if (!keyOk) {
                return {};
            }
        } else {
            const int keyStart = *position;
            while (*position < text.size() && text.at(*position) != QChar(':')) {
                ++(*position);
            }
            if (*position >= text.size()) {
                return {};
            }
            key = text.mid(keyStart, *position - keyStart).trimmed();
        }

        if (key.isEmpty() || *position >= text.size() || text.at(*position) != QChar(':')) {
            return {};
        }

        ++(*position);
        skipFlowWhitespace(text, position);

        bool valueOk = false;
        const QJsonValue value = parseFlowValue(text, position, &valueOk);
        if (!valueOk) {
            return {};
        }
        object.insert(key, value);

        skipFlowWhitespace(text, position);
        if (*position >= text.size()) {
            return {};
        }
        if (text.at(*position) == QChar(',')) {
            ++(*position);
            continue;
        }
        if (text.at(*position) == QChar('}')) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return object;
        }
        return {};
    }

    return {};
}

QJsonValue parseScalarValue(const QString& text)
{
    const QString normalized = yamlScalar(text);
    if (normalized.isEmpty()) {
        return QString();
    }

    if (normalized.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (normalized.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    if (normalized.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0
        || normalized == QStringLiteral("~")) {
        return QJsonValue(QJsonValue::Null);
    }

    bool integerOk = false;
    const int integerValue = normalized.toInt(&integerOk);
    if (integerOk) {
        return integerValue;
    }

    return normalized;
}

QJsonValue parseFlowValue(const QString& text, int* position, bool* ok)
{
    if (ok != nullptr) {
        *ok = false;
    }
    if (position == nullptr) {
        return {};
    }

    skipFlowWhitespace(text, position);
    if (*position >= text.size()) {
        return {};
    }

    const QChar current = text.at(*position);
    if (current == QChar('{')) {
        bool objectOk = false;
        const QJsonObject object = parseFlowObject(text, position, &objectOk);
        if (ok != nullptr) {
            *ok = objectOk;
        }
        return object;
    }
    if (current == QChar('[')) {
        bool arrayOk = false;
        const QJsonArray array = parseFlowArray(text, position, &arrayOk);
        if (ok != nullptr) {
            *ok = arrayOk;
        }
        return array;
    }
    if (current == QChar('"') || current == QChar('\'')) {
        bool stringOk = false;
        const QString value = parseFlowQuotedString(text, position, &stringOk);
        if (ok != nullptr) {
            *ok = stringOk;
        }
        return value;
    }

    const int start = *position;
    while (*position < text.size()) {
        const QChar ch = text.at(*position);
        if (ch == QChar(',') || ch == QChar('}') || ch == QChar(']')) {
            break;
        }
        ++(*position);
    }

    if (ok != nullptr) {
        *ok = true;
    }
    return parseScalarValue(text.mid(start, *position - start));
}

QJsonValue parseYamlLikeValue(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    if (trimmed.startsWith(QChar('{'))) {
        int position = 0;
        bool ok = false;
        const QJsonObject object = parseFlowObject(trimmed, &position, &ok);
        skipFlowWhitespace(trimmed, &position);
        if (ok && position == trimmed.size()) {
            return object;
        }
    }

    if (trimmed.startsWith(QChar('['))) {
        int position = 0;
        bool ok = false;
        const QJsonArray array = parseFlowArray(trimmed, &position, &ok);
        skipFlowWhitespace(trimmed, &position);
        if (ok && position == trimmed.size()) {
            return array;
        }
    }

    return parseScalarValue(trimmed);
}

QJsonObject objectField(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* rawKey : keys) {
        const QString key = QString::fromLatin1(rawKey);
        const QJsonValue value = object.value(key);
        if (value.isObject()) {
            return value.toObject();
        }
    }
    return {};
}

QString firstHeaderValue(const QJsonObject& headers)
{
    return firstNonEmpty(headers, {"Host", "host"});
}

void applyClashTransportOptions(const QJsonObject& proxy, VmessItem* item)
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

QList<VmessItem> parseClashProxyItems(const QString& content)
{
    QList<VmessItem> items;
    const QStringList lines = content.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::KeepEmptyParts);

    bool inProxies = false;
    int proxiesIndent = -1;
    QJsonObject current;

    const auto flushCurrent = [&items, &current]() {
        if (current.isEmpty()) {
            return;
        }

        QJsonArray proxyArray;
        proxyArray.append(current);
        QList<VmessItem> parsed = SubscriptionContentParser::tryParseClashProxyArray(proxyArray);
        for (const VmessItem& item : parsed) {
            items.append(item);
        }
        current = QJsonObject{};
    };

    for (const QString& rawLine : lines) {
        QString line = rawLine;
        const int commentIndex = line.indexOf(QStringLiteral(" #"));
        if (commentIndex >= 0) {
            line = line.left(commentIndex);
        }

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const int indent = yamlIndent(line);
        if (!inProxies) {
            if (trimmed == QStringLiteral("proxies:")) {
                inProxies = true;
                proxiesIndent = indent;
            }
            continue;
        }

        if (indent <= proxiesIndent && !yamlIsListItem(line)) {
            break;
        }

        if (yamlIsListItem(line)) {
            flushCurrent();
            const QString itemBody = trimmed.mid(2).trimmed();
            if (itemBody.startsWith(QChar('{'))) {
                const QJsonValue flowItem = parseYamlLikeValue(itemBody);
                if (flowItem.isObject()) {
                    current = flowItem.toObject();
                }
                continue;
            }

            QString key;
            QString value;
            if (yamlSplitKeyValue(itemBody, &key, &value)) {
                current.insert(key, parseYamlLikeValue(value));
            }
            continue;
        }

        QString key;
        QString value;
        if (!yamlSplitKeyValue(trimmed, &key, &value)) {
            continue;
        }
        current.insert(key, parseYamlLikeValue(value));
    }

    flushCurrent();
    return items;
}

} // namespace

QList<VmessItem> SubscriptionContentParser::parseMany(const QString& content)
{
    return parseManyWithReport(content).items;
}

SubscriptionParseReport SubscriptionContentParser::parseManyWithReport(const QString& content)
{
    SubscriptionParseReport report;
    QString current = content.trimmed();
    report.notes.append(QStringLiteral("Subscription parse: input length=%1").arg(current.size()));

    for (int iteration = 0; iteration < 8 && !current.isEmpty(); ++iteration) {
        if (QFile dumpFile(QCoreApplication::applicationDirPath() + QStringLiteral("/subscription_debug_pass%1.txt").arg(iteration)); dumpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            dumpFile.write(current.toUtf8());
        }
        const QList<VmessItem> shareItems = ShareUrlParser::parseMany(current);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 share-url count=%2").arg(iteration).arg(shareItems.size()));
        if (!shareItems.isEmpty()) {
            report.items = shareItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=share-url"));
            return report;
        }

        const QList<VmessItem> sipItems = tryParseSip008(current);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 sip008 count=%2").arg(iteration).arg(sipItems.size()));
        if (!sipItems.isEmpty()) {
            report.items = sipItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=sip008"));
            return report;
        }

        const QList<VmessItem> singBoxItems = tryParseSingBox(current);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 sing-box count=%2").arg(iteration).arg(singBoxItems.size()));
        if (!singBoxItems.isEmpty()) {
            report.items = singBoxItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=sing-box"));
            return report;
        }

        const QList<VmessItem> clashItems = tryParseClash(current);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 clash count=%2").arg(iteration).arg(clashItems.size()));
        if (!clashItems.isEmpty()) {
            report.items = clashItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=clash"));
            return report;
        }

        const QString decoded = tryDecodeBase64(current).trimmed();
        if (decoded.isEmpty() || decoded == current) {
            report.notes.append(QStringLiteral("Subscription parse: no further base64 decode on pass %1").arg(iteration));
            break;
        }

        report.notes.append(
            QStringLiteral("Subscription parse: base64 decode pass %1 produced length=%2 prefix=%3")
                .arg(iteration)
                .arg(decoded.size())
                .arg(decoded.left(80).replace(QChar('\n'), QChar(' '))));
        current = decoded;
    }

    report.notes.append(QStringLiteral("Subscription parse: no parser matched"));
    return report;
}

QList<VmessItem> SubscriptionContentParser::tryParse(const QString& content)
{
    QList<VmessItem> items = ShareUrlParser::parseMany(content);
    if (!items.isEmpty()) {
        return items;
    }

    items = tryParseSip008(content);
    if (!items.isEmpty()) {
        return items;
    }

    items = tryParseSingBox(content);
    if (!items.isEmpty()) {
        return items;
    }

    items = tryParseClash(content);
    if (!items.isEmpty()) {
        return items;
    }

    return {};
}

QList<VmessItem> SubscriptionContentParser::tryParseSip008(const QString& content)
{
    QList<VmessItem> items;
    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8());
    if (!document.isObject() && !document.isArray()) {
        return items;
    }

    QJsonArray servers;
    if (document.isArray()) {
        servers = document.array();
    } else {
        servers = document.object().value(QStringLiteral("servers")).toArray();
    }

    for (const QJsonValue& value : servers) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        VmessItem item;
        item.configType = ConfigType::Shadowsocks;
        item.remarks = object.value(QStringLiteral("remarks")).toString();
        item.address = object.value(QStringLiteral("server")).toString();
        item.port = object.value(QStringLiteral("server_port")).toInt(0);
        item.security = object.value(QStringLiteral("method")).toString();
        item.id = object.value(QStringLiteral("password")).toString();

        if (!item.address.isEmpty() && item.port > 0) {
            items.append(item);
        }
    }

    return items;
}

QList<VmessItem> SubscriptionContentParser::tryParseJsonArray(const QJsonArray& array)
{
    QList<VmessItem> items;

    bool allStrings = !array.isEmpty();
    for (const QJsonValue& value : array) {
        if (!value.isString()) {
            allStrings = false;
            break;
        }
    }

    if (allStrings) {
        QStringList lines;
        lines.reserve(array.size());
        for (const QJsonValue& value : array) {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty()) {
                lines.append(text);
            }
        }
        items = parseMany(lines.join(QChar('\n')));
        if (!items.isEmpty()) {
            return items;
        }
    }

    return tryParseClashProxyArray(array);
}

QList<VmessItem> SubscriptionContentParser::tryParseSingBox(const QString& content)
{
    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8());
    if (document.isArray()) {
        return tryParseJsonArray(document.array());
    }

    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("proxies")) && root.value(QStringLiteral("proxies")).isArray()) {
        QList<VmessItem> clashItems = tryParseClashProxyArray(root.value(QStringLiteral("proxies")).toArray());
        if (!clashItems.isEmpty()) {
            return clashItems;
        }
    }

    QJsonArray items;
    const QJsonArray outbounds = root.value(QStringLiteral("outbounds")).toArray();
    const QJsonArray endpoints = root.value(QStringLiteral("endpoints")).toArray();
    for (const QJsonValue& value : outbounds) {
        if (value.isObject()) {
            items.append(value.toObject());
        }
    }
    for (const QJsonValue& value : endpoints) {
        if (value.isObject()) {
            items.append(value.toObject());
        }
    }

    QList<VmessItem> parsed;
    for (const QJsonValue& value : items) {
        QList<VmessItem> current = tryParseSingBoxOutboundObject(value.toObject());
        for (const VmessItem& item : current) {
            parsed.append(item);
        }
    }

    return parsed;
}

QList<VmessItem> SubscriptionContentParser::tryParseSingBoxOutboundObject(const QJsonObject& object)
{
    QList<VmessItem> items;
    const QString type = object.value(QStringLiteral("type")).toString().trimmed().toLower();
    if (type.isEmpty()) {
        return items;
    }

    VmessItem item;
    item.coreType = CoreType::SingBox;
    item.remarks = firstNonEmpty(object, {"tag", "name"});
    item.address = firstNonEmpty(object, {"server"});
    parsePortValue(object.value(QStringLiteral("server_port")), &item.port);
    if (item.port <= 0) {
        parsePortValue(object.value(QStringLiteral("server-port")), &item.port);
    }
    if (item.port <= 0) {
        parsePortValue(object.value(QStringLiteral("port")), &item.port);
    }

    if (type == QStringLiteral("vmess")) {
        item.configType = ConfigType::VMess;
        item.id = firstNonEmpty(object, {"uuid", "id"});
        item.security = firstNonEmpty(object, {"security"});
        if (item.security.isEmpty()) {
            item.security = QStringLiteral("auto");
        }
        item.alterId = firstNonEmpty(object, {"alter_id", "alterId"}).toInt();
    } else if (type == QStringLiteral("vless")) {
        item.configType = ConfigType::VLESS;
        item.id = firstNonEmpty(object, {"uuid"});
        item.flow = firstNonEmpty(object, {"flow"});
        item.security = firstNonEmpty(object, {"encryption"});
        if (item.security.isEmpty()) {
            item.security = QStringLiteral("none");
        }
    } else if (type == QStringLiteral("trojan")) {
        item.configType = ConfigType::Trojan;
        item.id = firstNonEmpty(object, {"password"});
    } else if (type == QStringLiteral("shadowsocks")) {
        item.configType = ConfigType::Shadowsocks;
        item.id = firstNonEmpty(object, {"password"});
        item.security = firstNonEmpty(object, {"method"});
    } else if (type == QStringLiteral("socks")) {
        item.configType = ConfigType::Socks;
        item.id = firstNonEmpty(object, {"username"});
        item.security = firstNonEmpty(object, {"password"});
    } else if (type == QStringLiteral("http")) {
        item.configType = ConfigType::HTTP;
        item.id = firstNonEmpty(object, {"username"});
        item.security = firstNonEmpty(object, {"password"});
    } else if (type == QStringLiteral("hysteria2")) {
        item.configType = ConfigType::Hysteria2;
        item.id = firstNonEmpty(object, {"password"});
        item.streamSecurity = QStringLiteral("tls");
        item.sni = firstNonEmpty(object, {"server_name", "sni"});
        item.obfsPassword = firstNonEmpty(object, {"obfs-password", "obfs_password"});
        item.upMbps = firstNonEmpty(object, {"up_mbps", "upmbps"});
        item.downMbps = firstNonEmpty(object, {"down_mbps", "downmbps"});
    } else if (type == QStringLiteral("tuic")) {
        item.configType = ConfigType::TUIC;
        item.id = firstNonEmpty(object, {"uuid"});
        item.security = firstNonEmpty(object, {"password"});
        item.streamSecurity = QStringLiteral("tls");
        item.sni = firstNonEmpty(object, {"server_name", "sni"});
        item.alpn = stringListValue(object.value(QStringLiteral("alpn")));
        item.congestionControl = firstNonEmpty(object, {"congestion_control"});
        item.udpRelayMode = firstNonEmpty(object, {"udp_relay_mode"});
        item.zeroRttHandshake = firstNonEmpty(object, {"zero_rtt_handshake"}) == QStringLiteral("true");
    } else if (type == QStringLiteral("wireguard")) {
        item.configType = ConfigType::WireGuard;
        item.privateKey = firstNonEmpty(object, {"private_key"});
        item.peerPublicKey = firstNonEmpty(object, {"peer_public_key"});
        item.localAddress = joinStringList(stringListValue(object.value(QStringLiteral("local_address"))));
        item.reserved = joinStringList(stringListValue(object.value(QStringLiteral("reserved"))));
    } else if (type == QStringLiteral("anytls")) {
        item.configType = ConfigType::AnyTLS;
        item.id = firstNonEmpty(object, {"password"});
        item.streamSecurity = QStringLiteral("tls");
        item.sni = firstNonEmpty(object, {"server_name", "sni"});
        item.fingerprint = firstNonEmpty(object, {"utls", "fingerprint"});
        item.alpn = stringListValue(object.value(QStringLiteral("alpn")));
    } else if (type == QStringLiteral("naive")) {
        item.configType = ConfigType::Naive;
        item.username = firstNonEmpty(object, {"username"});
        item.id = firstNonEmpty(object, {"password"});
        item.streamSecurity = QStringLiteral("tls");
    } else {
        return items;
    }

    item.network = firstNonEmpty(object, {"network"});
    if (item.network.isEmpty()) {
        item.network = firstNonEmpty(object, {"transport"});
    }
    if (item.network.isEmpty()) {
        item.network = QStringLiteral("tcp");
    }
    item.headerType = firstNonEmpty(object, {"packet_encoding", "headerType"});
    if (item.headerType.isEmpty()) {
        item.headerType = QStringLiteral("none");
    }
    if (item.streamSecurity.isEmpty()) {
        item.streamSecurity = firstNonEmpty(object, {"tls", "security"});
    }
    item.sni = item.sni.isEmpty() ? firstNonEmpty(object, {"tls_server_name", "servername"}) : item.sni;
    item.requestHost = firstNonEmpty(object, {"host"});
    item.path = firstNonEmpty(object, {"path", "service_name"});
    item.allowInsecure = firstNonEmpty(object, {"insecure", "allow_insecure"});
    item.fingerprint = item.fingerprint.isEmpty() ? firstNonEmpty(object, {"fingerprint", "client_fingerprint"}) : item.fingerprint;
    item.publicKey = firstNonEmpty(object, {"reality_public_key", "public_key"});
    item.shortId = firstNonEmpty(object, {"reality_short_id", "short_id"});
    item.extra = compactJson(object.value(QStringLiteral("transport")));

    if (!item.address.isEmpty() && item.port > 0) {
        items.append(item);
    }
    return items;
}

QList<VmessItem> SubscriptionContentParser::tryParseClash(const QString& content)
{
    QList<VmessItem> items = parseClashProxyItems(content);
    if (!items.isEmpty()) {
        return items;
    }

    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8());
    if (document.isObject()) {
        const QJsonValue proxies = document.object().value(QStringLiteral("proxies"));
        if (proxies.isArray()) {
            return tryParseClashProxyArray(proxies.toArray());
        }
    }

    return {};
}

QList<VmessItem> SubscriptionContentParser::tryParseClashProxyArray(const QJsonArray& proxies)
{
    QList<VmessItem> items;
    for (const QJsonValue& value : proxies) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject proxy = value.toObject();
        const QString type = firstNonEmpty(proxy, {"type"}).toLower();
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

        if (type == QStringLiteral("vmess")) {
            item.configType = ConfigType::VMess;
            item.id = firstNonEmpty(proxy, {"uuid"});
            item.alterId = firstNonEmpty(proxy, {"alterId"}).toInt();
            item.security = firstNonEmpty(proxy, {"cipher"});
            if (item.security.isEmpty()) {
                item.security = QStringLiteral("auto");
            }
            item.streamSecurity = firstNonEmpty(proxy, {"tls"});
            if (item.streamSecurity == QStringLiteral("true")) {
                item.streamSecurity = QStringLiteral("tls");
            } else if (item.streamSecurity == QStringLiteral("false")) {
                item.streamSecurity.clear();
            }
            item.sni = firstNonEmpty(proxy, {"sni", "servername"});
            item.allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
            item.fingerprint = firstNonEmpty(proxy, {"client-fingerprint"});
            item.alpn = stringListValue(proxy.value(QStringLiteral("alpn")));

            const QString wsPath = firstNonEmpty(proxy, {"ws-path"});
            if (!wsPath.isEmpty()) {
                item.path = wsPath;
            }
            const QString httpPath = firstNonEmpty(proxy, {"h2-path"});
            if (item.path.isEmpty() && !httpPath.isEmpty()) {
                item.path = httpPath;
            }
        } else if (type == QStringLiteral("vless")) {
            item.configType = ConfigType::VLESS;
            item.id = firstNonEmpty(proxy, {"uuid"});
            item.flow = firstNonEmpty(proxy, {"flow"});
            item.security = QStringLiteral("none");
            item.streamSecurity = firstNonEmpty(proxy, {"tls"}) == QStringLiteral("true")
                ? QStringLiteral("tls")
                : QString();
            item.sni = firstNonEmpty(proxy, {"sni", "servername"});
            item.allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
            item.fingerprint = firstNonEmpty(proxy, {"client-fingerprint"});
            item.alpn = stringListValue(proxy.value(QStringLiteral("alpn")));
        } else if (type == QStringLiteral("trojan")) {
            item.configType = ConfigType::Trojan;
            item.id = firstNonEmpty(proxy, {"password"});
            item.streamSecurity = QStringLiteral("tls");
            item.sni = firstNonEmpty(proxy, {"sni", "servername"});
            item.allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
            item.fingerprint = firstNonEmpty(proxy, {"client-fingerprint"});
            item.alpn = stringListValue(proxy.value(QStringLiteral("alpn")));
        } else if (type == QStringLiteral("ss")) {
            item.configType = ConfigType::Shadowsocks;
            item.id = firstNonEmpty(proxy, {"password"});
            item.security = firstNonEmpty(proxy, {"cipher"});
        } else if (type == QStringLiteral("socks5") || type == QStringLiteral("socks")) {
            item.configType = ConfigType::Socks;
            item.id = firstNonEmpty(proxy, {"username"});
            item.security = firstNonEmpty(proxy, {"password"});
            item.streamSecurity = firstNonEmpty(proxy, {"tls"}) == QStringLiteral("true")
                ? QStringLiteral("tls")
                : QString();
        } else if (type == QStringLiteral("http")) {
            item.configType = ConfigType::HTTP;
            item.id = firstNonEmpty(proxy, {"username"});
            item.security = firstNonEmpty(proxy, {"password"});
            item.streamSecurity = firstNonEmpty(proxy, {"tls"}) == QStringLiteral("true")
                ? QStringLiteral("tls")
                : QString();
            item.sni = firstNonEmpty(proxy, {"sni", "servername"});
        } else if (type == QStringLiteral("hysteria2")) {
            item.configType = ConfigType::Hysteria2;
            item.id = firstNonEmpty(proxy, {"password"});
            item.streamSecurity = QStringLiteral("tls");
            item.sni = firstNonEmpty(proxy, {"sni"});
            item.allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
            item.obfsPassword = firstNonEmpty(proxy, {"obfs-password"});
        } else if (type == QStringLiteral("tuic")) {
            item.configType = ConfigType::TUIC;
            item.id = firstNonEmpty(proxy, {"uuid"});
            item.security = firstNonEmpty(proxy, {"password"});
            item.streamSecurity = QStringLiteral("tls");
            item.sni = firstNonEmpty(proxy, {"sni"});
            item.allowInsecure = firstNonEmpty(proxy, {"skip-cert-verify"});
            item.alpn = stringListValue(proxy.value(QStringLiteral("alpn")));
        } else {
            continue;
        }

        applyClashTransportOptions(proxy, &item);
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
        if (!item.address.isEmpty() && item.port > 0) {
            items.append(item);
        }
    }

    return items;
}

QString SubscriptionContentParser::tryDecodeBase64(const QString& content)
{
    QByteArray normalized = content.trimmed().toUtf8();
    normalized.replace('-', '+');
    normalized.replace('_', '/');
    normalized.replace("\r", "");
    normalized.replace("\n", "");

    const int padding = normalized.size() % 4;
    if (padding > 0) {
        normalized.append(QByteArray(4 - padding, '='));
    }

    return QByteArray::fromBase64(normalized);
}
