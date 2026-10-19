#include "auto/AutoCountrySelection.h"

#include "auto/AutoCountrySupport.h"

#include <QMap>

#include <algorithm>

namespace {

bool hasLatency(const AutoNodeEvaluation& evaluation)
{
    return evaluation.available && evaluation.latencyMs >= 0;
}

// Sort key for "this node answered but reported no latency". Any real measurement is smaller, so a
// measured node outranks an unmeasured one; two unmeasured nodes compare equal and the caller keeps
// the first in list order.
constexpr double kNoReadingScore = 1.0e12;

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
    // Ranking by raw latency is the same ordering as ranking by score when the score *is* the
    // latency, so this delegates rather than keeping a second copy of the loop. That matters
    // because the two used to drift: this one accepted a node with no latency reading and the
    // score-based one skipped it. Expressing "no reading" as a large sentinel keeps the old
    // behaviour (a measured node beats an unmeasured one, and two unmeasured nodes tie, so the
    // first in list order wins) while leaving exactly one selection rule in the codebase.
    QHash<QString, double> latencyScores;
    latencyScores.reserve(evaluations.size());
    for (const AutoNodeEvaluation& evaluation : evaluations) {
        latencyScores.insert(
            evaluation.indexId,
            evaluation.latencyMs >= 0 ? static_cast<double>(evaluation.latencyMs) : kNoReadingScore);
    }
    return bestAutoServerIdForCountryByScore(
        evaluations, countryCode, latencyScores, excludedServerId);
}

QString bestAutoServerIdForCountryByScore(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode,
    const QHash<QString, double>& effectiveScores,
    const QString& excludedServerId)
{
    const QString normalizedCountryCode = normalizeAutoCountryCode(countryCode);
    QString bestId;
    double bestScore = 0.0;
    for (const AutoNodeEvaluation& evaluation : evaluations) {
        if (!evaluation.available
            || !evaluation.tested
            || autoCountryKeyForEvaluation(evaluation) != normalizedCountryCode) {
            continue;
        }
        if (!excludedServerId.isEmpty() && evaluation.indexId == excludedServerId) {
            continue;
        }
        const auto scoreIt = effectiveScores.constFind(evaluation.indexId);
        if (scoreIt == effectiveScores.constEnd()) {
            continue;
        }
        if (bestId.isEmpty() || scoreIt.value() < bestScore) {
            bestId = evaluation.indexId;
            bestScore = scoreIt.value();
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
