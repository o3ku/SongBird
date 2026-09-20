#include "common/RoutingValuePattern.h"

#include <optional>

namespace RoutingValuePattern {
namespace {

struct PrefixEntry {
    const char* text;
    bool validAsDomain;
    bool validAsIp;
    DomainKind domainKind; // meaningful only when validAsDomain
    IpKind ipKind; // meaningful only when validAsIp
};

// Longest first, so a longer prefix is never shadowed by a shorter one that happens to be a
// prefix of it. ("ext:" and "ext-domain:" cannot actually collide -- "ext-domain:" does not
// begin with "ext:" -- but keeping the order stable removes the need to reason about it.)
//
// "ext:" is the one prefix both fields accept: the V2Ray manual documents "ext:file:tag" for
// loading a rule set in the `domain` field and in the `ip` field alike.
const PrefixEntry kPrefixes[] = {
    {"ext-domain:", true, false, DomainKind::Ext, IpKind::Address},
    {"geosite:", true, false, DomainKind::Geosite, IpKind::Address},
    {"keyword:", true, false, DomainKind::Keyword, IpKind::Address},
    {"dotless:", true, false, DomainKind::Dotless, IpKind::Address},
    {"domain:", true, false, DomainKind::Suffix, IpKind::Address},
    {"regexp:", true, false, DomainKind::Regex, IpKind::Address},
    {"geoip:", false, true, DomainKind::Bare, IpKind::GeoIp},
    {"full:", true, false, DomainKind::Exact, IpKind::Address},
    {"ext:", true, true, DomainKind::Ext, IpKind::Ext},
};

bool isHexWord(const QString& word)
{
    for (const QChar ch : word) {
        const char16_t code = ch.unicode();
        const bool isHex = (code >= u'0' && code <= u'9')
            || (code >= u'a' && code <= u'f')
            || (code >= u'A' && code <= u'F');
        if (!isHex) {
            return false;
        }
    }
    return !word.isEmpty();
}

bool containsAsciiLetter(const QString& word)
{
    for (const QChar ch : word) {
        const char16_t code = ch.unicode();
        if ((code >= u'a' && code <= u'z') || (code >= u'A' && code <= u'Z')) {
            return true;
        }
    }
    return false;
}

// Splits "prefix:payload" when -- and only when -- the text before the first colon really
// looks like a prefix word. This is deliberately stricter than "contains a colon", because
// two legitimate values do contain one: an IPv6 literal ("fe80::1") and, in principle, a
// host carrying a port ("example.com:8080"). Treating either as an unknown prefix would
// report a perfectly good rule as broken.
bool splitPrefix(const QString& value, QString& prefix, QString& payload)
{
    const int colon = value.indexOf(QChar(':'));
    if (colon <= 0) {
        return false;
    }

    const QString word = value.left(colon);
    if (word.at(0).isDigit()) {
        return false;
    }
    for (const QChar ch : word) {
        if (!(ch.isLetterOrNumber() || ch == QChar('-'))) {
            return false;
        }
    }

    // Every prefix in the vocabulary contains a letter, so a word made only of hex digits is
    // an IPv6 group ("dead", "fe80"), not a prefix.
    if (!containsAsciiLetter(word) || isHexWord(word)) {
        return false;
    }

    prefix = word + QChar(':');
    payload = value.mid(colon + 1);
    return true;
}

std::optional<DomainKind> domainKindForPrefix(const QString& prefix)
{
    for (const PrefixEntry& entry : kPrefixes) {
        if (entry.validAsDomain && prefix.compare(QLatin1String(entry.text), Qt::CaseInsensitive) == 0) {
            return entry.domainKind;
        }
    }
    return std::nullopt;
}

std::optional<IpKind> ipKindForPrefix(const QString& prefix)
{
    for (const PrefixEntry& entry : kPrefixes) {
        if (entry.validAsIp && prefix.compare(QLatin1String(entry.text), Qt::CaseInsensitive) == 0) {
            return entry.ipKind;
        }
    }
    return std::nullopt;
}

bool isCommentOrBlank(const QString& value)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() || trimmed.startsWith(QChar('#'));
}

void appendIssue(
    QList<Issue>& issues,
    IssueKind kind,
    const QString& field,
    const QString& rawValue,
    const QString& detail)
{
    Issue issue;
    issue.kind = kind;
    issue.field = field;
    issue.value = rawValue.trimmed();
    issue.detail = detail;
    issues.append(issue);
}

} // namespace

DomainValue parseDomain(const QString& value)
{
    DomainValue parsed;
    parsed.value = value.trimmed();
    if (parsed.value.isEmpty()) {
        return parsed;
    }

    // A leading dot already means "this host and its subdomains" to every backend, which is
    // exactly what "domain:" means.
    if (parsed.value.startsWith(QChar('.'))) {
        parsed.kind = DomainKind::Suffix;
        parsed.value = parsed.value.mid(1);
        return parsed;
    }

    QString prefix;
    QString payload;
    if (!splitPrefix(parsed.value, prefix, payload)) {
        return parsed;
    }

    const std::optional<DomainKind> kind = domainKindForPrefix(prefix);
    if (!kind.has_value()) {
        parsed.recognised = false;
        parsed.unknownPrefix = prefix;
        parsed.value.clear();
        return parsed;
    }

    parsed.kind = *kind;
    parsed.prefix = prefix;
    parsed.value = payload.trimmed();
    return parsed;
}

IpValue parseIp(const QString& value)
{
    IpValue parsed;
    parsed.value = value.trimmed();
    if (parsed.value.isEmpty()) {
        return parsed;
    }

    QString prefix;
    QString payload;
    if (!splitPrefix(parsed.value, prefix, payload)) {
        return parsed;
    }

    const std::optional<IpKind> kind = ipKindForPrefix(prefix);
    if (!kind.has_value()) {
        parsed.recognised = false;
        parsed.unknownPrefix = prefix;
        parsed.value.clear();
        return parsed;
    }

    parsed.kind = *kind;
    parsed.prefix = prefix;
    parsed.value = payload.trimmed();
    return parsed;
}

QList<Issue> issuesForValues(const QStringList& domains, const QStringList& ips)
{
    QList<Issue> issues;

    for (const QString& raw : domains) {
        if (isCommentOrBlank(raw)) {
            continue;
        }

        const DomainValue parsed = parseDomain(raw);
        if (!parsed.recognised) {
            appendIssue(issues, IssueKind::UnknownPrefix, QStringLiteral("domain"), raw, parsed.unknownPrefix);
            continue;
        }
        if (parsed.value.isEmpty()) {
            appendIssue(issues, IssueKind::EmptyValue, QStringLiteral("domain"), raw, parsed.prefix);
            continue;
        }
        if (parsed.kind == DomainKind::Dotless) {
            appendIssue(issues, IssueKind::Approximated, QStringLiteral("domain"), raw, parsed.prefix);
        } else if (parsed.kind == DomainKind::Ext) {
            appendIssue(issues, IssueKind::Unsupported, QStringLiteral("domain"), raw, parsed.prefix);
        }
    }

    for (const QString& raw : ips) {
        if (isCommentOrBlank(raw)) {
            continue;
        }

        const IpValue parsed = parseIp(raw);
        if (!parsed.recognised) {
            appendIssue(issues, IssueKind::UnknownPrefix, QStringLiteral("ip"), raw, parsed.unknownPrefix);
            continue;
        }
        if (parsed.value.isEmpty()) {
            appendIssue(issues, IssueKind::EmptyValue, QStringLiteral("ip"), raw, parsed.prefix);
            continue;
        }
        if (parsed.kind == IpKind::Ext) {
            appendIssue(issues, IssueKind::Unsupported, QStringLiteral("ip"), raw, parsed.prefix);
        }
    }

    return issues;
}

} // namespace RoutingValuePattern
