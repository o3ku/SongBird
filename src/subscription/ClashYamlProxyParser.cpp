#include "subscription/ClashYamlProxyParser.h"

#include <optional>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include "subscription/ClashProxyItemParser.h"
#include "subscription/SubscriptionSkippedNode.h"
#include "subscription/SubscriptionYamlParser.h"
#include "subscription/SubscriptionYamlQuoteStateMachine.h"

namespace {

// A node that yields no item is still a node we saw and could not use, so its
// type belongs in the same skipped report as an unsupported one. Nothing else
// records it: by the time the caller looks, the node is indistinguishable from
// one that was never in the document. A type alone cannot be acted on --
// "skipped 1 node(s): (no type)" names no node the user could go and fix -- so
// SubscriptionSkippedNode::label() puts the name beside it whenever the document
// gave one.

QString fieldText(const QList<QPair<QString, QJsonValue>>& fields, const QString& key)
{
    for (const auto& field : fields) {
        if (field.first == key) {
            return field.second.toString().trimmed();
        }
    }
    return {};
}

QString skippedEntryLabel(const QList<QPair<QString, QJsonValue>>& node)
{
    return SubscriptionSkippedNode::label(fieldText(node, QStringLiteral("type")),
                                          fieldText(node, QStringLiteral("name")));
}

// What the built-in YAML reader cannot read, and where. The walker used to misread
// every one of these as data -- an anchor became a literal "&name" value, a merge
// key became a field named "<<", a block scalar's body became stray fields, a second
// document's proxies were dropped at the break -- so a subscription that used them
// imported wrong nodes or lost whole documents without a word. Detecting them before
// the walk turns a silent misread into a surfaced parse error; reading them for real
// is the yaml-cpp migration's job, not this scan's.
enum class UnsupportedYamlKind {
    Anchor,
    Alias,
    MergeKey,
    BlockScalar,
    Tag,
    SecondDocument
};

struct UnsupportedYaml {
    UnsupportedYamlKind kind = UnsupportedYamlKind::Anchor;
    int line = 0;
};

// The construct names in the parse error. Separate translate() calls, one literal
// each: the localization scan only recognises single-literal calls, so the names
// cannot be folded into the message template as concatenated fragments.
QString unsupportedYamlKindName(UnsupportedYamlKind kind)
{
    switch (kind) {
    case UnsupportedYamlKind::Anchor:
        return QCoreApplication::translate("SubscriptionContentParser", "anchor (&name)");
    case UnsupportedYamlKind::Alias:
        return QCoreApplication::translate("SubscriptionContentParser", "alias (*name)");
    case UnsupportedYamlKind::MergeKey:
        return QCoreApplication::translate("SubscriptionContentParser", "merge key (<<)");
    case UnsupportedYamlKind::BlockScalar:
        return QCoreApplication::translate("SubscriptionContentParser", "block scalar (| or >)");
    case UnsupportedYamlKind::Tag:
        return QCoreApplication::translate("SubscriptionContentParser", "tag (!name)");
    case UnsupportedYamlKind::SecondDocument:
        return QCoreApplication::translate("SubscriptionContentParser", "second document (---)");
    }
    return {};
}

QString unsupportedYamlMessage(const UnsupportedYaml& found)
{
    return QCoreApplication::translate("SubscriptionContentParser", "Unsupported YAML feature (%1) on line %2.")
        .arg(unsupportedYamlKindName(found.kind))
        .arg(found.line);
}

// Whether `position` in `text` starts a node in flow style: the previous non-space
// character is a structural one (`{`, `[`, `,`, `:`), or there is none. An indicator
// mid-scalar (`p*ss`, `a&b`) is literal content, not a construct, so only a start
// counts.
bool isFlowNodeStart(const QString& text, int position)
{
    int previous = position - 1;
    while (previous >= 0 && text.at(previous).isSpace()) {
        --previous;
    }
    if (previous < 0) {
        return true;
    }
    const QChar before = text.at(previous);
    // Written as a lookup, not a chain of QChar comparisons: the localization
    // guard's statement scanner counts braces inside char literals, so a return
    // expression holding '{' or '[' never ends at its own semicolon and the scan
    // swallows the rest of the file as one "statement".
    return QStringLiteral("{[:,").contains(before);
}

// The first unsupported construct in one flow-style item (`{...}`), or nothing. The
// scan is quote-aware: an indicator inside a quoted scalar is data, and an unclosed
// quote ends the scan -- the flow parser reports that item as malformed itself.
std::optional<UnsupportedYaml> findUnsupportedYamlInFlow(const QString& body, int lineNumber)
{
    int position = 0;
    while (position < body.size()) {
        const QChar current = body.at(position);
        if (current == QChar('"') || current == QChar('\'')) {
            const SubscriptionYamlQuoteStateMachine::Scan scan =
                SubscriptionYamlQuoteStateMachine::scanQuotedScalar(body, position);
            if (!scan.ok) {
                return std::nullopt;
            }
            position = scan.quoteEndedAt + 1;
            continue;
        }
        // A merge key is a key, not a value: `<<` at a key start, followed by
        // optional spaces and ':'.
        if (current == QChar('<') && position + 1 < body.size() && body.at(position + 1) == QChar('<')
            && isFlowNodeStart(body, position)) {
            int after = position + 2;
            while (after < body.size() && body.at(after).isSpace()) {
                ++after;
            }
            if (after < body.size() && body.at(after) == QChar(':')) {
                return UnsupportedYaml{UnsupportedYamlKind::MergeKey, lineNumber};
            }
        }
        if ((current == QChar('&') || current == QChar('*') || current == QChar('!')
             || current == QChar('|') || current == QChar('>'))
            && isFlowNodeStart(body, position)) {
            const UnsupportedYamlKind kind = current == QChar('&')   ? UnsupportedYamlKind::Anchor
                : current == QChar('*')                             ? UnsupportedYamlKind::Alias
                : current == QChar('!')                             ? UnsupportedYamlKind::Tag
                                                                    : UnsupportedYamlKind::BlockScalar;
            return UnsupportedYaml{kind, lineNumber};
        }
        ++position;
    }
    return std::nullopt;
}

// The first unsupported construct on one block-style line of the proxies section, or
// nothing. `body` is the line's content after the "- " of a list item, or the whole
// trimmed line for a field line. The value is read raw, before scalar(): a quoted
// "&name" is data, an unquoted one is an anchor, and the processed value has already
// lost which it was.
std::optional<UnsupportedYaml> findUnsupportedYamlOnLine(const QString& body, int lineNumber)
{
    if (body.startsWith(QChar('{'))) {
        return findUnsupportedYamlInFlow(body, lineNumber);
    }
    if (body.startsWith(QChar('&'))) {
        return UnsupportedYaml{UnsupportedYamlKind::Anchor, lineNumber};
    }
    if (body.startsWith(QChar('*'))) {
        return UnsupportedYaml{UnsupportedYamlKind::Alias, lineNumber};
    }
    if (body.startsWith(QChar('!'))) {
        return UnsupportedYaml{UnsupportedYamlKind::Tag, lineNumber};
    }
    const int separator = SubscriptionYamlParser::keyValueSeparator(body);
    if (separator <= 0) {
        return std::nullopt;
    }
    if (body.left(separator).trimmed() == QStringLiteral("<<")) {
        return UnsupportedYaml{UnsupportedYamlKind::MergeKey, lineNumber};
    }
    const QString rawValue = body.mid(separator + 1).trimmed();
    if (rawValue.isEmpty()) {
        return std::nullopt;
    }
    const QChar first = rawValue.at(0);
    if (first == QChar('&')) {
        return UnsupportedYaml{UnsupportedYamlKind::Anchor, lineNumber};
    }
    if (first == QChar('*')) {
        return UnsupportedYaml{UnsupportedYamlKind::Alias, lineNumber};
    }
    if (first == QChar('!')) {
        return UnsupportedYaml{UnsupportedYamlKind::Tag, lineNumber};
    }
    if (first == QChar('|') || first == QChar('>')) {
        return UnsupportedYaml{UnsupportedYamlKind::BlockScalar, lineNumber};
    }
    return std::nullopt;
}

// Whether `trimmed` is a document marker line: `---` starts a document, `...` ends
// one. Either may carry trailing content on the same line (`--- {...}`), which is
// still a separator.
bool isDocumentStart(const QString& trimmed)
{
    return trimmed == QStringLiteral("---") || trimmed.startsWith(QStringLiteral("--- "))
        || trimmed.startsWith(QStringLiteral("---\t"));
}

bool isDocumentEnd(const QString& trimmed)
{
    return trimmed == QStringLiteral("...") || trimmed.startsWith(QStringLiteral("... "))
        || trimmed.startsWith(QStringLiteral("...\t"));
}

// The first unsupported YAML construct in the document, or nothing. Document markers
// are document-global: a second `---` anywhere ends the first document, so they are
// checked over the whole document. Anchors, aliases, merge keys, block scalars and
// tags are only read where the walker reads -- inside `proxies:`. A construct in
// `proxy-groups:` or `rules:` never reaches a node, so failing on it would refuse a
// subscription whose nodes are perfectly readable.
//
// The scan only runs when the document has a `proxies:` section at all: without one
// the walker returns nothing whatever the document holds, and the generic "no
// supported servers" is the honest answer for a document that was never a Clash
// subscription. When both a marker and a section construct are present, the earlier
// line wins.
std::optional<UnsupportedYaml> findUnsupportedYaml(const QStringList& lines)
{
    int firstContent = -1;
    int lastContent = -1;
    int proxiesLine = -1;
    int proxiesIndent = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = SubscriptionYamlParser::stripComment(lines.at(index)).trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (firstContent < 0) {
            firstContent = index;
        }
        lastContent = index;
        if (proxiesLine < 0 && trimmed == QStringLiteral("proxies:")) {
            proxiesLine = index;
            proxiesIndent = SubscriptionYamlParser::indent(lines.at(index));
        }
    }
    if (proxiesLine < 0) {
        return std::nullopt;
    }

    std::optional<UnsupportedYaml> marker;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = SubscriptionYamlParser::stripComment(lines.at(index)).trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        // A leading `---` is the document start, and a trailing `...` is its end;
        // either anywhere else opens or closes a second document.
        if (isDocumentStart(trimmed) && index != firstContent) {
            marker = UnsupportedYaml{UnsupportedYamlKind::SecondDocument, index + 1};
            break;
        }
        if (isDocumentEnd(trimmed) && index != lastContent) {
            marker = UnsupportedYaml{UnsupportedYamlKind::SecondDocument, index + 1};
            break;
        }
    }

    std::optional<UnsupportedYaml> section;
    for (int index = proxiesLine + 1; index < lines.size(); ++index) {
        const QString line = SubscriptionYamlParser::stripComment(lines.at(index));
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        // The walker's own boundary: a non-list line at or above the proxies indent
        // ends the section, and the walk stops there.
        if (SubscriptionYamlParser::indent(line) <= proxiesIndent
            && !SubscriptionYamlParser::isListItem(line)) {
            break;
        }
        const QString body = SubscriptionYamlParser::isListItem(line)
            ? trimmed.mid(2).trimmed()
            : trimmed;
        if (body.isEmpty()) {
            continue;
        }
        if (const std::optional<UnsupportedYaml> found = findUnsupportedYamlOnLine(body, index + 1)) {
            section = found;
            break;
        }
    }

    if (marker.has_value() && (!section.has_value() || marker->line < section->line)) {
        return marker;
    }
    return section;
}

// Read one flow-style list item into the node being built. Returns false when the
// item is not a well-formed object, which is the caller's signal that the entry is
// a fragment and the fields after it belong to nothing.
bool appendFlowFields(const QString& itemBody, QList<QPair<QString, QJsonValue>>* fields)
{
    const QJsonValue flowItem = SubscriptionYamlParser::parseValue(itemBody);
    if (!flowItem.isObject()) {
        return false;
    }

    const QJsonObject flowNode = flowItem.toObject();
    for (auto it = flowNode.begin(); it != flowNode.end(); ++it) {
        fields->append({it.key(), it.value()});
    }
    return true;
}

} // namespace

namespace ClashYamlProxyParser {

QList<VmessItem> parseProxyItems(const QString& content, QStringList* skippedTypes, QString* parseError)
{
    QList<VmessItem> items;
    const QStringList lines = content.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::KeepEmptyParts);

    // Before the walk: a document that uses a YAML feature the reader cannot read is
    // refused whole, with the reason named, instead of being walked into wrong nodes.
    if (const std::optional<UnsupportedYaml> found = findUnsupportedYaml(lines)) {
        if (parseError != nullptr) {
            *parseError = unsupportedYamlMessage(*found);
        }
        return items;
    }

    bool inProxies = false;
    int proxiesIndent = -1;
    // The node being read, as the key/value pairs the document actually wrote.
    //
    // Not a QJsonObject: QJsonObject::insert() drops a key equal to nullptr, so a
    // node that repeats a field or writes an empty one loses the key outright --
    // "type:" with no value is parsed to QJsonValue(QJsonValue::Null), which arrives
    // at insert() as a null string. A node whose type vanished that way used to keep
    // the *previous* node's type when the node was handed over as JSON, so it parsed
    // as a supported protocol carrying another node's address. Keeping the pairs in
    // order and converting once, per node, is what stops the leak.
    QList<QPair<QString, QJsonValue>> currentFields;
    // A node the document opened with "- " but whose fields never arrived. It has
    // no type to report, so it needs its own entry rather than falling through
    // the "unsupported node" path.
    bool nodeWithoutFields = false;
    // A flow-style item that is not a well-formed object. It is reported where it
    // is read, because the block-style fields that follow do not belong to it and
    // must not be added to a fragment of it.
    QString brokenFlowItemLabel;
    // A block-style value whose quote never closes. The value swallowed the rest of
    // the line, so whatever fields were meant to follow it are inside it now: the
    // node is unusable, and saying so is the only way it is not a silent loss. Set
    // both for the item form (`- name: "abc`) and for the field form (`name: "abc`).
    bool hasUnclosedValue = false;
    // The list item that opened this node was a malformed flow entry. Everything
    // after it is the wreckage of that entry, so it is read and discarded instead
    // of being added to a node stitched from two different list items.
    bool inBrokenFlowItem = false;

    const auto reportNodeWithoutFields = [&nodeWithoutFields, skippedTypes]() {
        if (!nodeWithoutFields) {
            return;
        }
        nodeWithoutFields = false;
        if (skippedTypes != nullptr) {
            skippedTypes->append(QCoreApplication::translate(
                "SubscriptionContentParser", "(no fields) here"));
        }
    };

    // The node as a JSON object, built from this node's fields alone and discarded
    // with them.
    //
    // Object reuse was what leaked: `type:` with no value becomes a null QJsonValue,
    // QJsonObject::insert() drops a null key outright, and the key that survived in
    // the reused object was the *previous* node's -- so a node the document wrote
    // without a type arrived as a supported protocol carrying another node's address.
    // A fresh object per node means a dropped key is missing, not stale. Null itself
    // is handed on as an empty string for the same reason: "wrote nothing" must not
    // look like "wrote nothing there" once it reaches a field lookup.
    const auto nodeObject = [&currentFields]() {
        QJsonObject node;
        for (const auto& field : currentFields) {
            node.insert(field.first, field.second.isNull() ? QJsonValue(QString()) : field.second);
        }
        return node;
    };

    const auto flushCurrent = [&items, &currentFields, &nodeWithoutFields, &brokenFlowItemLabel,
                               &hasUnclosedValue, &reportNodeWithoutFields, &nodeObject,
                               skippedTypes]() {
        reportNodeWithoutFields();
        if (!brokenFlowItemLabel.isEmpty()) {
            if (skippedTypes != nullptr) {
                skippedTypes->append(brokenFlowItemLabel);
            }
            brokenFlowItemLabel.clear();
        }
        const bool unclosedValue = hasUnclosedValue;
        hasUnclosedValue = false;
        if (currentFields.isEmpty()) {
            // An unclosed value that never reached a field still names a node the user
            // wrote, and it is the only record that line exists at all.
            if (unclosedValue && skippedTypes != nullptr) {
                skippedTypes->append(QCoreApplication::translate(
                    "SubscriptionContentParser", "(unclosed quoted value) here"));
            }
            return;
        }

        QJsonArray proxyArray;
        proxyArray.append(nodeObject());
        QStringList nodeSkipped;
        const QList<VmessItem> parsed = ClashProxyItemParser::parseProxyArray(proxyArray, &nodeSkipped);
        if (skippedTypes != nullptr) {
            // parseProxyArray records the type it recognised but could not use, with
            // the node's name beside it. Recording nothing means the node was dropped
            // for a reason it does not report, which is precisely the silent loss
            // worth naming -- and the name is the only way to find that node in the
            // document again.
            //
            // An unclosed value outranks the generic label: "this field's quote never
            // closed" is actionable, "(no type) name" is not, and reporting both would
            // count one node twice.
            if (unclosedValue) {
                skippedTypes->append(QCoreApplication::translate(
                    "SubscriptionContentParser", "(unclosed quoted value) here"));
            } else if (nodeSkipped.isEmpty() && parsed.isEmpty()) {
                skippedTypes->append(skippedEntryLabel(currentFields));
            } else {
                skippedTypes->append(nodeSkipped);
            }
        }
        for (const VmessItem& item : parsed) {
            items.append(item);
        }
        currentFields.clear();
    };

    for (const QString& rawLine : lines) {
        const QString line = SubscriptionYamlParser::stripComment(rawLine);

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const int indent = SubscriptionYamlParser::indent(line);
        if (!inProxies) {
            if (trimmed == QStringLiteral("proxies:")) {
                inProxies = true;
                proxiesIndent = indent;
            }
            continue;
        }

        if (indent <= proxiesIndent && !SubscriptionYamlParser::isListItem(line)) {
            break;
        }

        if (SubscriptionYamlParser::isListItem(line)) {
            // Report first, then clear. flushCurrent() is what turns the node that
            // just ended into an item or a skipped entry, and clearing the fields
            // before calling it throws that node away: every node that was missing a
            // required field disappeared together with the one after it, and only the
            // last node in the document was ever reported.
            flushCurrent();
            nodeWithoutFields = false;
            inBrokenFlowItem = false;

            const QString itemBody = trimmed.mid(2).trimmed();
            if (itemBody.startsWith(QChar('{'))) {
                // A flow item that did not parse is a fragment, not a node. The
                // block-style fields that follow belong to no node at all -- they
                // are the remains of the entry that broke -- so they are dropped
                // rather than stitched onto the fragment.
                if (!appendFlowFields(itemBody, &currentFields)) {
                    // `- { name: broken, type: trojan` with no closing brace is not an
                    // object at all. It is reported as itself, not as the node it failed
                    // to become: the fields inside it are an unfinished reading of an
                    // entry, and naming it after one of them points at a field the
                    // document never completed.
                    inBrokenFlowItem = true;
                    brokenFlowItemLabel = QCoreApplication::translate(
                        "SubscriptionContentParser", "(malformed flow item) here");
                }
                continue;
            }
            inBrokenFlowItem = false;

            if (itemBody.isEmpty()) {
                nodeWithoutFields = true;
                continue;
            }

            QString key;
            QString value;
            bool malformedValue = false;
            if (SubscriptionYamlParser::splitKeyValue(itemBody, &key, &value, &malformedValue)) {
                hasUnclosedValue = hasUnclosedValue || malformedValue;
                currentFields.append({key, SubscriptionYamlParser::parseValue(value)});
            } else {
                nodeWithoutFields = true;
            }
            continue;
        }

        QString key;
        QString value;
        bool malformedValue = false;
        if (inBrokenFlowItem) {
            continue;
        }
        if (!SubscriptionYamlParser::splitKeyValue(trimmed, &key, &value, &malformedValue)) {
            continue;
        }
        // The pair is kept (there is no better reading of the line) and the node is
        // reported: the value swallowed the rest of the line, so the fields meant to
        // follow it are inside it and the node cannot be usable.
        hasUnclosedValue = hasUnclosedValue || malformedValue;
        currentFields.append({key, SubscriptionYamlParser::parseValue(value)});
    }

    flushCurrent();
    return items;
}

} // namespace ClashYamlProxyParser
