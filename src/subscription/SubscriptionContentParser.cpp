#include "subscription/SubscriptionContentParser.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QStringList>

#include "common/PortValidator.h"
#include "subscription/ClashSubscriptionParser.h"
#include "subscription/ShareUrlParser.h"
#include "subscription/SubscriptionJsonSupport.h"
#include "subscription/SubscriptionSingBoxParser.h"

using SubscriptionJsonSupport::parsePortValue;

namespace {

// How much of a decoded payload is looked at when deciding whether it is text at
// all. Only the head carries a format hint anyway.
constexpr int kPrefixCharacters = 80;

// Whether a decoded payload is text at all, judged on its head: that is the only
// part that carries a format hint anyway, and a layer that decoded into binary has
// no further layer to unwrap. Deciding this is what stops the walk from decoding
// noise -- and from spending its budget on it.
bool isDecodableText(const QString& decoded)
{
    const QString head = decoded.left(kPrefixCharacters);
    if (head.isEmpty()) {
        return false;
    }

    int printable = 0;
    for (const QChar ch : head) {
        // Newline counts as structure: it is the one hint that separates a share-link
        // list from a YAML document.
        if (ch.isPrint() || ch == QChar('\n') || ch == QChar('\r') || ch == QChar('\t')) {
            ++printable;
        }
    }

    // A stray high byte is fine; a payload that is mostly not text is not text.
    return printable * 4 >= head.size() * 3;
}

// What the decoded payload looks like, for the operator reading the note. A binary
// payload decoded into a run of replacement characters says nothing; the byte count
// is the only fact it carries, so it says that instead.
QString readablePrefix(const QString& decoded)
{
    if (!isDecodableText(decoded)) {
        return QStringLiteral("non-printable %1 bytes").arg(decoded.size());
    }

    QString prefix = decoded.left(kPrefixCharacters);
    prefix.replace(QChar('\r'), QChar(' '));
    prefix.replace(QChar('\n'), QChar('|'));
    return prefix;
}

} // namespace

QString SubscriptionParseReport::skippedSummary() const
{
    return skippedSummary(skippedTypes);
}

QString SubscriptionParseReport::skippedSummary(const QStringList& types)
{
    if (types.isEmpty()) {
        return {};
    }

    QStringList distinctTypes;
    for (const QString& type : types) {
        if (!distinctTypes.contains(type)) {
            distinctTypes.append(type);
        }
    }

    // Reaches the user: SubscriptionUpdateService appends this to the "Imported N server(s)"
    // message via appendSkippedSummary(), so it is prose in the log panel, not a diagnostic.
    return QCoreApplication::translate("SubscriptionContentParser", "skipped %1 node(s): %2")
        .arg(types.size())
        .arg(distinctTypes.join(QStringLiteral(", ")));
}

namespace {

// Reports what the parser had to drop, in one place.
//
// The skip list covers every pass, not just the selected one, so a node the
// parser could not read is still counted when nothing at all was imported: base64
// decoding makes the second pass reparse the same nodes as text. The entries are
// deduplicated first, so the count the user reads is the node count.
void appendSkippedSummaryNote(SubscriptionParseReport& report)
{
    if (!report.hasSkippedNodes()) {
        return;
    }

    report.notes.append(QStringLiteral("Subscription parse: skipped %1 node(s)").arg(report.skippedTypes.size()));
}

// Two skips are the same skip when they describe the same node: ClashYamlProxyParser
// names the node ("hysteria \"h1\"") so a 2000-node subscription stays navigable,
// while the other parsers only have the protocol. NodeFingerprint does not help
// here -- it keys endpoints, not the nodes that had none.
//
// First appearance wins, so the list reads in document order. The seen-set is not a
// micro-optimisation: base64 bodies are walked several times, so the entry list is a
// multiple of the node count, and a linear scan of the output turns this quadratic in
// exactly the case the skip list exists for -- an airport whose nodes are all
// unsupported. A QSet does it in one pass.
QStringList uniqueSkippedNodes(const QStringList& entries)
{
    QSet<QString> seen;
    seen.reserve(entries.size());
    QStringList distinct;
    for (const QString& entry : entries) {
        if (seen.contains(entry)) {
            continue;
        }
        seen.insert(entry);
        distinct.append(entry);
    }
    return distinct;
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

    // A parser that produced no item can still say why it produced none: every node
    // it saw was a type it does not support. That is the answer to "why did my
    // subscription import nothing", and returning early with an empty report threw
    // it away -- the failure message then read "no supported servers" beside a run
    // of count=0 lines, none of which named the protocol. Kept as the union of
    // every pass for that reason, and deduplicated when it is rendered.
    //
    // It also covers nodes dropped from a document that did import: a node missing
    // its port shrinks the server count, and only the skip list can say which one
    // it was. The two cases are the same question at different sizes, so there is
    // one list rather than two.
    QStringList skipped;
    bool exhaustedPasses = false;

    for (int iteration = 0; iteration < 8 && !current.isEmpty(); ++iteration) {
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

        QStringList singBoxSkipped;
        const QList<VmessItem> singBoxItems = tryParseSingBox(current, &singBoxSkipped);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 sing-box count=%2").arg(iteration).arg(singBoxItems.size()));
        // Collected before the emptiness check on purpose: a pass that produced
        // nothing is exactly the case whose skips the user needs to see.
        skipped.append(singBoxSkipped);
        if (!singBoxItems.isEmpty()) {
            report.items = singBoxItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=sing-box"));
            report.skippedTypes = uniqueSkippedNodes(skipped);
            return report;
        }

        QStringList clashSkipped;
        const QList<VmessItem> clashItems = tryParseClash(current, &clashSkipped);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 clash count=%2").arg(iteration).arg(clashItems.size()));
        skipped.append(clashSkipped);
        if (!clashItems.isEmpty()) {
            report.items = clashItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=clash"));
            report.skippedTypes = uniqueSkippedNodes(skipped);
            return report;
        }

        const QString decoded = tryDecodeBase64(current).trimmed();
        if (decoded.isEmpty() || decoded == current) {
            report.notes.append(QStringLiteral("Subscription parse: no further base64 decode on pass %1").arg(iteration));
            break;
        }

        // The pass counter is bounded, so the walk can stop while a decoded layer is
        // still unread: "no parser matched" alone would blame the content when what
        // ran out was the budget.
        if (iteration + 1 == 8) {
            exhaustedPasses = true;
        }

        // The prefix is shown decoded, not as the base64 of a prefix: four characters
        // of base64 are three bytes, so what was printed was the encoding of a slice
        // of the text, which is not what the user pasted and cannot be read as a
        // format. A newline marker is inserted for the same reason -- it is the one
        // structural hint that separates a share-link list from a YAML document.
        //
        // A layer that decoded into binary is the end of the walk. It is offered to
        // the parsers once -- that is the one attempt the note describes -- and then
        // the walk stops: base64 of noise is noise, so the next pass would decode the
        // same bytes again, the report would fill with identical lines, and the note
        // would claim a pass limit was reached on a payload with no layers left in it.
        report.notes.append(
            QStringLiteral("Subscription parse: base64 decode pass %1 produced length=%2 prefix=%3")
                .arg(iteration)
                .arg(decoded.size())
                .arg(readablePrefix(decoded)));
        current = isDecodableText(decoded) ? decoded : QString();
    }

    // Every pass is represented, deduplicated, because the report answers "why is
    // this subscription short of nodes" and an entry repeated once per pass makes
    // the count it prints wrong. A skipped node is then reachable through
    // skippedTypes, so the note says "Subscription parse: skipped N node(s): ..."
    // without repeating the list itself -- a diagnostic belongs in the text that is
    // only read when something goes wrong.
    report.skippedTypes = uniqueSkippedNodes(skipped);
    if (exhaustedPasses) {
        report.notes.append(QStringLiteral("Subscription parse: pass limit reached"));
    }
    report.notes.append(QStringLiteral("Subscription parse: no parser matched"));
    appendSkippedSummaryNote(report);
    return report;
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
        parsePortValue(object.value(QStringLiteral("server_port")), &item.port);
        item.security = object.value(QStringLiteral("method")).toString();
        item.id = object.value(QStringLiteral("password")).toString();

        if (!item.address.isEmpty() && isValidTcpPort(item.port)) {
            items.append(item);
        }
    }

    return items;
}

QList<VmessItem> SubscriptionContentParser::tryParseJsonArray(const QJsonArray& array, QStringList* skippedTypes)
{
    QList<VmessItem> items;

    bool allStrings = !array.isEmpty();
    for (const QJsonValue& value : array) {
        if (!value.isString()) {
            allStrings = false;
            break;
        }
    }

    // A branch that produces no item still saw nodes, and returning before the caller
    // hears about them is how a JSON array of share links that all failed to parse
    // reported no reason at all -- the same loss the JSON "proxies" branch below had.
    // The nested parse keeps its own report, so its skips are handed on as they are.
    if (allStrings) {
        QStringList lines;
        lines.reserve(array.size());
        for (const QJsonValue& value : array) {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty()) {
                lines.append(text);
            }
        }
        const SubscriptionParseReport nested = parseManyWithReport(lines.join(QChar('\n')));
        items = nested.items;
        if (!items.isEmpty()) {
            return items;
        }
        if (skippedTypes != nullptr) {
            skippedTypes->append(nested.skippedTypes);
        }
    }

    return ClashSubscriptionParser::parseProxyArray(array, skippedTypes);
}

QList<VmessItem> SubscriptionContentParser::tryParseSingBox(const QString& content, QStringList* skippedTypes)
{
    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8());
    if (document.isArray()) {
        return tryParseJsonArray(document.array(), skippedTypes);
    }

    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("proxies")) && root.value(QStringLiteral("proxies")).isArray()) {
        QStringList proxiesSkipped;
        QList<VmessItem> clashItems =
            ClashSubscriptionParser::parseProxyArray(root.value(QStringLiteral("proxies")).toArray(), &proxiesSkipped);
        // Reported before the emptiness check, like the YAML reader does: a Clash
        // document written as JSON whose nodes are all unsupported is the same answer
        // to the same question, and it used to be the one shape that answered nothing.
        if (skippedTypes != nullptr) {
            skippedTypes->append(proxiesSkipped);
        }
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
        QList<VmessItem> current =
            SubscriptionSingBoxParser::parseOutboundObject(value.toObject(), skippedTypes);
        for (const VmessItem& item : current) {
            parsed.append(item);
        }
    }

    return parsed;
}

QList<VmessItem> SubscriptionContentParser::tryParseClash(const QString& content, QStringList* skippedTypes)
{
    return ClashSubscriptionParser::parseContent(content, skippedTypes);
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

    // The length is passed explicitly, and it has to be: a payload that decodes to
    // binary starts with a NUL byte as often as not, and the implicit QByteArray ->
    // QString conversion stops there, so the decode reported success and handed back
    // an empty string. Every caller then saw "no parser matched" for a payload that
    // had been decoded perfectly.
    const QByteArray decoded = QByteArray::fromBase64(normalized);
    return QString::fromLatin1(decoded.constData(), decoded.size());
}
