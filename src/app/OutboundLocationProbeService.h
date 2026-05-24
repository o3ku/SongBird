#pragma once

#include <QString>
#include <QStringList>

#include "domain/models/Config.h"

struct OutboundLocationProbeResult
{
    QString location;
    QString error;
};

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

private:
    static QStringList probeUrls();
    static OutboundLocationProbeResult probeOnce(const QStringList& probeUrls, int httpPort, int timeoutMs);
};
