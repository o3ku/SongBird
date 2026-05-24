#include "subscription/SubscriptionUrlImportService.h"

#include <utility>

#include <QUuid>

#include "subscription/SubscriptionImportTextParser.h"

SubscriptionUrlImportService::SubscriptionUrlImportService(
    IConfigRepository& repository,
    SubscriptionService& subscriptionService,
    UpdateByIdsCallback updateByIds)
    : repository_(repository)
    , subscriptionService_(subscriptionService)
    , updateByIds_(std::move(updateByIds))
{
}

SubscriptionUrlImportService::ImportPlan SubscriptionUrlImportService::prepareImport(
    const Config& config,
    const QString& text) const
{
    ImportPlan plan;
    plan.subscriptions = subscriptionService_.list(config);

    QStringList visitedUrls;
    for (const QString& line : SubscriptionImportTextParser::nonEmptyLines(text)) {
        if (!SubscriptionImportTextParser::isSubscriptionUrl(line)) {
            continue;
        }

        const QString normalizedUrl = line.trimmed();
        if (normalizedUrl.isEmpty()) {
            continue;
        }

        bool alreadyVisited = false;
        for (const QString& visitedUrl : visitedUrls) {
            if (visitedUrl.compare(normalizedUrl, Qt::CaseInsensitive) == 0) {
                alreadyVisited = true;
                break;
            }
        }
        if (alreadyVisited) {
            continue;
        }
        visitedUrls.append(normalizedUrl);

        QString subscriptionId;
        for (SubItem& item : plan.subscriptions) {
            if (item.url.trimmed().compare(normalizedUrl, Qt::CaseInsensitive) != 0) {
                continue;
            }

            if (item.id.trimmed().isEmpty()) {
                item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            }
            if (item.remarks.trimmed().isEmpty()) {
                item.remarks = SubscriptionImportTextParser::extractHostOrIpFromUrl(normalizedUrl);
            }
            item.enabled = true;
            subscriptionId = item.id.trimmed();
            break;
        }

        if (subscriptionId.isEmpty()) {
            SubItem item;
            item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            item.remarks = SubscriptionImportTextParser::extractHostOrIpFromUrl(normalizedUrl);
            if (item.remarks.trimmed().isEmpty()) {
                item.remarks = QStringLiteral("import sub");
            }
            item.url = normalizedUrl;
            item.enabled = true;
            plan.subscriptions.append(item);
            subscriptionId = item.id;
        }

        if (!subscriptionId.isEmpty() && !plan.subscriptionIds.contains(subscriptionId)) {
            plan.subscriptionIds.append(subscriptionId);
            plan.lastSubscriptionId = subscriptionId;
        }
    }

    return plan;
}

OperationResult SubscriptionUrlImportService::importAndUpdate(
    Config& config,
    const QString& text,
    ImportPlan* appliedPlan)
{
    const ImportPlan plan = prepareImport(config, text);
    if (appliedPlan != nullptr) {
        *appliedPlan = plan;
    }

    if (plan.subscriptionIds.isEmpty()) {
        return OperationResult::fail(QStringLiteral("No supported share URL or subscription payload was detected."));
    }

    const OperationResult saveResult = subscriptionService_.saveSubscriptions(config, plan.subscriptions);
    if (!saveResult.success) {
        return saveResult;
    }

    config.ui().mainSelectedSubId = plan.lastSubscriptionId;
    repository_.save(config);
    const OperationResult updateResult = updateByIds_
        ? updateByIds_(config, plan.subscriptionIds)
        : OperationResult::fail(QStringLiteral("Subscription update service is unavailable."));

    QStringList lines;
    lines.append(QStringLiteral("Imported %1 subscription URL(s).").arg(plan.subscriptionIds.size()));
    if (!updateResult.message.trimmed().isEmpty()) {
        lines.append(updateResult.message);
    }

    return updateResult.success
        ? OperationResult::ok(lines.join(QStringLiteral("\n")))
        : OperationResult::fail(lines.join(QStringLiteral("\n")));
}
