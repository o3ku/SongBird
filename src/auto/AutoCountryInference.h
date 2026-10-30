#pragma once

#include <QList>
#include <QString>

#include "auto/AutoTypes.h"
#include "services/SpeedTestRequestItem.h"

// Thin auto/-front-end shim over the shared inference. The pure name->ISO-code walk now lives in
// appcore/CountryNameInference (both front ends need the same answer, and a front end must not
// include another front end); what stays here is the mapping into Auto's fixed country list
// (normalizeAutoCountryCode) and the SpeedTestRequestItem plumbing, neither of which the main
// window has any use for.
QString inferAutoCountryCodeFromNodeName(const QString& name);
AutoNodeEvaluation inferredAutoEvaluationFromItem(const SpeedTestRequestItem& item);
QList<AutoNodeEvaluation> inferredAutoEvaluationsFromItems(const QList<SpeedTestRequestItem>& items);
