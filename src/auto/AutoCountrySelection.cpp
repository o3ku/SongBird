#include "auto/AutoCountrySelection.h"

#include "auto/AutoCountrySupport.h"

#include <QMap>

#include <algorithm>

namespace {

bool hasLatency(const AutoNodeEvaluation& evaluation)
{
    return evaluation.available && evaluation.latencyMs >= 0;
}

} // namespace

QString autoCountryKeyForEvaluation(const AutoNodeEvaluation& evaluation)
{
    if (!evaluation.inferredCountryCode.trimmed().isEmpty()) {
        return normalizeAutoCountryCode(evaluation.inferredCountryCode);
    }
    const QString countryCode = normalizeAutoCountryCode(evaluation.countryCode);
    if (!countryCode.isEmpty()) {
        return countryCode;
    }
    const QString countryName = normalizeAutoCountryCode(evaluation.countryName);
    return countryName.isEmpty() ? autoUnknownCountryCode() : countryName;
}

QList<AutoCountrySummary> buildAutoCountrySummaries(const QList<AutoNodeEvaluation>& evaluations)
{
    QMap<QString, AutoCountrySummary> summaries;
    for (const QString& countryCode : autoFixedCountryCodes()) {
        AutoCountrySummary summary;
        summary.countryCode = countryCode;
        summary.displayName = autoCountryDisplayName(countryCode);
        summaries.insert(countryCode, summary);
    }

    for (const AutoNodeEvaluation& evaluation : evaluations) {
        if (!evaluation.available) {
            continue;
        }
        QString countryCode = autoCountryKeyForEvaluation(evaluation);
        if (countryCode.isEmpty() || !summaries.contains(countryCode)) {
            countryCode = autoUnknownCountryCode();
        }
        AutoCountrySummary& summary = summaries[countryCode];
        summary.availableCount += 1;
        if (summary.bestServerId.isEmpty()
            || (hasLatency(evaluation) && (summary.bestLatencyMs < 0 || evaluation.latencyMs < summary.bestLatencyMs))) {
            summary.bestLatencyMs = evaluation.latencyMs;
            summary.bestServerId = evaluation.indexId;
        }
    }

    QList<AutoCountrySummary> result;
    const QStringList fixedCodes = autoFixedCountryCodes();
    result.reserve(fixedCodes.size());
    for (const QString& countryCode : fixedCodes) {
        result.append(summaries.value(countryCode));
    }
    return result;
}

QString bestAutoServerIdForCountry(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode,
    const QString& excludedServerId)
{
    const QString normalizedCountryCode = normalizeAutoCountryCode(countryCode);
    QString bestId;
    qint64 bestLatency = -1;
    for (const AutoNodeEvaluation& evaluation : evaluations) {
        if (!evaluation.available
            || !evaluation.tested
            || autoCountryKeyForEvaluation(evaluation) != normalizedCountryCode) {
            continue;
        }
        if (!excludedServerId.isEmpty() && evaluation.indexId == excludedServerId) {
            continue;
        }
        if (bestId.isEmpty()
            || (evaluation.latencyMs >= 0 && (bestLatency < 0 || evaluation.latencyMs < bestLatency))) {
            bestId = evaluation.indexId;
            bestLatency = evaluation.latencyMs;
        }
    }
    return bestId;
}

int countAvailableAutoServersForCountry(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode)
{
    const QString normalizedCountryCode = normalizeAutoCountryCode(countryCode);
    int count = 0;
    for (const AutoNodeEvaluation& evaluation : evaluations) {
        if (evaluation.available
            && evaluation.tested
            && autoCountryKeyForEvaluation(evaluation) == normalizedCountryCode) {
            ++count;
        }
    }
    return count;
}
