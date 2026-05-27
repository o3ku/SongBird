#pragma once

#include <QList>
#include <QString>

#include "domain/models/RoutingRule.h"

struct RoutingItem {
    QString id;
    QString remarks;
    QString url;
    QList<RoutingRule> rules;
    bool enabled = true;
    bool locked = false;
    bool builtin = false;
    QString customIcon;
    QString domainStrategy4Singbox;

    bool operator==(const RoutingItem& other) const = default;
};
