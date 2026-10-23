#pragma once

#include <QString>
#include <QStringList>

#include "common/LocalInboundPorts.h"
#include "domain/models/Config.h"

struct OutboundLocationProbeResult
{
    QString location;
    QString error;
};

struct OutboundLocationDetails
{
    QString location;
    QString countryCode;
    QString countryName;
    QString city;
    QString ip;
    QString error;
};

// Declared ahead of the service so probeStructured() can take a reference to one, and defined
// after it so the defaults can name the constants they stand in for.
struct LocationProbeTimings;

class OutboundLocationProbeService
{
public:
    // Kept as the named constant callers already pass around; the value itself lives with the rest
    // of the local inbound policy so the runtime config and this probe cannot drift apart.
    static constexpr int LocationProbePortOffset = LocalInboundPorts::kLocationProbeOffset;
    static constexpr int LocationProbeTimeoutMs = 5000;
    static constexpr int LocationProbeRetryDelayMs = 300;
    // This probe is display-only now -- it runs after the system proxy is already applied and a
    // failure only costs the user the flag next to the node name. It used to be the startup gate,
    // which is why it retried eight times; against the same handful of rate-limiting geo services
    // that was up to forty requests per launch for a decoration. Two rounds is enough to ride out a
    // single transient failure, and the total budget is cut to match.
    static constexpr int LocationProbeTotalTimeoutMs = 8000;
    static constexpr int LocationProbeMaxRounds = 2;

    static int resolveHttpPort(const Config& config, bool usesDedicatedProbe);

    OutboundLocationProbeResult probe(int httpPort) const;
    OutboundLocationDetails probeStructured(int httpPort) const;
    // The same probe with a shortened budget. The production constants add up to eight seconds
    // of waiting before the timeout branch is reached, so it cannot be asserted without a seam.
    OutboundLocationDetails probeStructured(int httpPort, const LocationProbeTimings& timings) const;

private:
    static QStringList probeUrls();
    static OutboundLocationDetails probeOnce(const QStringList& probeUrls, int httpPort, int timeoutMs);
};

struct LocationProbeTimings
{
    int perRequestTimeoutMs = OutboundLocationProbeService::LocationProbeTimeoutMs;
    int retryDelayMs = OutboundLocationProbeService::LocationProbeRetryDelayMs;
    int totalTimeoutMs = OutboundLocationProbeService::LocationProbeTotalTimeoutMs;
    int maxRounds = OutboundLocationProbeService::LocationProbeMaxRounds;
};
