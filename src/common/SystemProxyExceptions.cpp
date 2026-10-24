#include "common/SystemProxyExceptions.h"

#include <QHostAddress>
#include <QList>

#include <utility>

#include "common/RoutingValuePattern.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/RoutingRule.h"
#include "runtime/RoutingConfigFragments.h"

namespace SystemProxyExceptions {

QString defaultList()
{
    return QStringLiteral(
        "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;"
        "172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*");
}

QStringList split(const QString& value)
{
    QString normalized = value;
    normalized.replace(QChar(','), QChar(';'));

    QStringList parts;
    for (const QString& item : normalized.split(QChar(';'), Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }
    return parts;
}

void appendUnique(QStringList& entries, const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    for (const QString& existing : std::as_const(entries)) {
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    entries.append(trimmed);
}

namespace {

// One ip value as the bypass list should spell it: a single host address, or empty when there is
// nothing WinINET could match.
//
// The list is matched against the text a browser sends as the host of a URL, and that is the
// compressed IPv6 form -- so a rule written out in full, or as the one-address CIDR a core accepts,
// has to be written back the same way or it never matches. "/128" is that one-address CIDR.
QString bypassLiteral(const QString& value)
{
    QString address = value.trimmed();
    if (address.endsWith(QStringLiteral("/128"))) {
        address.chop(4);
    }
    if (address.isEmpty() || address.contains(QLatin1Char('/'))) {
        return QString();
    }

    QHostAddress host;
    if (!host.setAddress(address)) {
        // Not an address this build recognises. Pass the value through rather than dropping it:
        // the entry is visible in the system UI, which is how a typo gets noticed.
        return value.trimmed();
    }
    // toString() is the spelling a URL carries: dotted quad for IPv4, and the compressed form for
    // IPv6 -- which is what the rule was written in as often as not ("2001:0db8:0000:...:0001").
    return host.toString();
}

} // namespace

QStringList routeDerived(const Config& config)
{
    // Same list, same order, as the core reads. The first matching rule decides there, so a second
    // opinion assembled here can disagree with it: a high-priority `proxy` rule covering a host that
    // a lower-priority `direct` rule names sent that host through the system proxy while the core
    // sent it direct. effectiveRoutingRules() is that list -- the legacy DNS fragments already take
    // their direct domains from it for the same reason.
    const std::optional<RoutingItem> selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    const RoutingItem* selectedRoutingPtr = selectedRouting.has_value() ? &selectedRouting.value() : nullptr;
    const QList<RoutingRule> effectiveRules = RoutingConfigFragments::effectiveRoutingRules(config, selectedRoutingPtr);

    QStringList derived;
    for (const RoutingRule& rule : std::as_const(effectiveRules)) {
        if (rule.outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        for (const QString& domainValue : rule.domain) {
            const RoutingValuePattern::DomainValue parsed = RoutingValuePattern::parseDomain(domainValue);
            if (parsed.value.isEmpty()) {
                continue;
            }

            if (parsed.kind == RoutingValuePattern::DomainKind::Suffix) {
                // "domain:example.com" and a leading ".example.com" mean the same thing -- the
                // host and its subdomains -- and the system proxy needs an entry for each.
                appendUnique(derived, parsed.value);
                appendUnique(derived, QStringLiteral("*.%1").arg(parsed.value));
                continue;
            }

            if (parsed.kind == RoutingValuePattern::DomainKind::Exact) {
                appendUnique(derived, parsed.value);
            }
        }

        // The ip field of the same rule says the same thing about literal addresses. WinINET has
        // no CIDR syntax, so a ranged value stays core-only; a plain literal translates as itself.
        for (const QString& ipValue : rule.ip) {
            const RoutingValuePattern::IpValue parsed = RoutingValuePattern::parseIp(ipValue);
            if (!parsed.recognised || parsed.kind != RoutingValuePattern::IpKind::Address) {
                continue;
            }
            const QString literal = bypassLiteral(parsed.value);
            if (!literal.isEmpty()) {
                appendUnique(derived, literal);
            }
        }
    }

    return derived;
}

QString resolve(const Config& config)
{
    const QString configured = config.defaults().defIeProxyExceptions.trimmed();
    QStringList entries = split(configured.isEmpty() ? defaultList() : configured);

    for (const QString& item : routeDerived(config)) {
        appendUnique(entries, item);
    }

    return entries.join(QStringLiteral(";"));
}

} // namespace SystemProxyExceptions
