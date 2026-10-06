#pragma once

#include <QString>
#include <QStringList>

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
    static constexpr int LocationProbePortOffset = 103;
    static constexpr int LocationProbeTimeoutMs = 5000;
    static constexpr int LocationProbeRetryDelayMs = 300;
    static constexpr int LocationProbeTotalTimeoutMs = 12000;
    static constexpr int LocationProbeMaxRounds = 8;

    static int resolveHttpPort(const Config& config, bool usesDedicatedProbe);

    OutboundLocationProbeResult probe(int httpPort) const;
    OutboundLocationDetails probeStructured(int httpPort) const;
    // The same probe with a shortened budget. The production constants add up to twelve seconds
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
