#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

struct AutoNodeEvaluation
{
    QString indexId;
    QString displayName;
    QString countryCode;
    QString inferredCountryCode;
    QString countryName;
    QString countryDisplay;
    QString locationSummary;
    qint64 latencyMs = -1;
    bool available = false;
    // True when the probe never reached the node: no measurement was taken, so `available` says
    // nothing about the node. A busy temporary port is the common cause. Kept separate because the
    // scoring policy treats a failure as evidence, and "we could not test it" is not evidence.
    bool inconclusive = false;
    bool tested = false;
    QString error;
    QDateTime checkedAt;
};

struct AutoCountrySummary
{
    QString countryCode;
    QString displayName;
    int availableCount = 0;
    qint64 bestLatencyMs = -1;
    QString bestServerId;
};

Q_DECLARE_METATYPE(AutoNodeEvaluation)
Q_DECLARE_METATYPE(QList<AutoNodeEvaluation>)
Q_DECLARE_METATYPE(AutoCountrySummary)
Q_DECLARE_METATYPE(QList<AutoCountrySummary>)
