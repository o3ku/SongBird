#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/// The vocabulary that routing rule values use to carry their match type.
///
/// A `RoutingRule::domain` entry such as "domain:example.com" encodes its match type in a
/// textual prefix. Each backend used to re-implement that `startsWith` chain on its own and
/// the copies drifted apart:
///
///   * "domain:" resolved to an exact host match in mihomo but to a suffix match in sing-box,
///     so one rule meant two different things depending on which core was selected.
///   * A prefix a backend did not know about fell through to its keyword branch, which
///     produced a rule matching the literal text "keyword:example" -- a rule that could
///     never match anything, with nothing reported to the user.
///
/// This namespace is the single place that knows the vocabulary. It deliberately carries no
/// user-facing text: the routing settings page turns the structured `Issue`s below into
/// translated messages.
///
/// The vocabulary follows the V2Ray routing manual, which is where these values originate:
/// a bare string matches any part of the host, "domain:" the host and its subdomains,
/// "full:" the host exactly, plus "regexp:", "geosite:" and "ext:". "keyword:" and
/// "dotless:" are xray extensions that the manual does not describe.
namespace RoutingValuePattern {

enum class DomainKind {
    Bare, ///< no prefix; matches any part of the host, like "keyword:"
    Exact, ///< "full:" -- the host itself, no subdomains
    Suffix, ///< "domain:" -- the host and every subdomain of it
    Keyword, ///< "keyword:" -- any host containing the text
    Regex, ///< "regexp:" -- regular expression
    Geosite, ///< "geosite:" -- named rule set from geosite.dat
    Dotless, ///< "dotless:" -- xray extension, not described by the manual
    Ext, ///< "ext:" / "ext-domain:" -- rule set loaded from a file
};

enum class IpKind {
    Address, ///< no prefix; a literal address or CIDR
    GeoIp, ///< "geoip:" -- country code, or "private"
    Ext, ///< "ext:" -- rule set loaded from a file
};

struct DomainValue {
    DomainKind kind = DomainKind::Bare;
    /// The payload with the prefix removed. Empty when `recognised` is false, so a caller that
    /// ignores `recognised` drops the value instead of mis-typing it as a bare hostname.
    QString value;
    QString prefix; ///< the recognised prefix; empty for Bare
    QString unknownPrefix; ///< set when a "word:" prefix was not recognised
    bool recognised = true; ///< false when `unknownPrefix` is set
};

struct IpValue {
    IpKind kind = IpKind::Address;
    /// The payload with the prefix removed; empty when `recognised` is false.
    QString value;
    QString prefix;
    QString unknownPrefix;
    bool recognised = true;
};

/// Parses one `RoutingRule::domain` entry. A leading '.' is read as Suffix with the dot
/// removed, which is the shorthand every backend already treats that way.
DomainValue parseDomain(const QString& value);

/// Parses one `RoutingRule::ip` entry. An IPv6 literal is never mistaken for a prefix.
IpValue parseIp(const QString& value);

enum class IssueKind {
    UnknownPrefix, ///< a "word:" prefix no backend here defines
    EmptyValue, ///< a recognised prefix with nothing after it
    Approximated, ///< expressible, but not with the same meaning on every core
    Unsupported, ///< not expressible by mihomo or sing-box at all
};

struct Issue {
    IssueKind kind = IssueKind::UnknownPrefix;
    QString field; ///< "domain" or "ip", untranslated
    QString value; ///< the offending value, trimmed, as the user wrote it
    QString detail; ///< the prefix involved, e.g. "dotless:"
};

/// Collects everything about these values that the selected core cannot honour faithfully.
/// Empty when every value is portable. Comments and blank entries are ignored.
QList<Issue> issuesForValues(const QStringList& domains, const QStringList& ips);

} // namespace RoutingValuePattern
