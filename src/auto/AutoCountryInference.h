#pragma once

#include <QList>
#include <QString>

#include "auto/AutoTypes.h"
#include "services/SpeedTestRequestItem.h"

QString inferAutoCountryCodeFromNodeName(const QString& name);
AutoNodeEvaluation inferredAutoEvaluationFromItem(const SpeedTestRequestItem& item);
QList<AutoNodeEvaluation> inferredAutoEvaluationsFromItems(const QList<SpeedTestRequestItem>& items);
