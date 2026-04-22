#pragma once

#include <QString>
#include <QStringList>

struct RoutingRule {
    QString type;
    QString port;
    QString network;
    QStringList inboundTag;
    QString outboundTag;
    QStringList ip;
    QStringList domain;
    QStringList protocol;
    QStringList process;
    bool enabled = true;
};
