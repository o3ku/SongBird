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
#include "subscription/SubscriptionSkippedNode.h"

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
        return QCoreApplication::translate("SubscriptionContentParser", "non-printable %1 bytes").arg(decoded.size());
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

    // The count is the list's size, and that is the node count: entries are never
    // collapsed into one another, because two entries reading "hysteria \"h1\"" are two
    // nodes that share a name -- the norm on a numbered airport -- and merging them
    // is how a 2000-node subscription came to report "skipped 1 node(s)".
    //
    // The list beside the count is therefore not the entries. It is the types among
    // them, each named once and in the order the nodes appeared, because that is
    // what answers "why did nothing import" without repeating itself forty times.
    QStringList distinctTypes;
    for (const QString& type : types) {
        const QString typeName = SubscriptionSkippedNode::typeName(type);
        if (!distinctTypes.contains(typeName)) {
            distinctTypes.append(typeName);
        }
    }

    // Reaches the user: SubscriptionUpdateService appends this to the "Imported N server(s)"
    // message via appendSkippedSummary(), so it is prose in the log panel, not a diagnostic.
    return QCoreApplication::translate("SubscriptionContentParser", "skipped %1 node(s): %2")
        .arg(types.size())
        .arg(distinctTypes.join(QStringLiteral(", ")));
}

namespace {

// Reports how many nodes the parser had to drop, in one place. The list itself is
// not repeated here: it is what skippedTypes is, and this note is only read when
// something went wrong.
void appendSkippedSummaryNote(SubscriptionParseReport& report)
{
    if (!report.hasSkippedNodes()) {
        return;
    }

    report.notes.append(QStringLiteral("Subscription parse: skipped %1 node(s)").arg(report.skippedTypes.size()));
}

// The skip list of the pass a reader is reporting from, added to what the passes
// before it saw.
//
// Nothing here is filtered by its own contents. An entry names a node, so two
// entries reading "hysteria \"h1\"" are two nodes that share a name -- the norm on a
// numbered airport -- and an address-less list cannot tell them apart from one node
// seen twice. Collapsing them is how a 2000-node subscription came to report
// "skipped 1 node(s)".
//
// What keeps the count honest is the shape of the walk rather than a filter: a
// document is handed to the readers once per layer, and the layers are either empty
// (the base64 encoding of a payload, which no reader can read) or the nodes
// themselves. The one real repeat is inside a pass -- a Clash document written as
// JSON is read by sing-box and again by clash -- and that is handled by taking one
// reading per pass.
void recordSkips(SubscriptionParseReport& report,
                 const QStringList& skippedBeforePass,
                 const QStringList& passSkipped)
{
    // Skipped whether or not anything was imported: a document that imported two
    // nodes and dropped a third is the case this list exists for (the server count
    // shrank and only the skip list can say why), and it is the same list.
    report.skippedTypes = skippedBeforePass + passSkipped;
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
    // The reading of the pass that is currently running; declared outside the loop
    // because the "no parser matched" exit reports it too.
    QStringList passSkipped;
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

        // One reading per pass, not one per reader. Both readers below see the same
        // JSON document and both name the same nodes, so the second reading replaces
        // the first rather than being added to it. A reader that saw nothing (the
        // line walker on a body it cannot read) is no evidence against one that saw
        // nodes, so an empty list never replaces a full one.
        passSkipped.clear();

        QStringList singBoxSkipped;
        const QList<VmessItem> singBoxItems = tryParseSingBox(current, &singBoxSkipped);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 sing-box count=%2").arg(iteration).arg(singBoxItems.size()));
        // Collected before the emptiness check on purpose: a pass that produced
        // nothing is exactly the case whose skips the user needs to see.
        passSkipped = singBoxSkipped;
        if (!singBoxItems.isEmpty()) {
            report.items = singBoxItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=sing-box"));
            recordSkips(report, skipped, passSkipped);
            return report;
        }

        QStringList clashSkipped;
        const QList<VmessItem> clashItems = tryParseClash(current, &clashSkipped);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 clash count=%2").arg(iteration).arg(clashItems.size()));
        if (!clashSkipped.isEmpty()) {
            passSkipped = clashSkipped;
        }
        if (!clashItems.isEmpty()) {
            report.items = clashItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=clash"));
            recordSkips(report, skipped, passSkipped);
            return report;
        }

        // Nobody read this document, so the pass is over: what it saw is added to the
        // accumulation and the walk moves on to the next layer, which is the decoded
        // copy of the same payload.
        skipped.append(passSkipped);
        passSkipped.clear();

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

    // No reader matched, so the passes are all there is: what the last pass saw is
    // added to what the passes before it saw.
    report.skippedTypes = skipped + passSkipped;
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
