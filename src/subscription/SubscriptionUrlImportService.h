#pragma once

#include <functional>

#include <QList>
#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/SubItem.h"
#include "persistence/IConfigRepository.h"
#include "services/SubscriptionService.h"

class SubscriptionUrlImportService {
public:
    using UpdateByIdsCallback = std::function<OperationResult(Config&, const QStringList&)>;

    struct ImportPlan {
        QList<SubItem> subscriptions;
        QStringList subscriptionIds;
        QString lastSubscriptionId;
    };

    SubscriptionUrlImportService(
        IConfigRepository& repository,
        SubscriptionService& subscriptionService,
        UpdateByIdsCallback updateByIds);

    ImportPlan prepareImport(const Config& config, const QString& text) const;
    OperationResult importAndUpdate(Config& config, const QString& text, ImportPlan* appliedPlan = nullptr);

private:
    IConfigRepository& repository_;
    SubscriptionService& subscriptionService_;
    UpdateByIdsCallback updateByIds_;
};
