#pragma once

#include <QList>
#include <QString>

#include "domain/models/RoutingRule.h"

struct RoutingItem {
    QString remarks;
    QString url;
    QList<RoutingRule> rules;
    bool enabled = true;
    bool locked = false;
    QString customIcon;
    QString domainStrategy4Singbox;
};
