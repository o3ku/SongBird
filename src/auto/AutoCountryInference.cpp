#include "auto/AutoCountryInference.h"

#include "appcore/CountryNameInference.h"
#include "auto/AutoCountrySupport.h"

QString inferAutoCountryCodeFromNodeName(const QString& name)
{
    // A raw ISO code in, Auto's fixed list out: an inferred country SongBirdAuto has no item for
    // (FR, CA, ...) is UNKNOWN here, exactly as before the shared move.
    return normalizeAutoCountryCode(CountryNameInference::inferCountryCodeFromNodeName(name));
}

AutoNodeEvaluation inferredAutoEvaluationFromItem(const SpeedTestRequestItem& item)
{
    AutoNodeEvaluation evaluation;
    evaluation.indexId = item.indexId;
    evaluation.displayName = item.displayName;
    evaluation.countryCode = inferAutoCountryCodeFromNodeName(item.displayName);
    evaluation.inferredCountryCode = evaluation.countryCode;
    evaluation.countryName = evaluation.countryCode == autoUnknownCountryCode() ? QStringLiteral("Unknown") : QString();
    evaluation.countryDisplay = evaluation.countryCode == autoUnknownCountryCode()
        ? QStringLiteral("Unknown")
        : autoCountryDisplayName(evaluation.countryCode);
    evaluation.available = true;
    evaluation.error = QStringLiteral("Pending test");
    return evaluation;
}

QList<AutoNodeEvaluation> inferredAutoEvaluationsFromItems(const QList<SpeedTestRequestItem>& items)
{
    QList<AutoNodeEvaluation> evaluations;
    evaluations.reserve(items.size());
    for (const SpeedTestRequestItem& item : items) {
        evaluations.append(inferredAutoEvaluationFromItem(item));
    }
    return evaluations;
}
