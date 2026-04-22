#include "services/GrpcStatisticsParser.h"

#include <QStringList>

QString GrpcStatisticsParser::defaultOutboundTag()
{
    return QStringLiteral("proxy");
}

bool GrpcStatisticsParser::parseCounterName(const QString& name, QString* outboundTag, QString* direction)
{
    const QStringList tokens = name.trimmed().split(QStringLiteral(">>>"), Qt::SkipEmptyParts);
    if (tokens.size() < 4) {
        return false;
    }

    if (outboundTag != nullptr) {
        *outboundTag = tokens.at(1).trimmed();
    }
    if (direction != nullptr) {
        *direction = tokens.at(3).trimmed();
    }
    return true;
}

GrpcTrafficDelta GrpcStatisticsParser::parseCounters(
    const QList<StatisticsCounterEntry>& counters,
    const QString& expectedOutboundTag)
{
    GrpcTrafficDelta result;
    const QString expectedTag = expectedOutboundTag.trimmed().isEmpty()
        ? defaultOutboundTag()
        : expectedOutboundTag.trimmed();

    for (const StatisticsCounterEntry& counter : counters) {
        QString outboundTag;
        QString direction;
        if (!parseCounterName(counter.name, &outboundTag, &direction)) {
            continue;
        }

        if (outboundTag.compare(expectedTag, Qt::CaseInsensitive) != 0) {
            continue;
        }

        result.matched = true;
        const quint64 value = counter.value > 0
            ? static_cast<quint64>(counter.value)
            : 0;
        if (direction.compare(QStringLiteral("uplink"), Qt::CaseInsensitive) == 0) {
            result.up += value;
        } else if (direction.compare(QStringLiteral("downlink"), Qt::CaseInsensitive) == 0) {
            result.down += value;
        }
    }

    return result;
}
