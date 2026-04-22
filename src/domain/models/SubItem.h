#pragma once

#include <QString>

struct SubItem {
    QString id;
    QString remarks;
    QString url;
    bool enabled = true;
    QString userAgent;
};
