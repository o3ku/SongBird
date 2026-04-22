#pragma once

#include <QList>
#include <QString>

struct ServerStatItem {
    QString itemId;
    quint64 totalUp = 0;
    quint64 totalDown = 0;
    quint64 todayUp = 0;
    quint64 todayDown = 0;
    qint64 dateNow = 0;
};

struct ServerStatistics {
    QList<ServerStatItem> server;
};
