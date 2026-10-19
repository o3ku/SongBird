#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include "auto/AutoTypes.h"

QString autoCountryKeyForEvaluation(const AutoNodeEvaluation& evaluation);
QList<AutoCountrySummary> buildAutoCountrySummaries(const QList<AutoNodeEvaluation>& evaluations);
QString bestAutoServerIdForCountry(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode,
    const QString& excludedServerId = {});

// Same selection, but ranked by a precomputed effective score (lower is better) instead of the raw
// latency the last probe happened to report.
//
// The caller builds the score map from RoutingScorePolicy state, so the pick already reflects
// EWMA smoothing (one slow probe does not reorder the list) and the circuit breaker (a node that
// is currently failing sorts last even when its last good latency was excellent). A node missing
// from `scores` is skipped: that is how the caller excludes nodes it has no evidence for.
QString bestAutoServerIdForCountryByScore(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode,
    const QHash<QString, double>& effectiveScores,
    const QString& excludedServerId = {});

int countAvailableAutoServersForCountry(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode);
