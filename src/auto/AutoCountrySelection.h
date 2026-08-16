#pragma once

#include <QList>
#include <QString>

#include "auto/AutoTypes.h"

QString autoCountryKeyForEvaluation(const AutoNodeEvaluation& evaluation);
QList<AutoCountrySummary> buildAutoCountrySummaries(const QList<AutoNodeEvaluation>& evaluations);
QString bestAutoServerIdForCountry(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode,
    const QString& excludedServerId = {});
int countAvailableAutoServersForCountry(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode);
