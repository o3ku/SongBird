#pragma once

#include <atomic>
#include <functional>

#include <QList>
#include <QString>

#include "auto/AutoTypes.h"
#include "domain/models/Config.h"
#include "services/SpeedTestRequestItem.h"

class AutoNodeEvaluationService
{
public:
    struct Request {
        Config config;
        QList<SpeedTestRequestItem> items;
        QString customConfigDirectory;
        QString urlTestUrl;
        int maxConcurrency = 8;
    };

    using LogCallback = std::function<void(const QString&)>;
    using ResultCallback = std::function<void(const AutoNodeEvaluation&)>;

    static QList<AutoNodeEvaluation> evaluate(
        const Request& request,
        const std::atomic_bool& cancelled,
        const LogCallback& log = {},
        const ResultCallback& resultReady = {});
};
