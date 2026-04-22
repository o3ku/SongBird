#pragma once

#include <QString>
#include <QStringList>

struct PolicyGroupItem {
    enum class Strategy {
        LeastPing = 0,
        LeastLoad = 1,
        Fallback = 2,
        Random = 3,
        RoundRobin = 4,
        UrlTest = 5
    };

    QString id;
    QString name;
    Strategy strategy = Strategy::LeastPing;
    QStringList memberServerIds;
    QString urlTestUrl;
    int toleranceMs = 0;
};
