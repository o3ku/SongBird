#pragma once

#include <QRegularExpression>
#include <QString>

namespace UrlProbeLatency {

// Parses speed-test result text such as "12 ms" or "Accessible 34 ms" into a
// latency value in milliseconds. Shared by speed-test services and UI code so
// both agree on the accepted formats.
inline bool tryParseLatencyMs(const QString& value, double& latencyMs)
{
    static const QRegularExpression expression(
        QStringLiteral("^(?:accessible\\s+)?([+-]?\\d+(?:\\.\\d+)?)\\s*ms$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(value.trimmed());
    if (!match.hasMatch()) {
        return false;
    }

    bool ok = false;
    const double parsed = match.captured(1).toDouble(&ok);
    if (!ok) {
        return false;
    }

    latencyMs = parsed;
    return true;
}

} // namespace UrlProbeLatency
