#pragma once

#include <QString>
#include <QStringList>

struct PolicyGroupItem {
    enum class Strategy {
        LeastPing = 0,
        UrlTest = 5
    };

    QString id;
    QString name;
    Strategy strategy = Strategy::LeastPing;
    QStringList memberServerIds;
    QString urlTestUrl;
    int toleranceMs = 0;
};
