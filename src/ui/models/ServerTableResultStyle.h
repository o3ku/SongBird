#pragma once

#include <QString>

#include "common/UrlProbeLatency.h"

// Maps a server table test-result string onto a colour band so the "Result"
// column can show latency at a glance instead of as bare text.
//
// The classification is deliberately pure and I/O free: it parses the same
// "<n> ms" form the speed-test services emit (see UrlProbeLatency) and falls
// back to a failure band for anything else, so the table stays honest about a
// result it cannot interpret ("Timeout", "Blocked", a raw error string).
namespace ServerTableResultStyle {

enum class Severity {
    None,
    Fast,
    Medium,
    Slow,
    Failure
};

// Band edges in milliseconds. A latency below kFastLatencyMs is green, below
// kSlowLatencyMs is yellow, and anything slower is red.
inline constexpr double kFastLatencyMs = 300.0;
inline constexpr double kSlowLatencyMs = 800.0;

inline Severity classify(const QString& testResult)
{
    const QString trimmed = testResult.trimmed();
    if (trimmed.isEmpty()) {
        return Severity::None;
    }

    double latencyMs = 0.0;
    if (!UrlProbeLatency::tryParseLatencyMs(trimmed, latencyMs)) {
        return Severity::Failure;
    }

    if (latencyMs < kFastLatencyMs) {
        return Severity::Fast;
    }
    if (latencyMs < kSlowLatencyMs) {
        return Severity::Medium;
    }

    return Severity::Slow;
}

} // namespace ServerTableResultStyle
