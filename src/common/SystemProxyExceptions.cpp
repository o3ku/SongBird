#include "common/SystemProxyExceptions.h"

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

QStringList routeDerived(const Config& config)
{
    QList<RoutingRule> effectiveRules;
    for (const RoutingRule& rule : config.collection().routingCustomRules) {
        if (rule.enabled) {
            effectiveRules.append(rule);
        }
    }

    const std::optional<RoutingItem> selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    if (selectedRouting.has_value()) {
        for (const RoutingRule& rule : selectedRouting->rules) {
            if (rule.enabled) {
                effectiveRules.append(rule);
            }
        }
    }

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
