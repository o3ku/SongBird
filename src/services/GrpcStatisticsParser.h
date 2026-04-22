#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

struct StatisticsCounterEntry {
    QString name;
    qint64 value = 0;
};

struct GrpcTrafficDelta {
    quint64 up = 0;
    quint64 down = 0;
    bool matched = false;
};

class GrpcStatisticsParser final {
public:
    static QString defaultOutboundTag();
    static bool parseCounterName(const QString& name, QString* outboundTag, QString* direction);
    static GrpcTrafficDelta parseCounters(
        const QList<StatisticsCounterEntry>& counters,
        const QString& expectedOutboundTag = defaultOutboundTag());
};
