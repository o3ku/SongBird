#include "auto/AutoCoordinatorLogic.h"

#include <QCoreApplication>

#include <algorithm>

#include "services/SubscriptionService.h"

namespace AutoCoordinatorLogic {

const QString kAutoStrategyFirstAvailable = QStringLiteral("firstAvailable");
const QString kAutoStrategyLowestLatency = QStringLiteral("lowestLatency");

QString normalizeSubscriptionUrl(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QChar('#'))) {
        return {};
    }
    return value;
}

QString normalizeAutoSelectionStrategy(QString value)
{
    value = value.trimmed();
    return value.compare(kAutoStrategyFirstAvailable, Qt::CaseInsensitive) == 0
        ? kAutoStrategyFirstAvailable
        : kAutoStrategyLowestLatency;
}

QString evaluationStateText(const AutoNodeEvaluation& evaluation)
{
    if (evaluation.available) {
        return QStringLiteral("%1 %2 ms").arg(evaluation.countryDisplay).arg(evaluation.latencyMs);
    }
    return evaluation.error.trimmed().isEmpty() ? QCoreApplication::translate("SongBirdAuto", "Failed") : evaluation.error.trimmed();
}

QString firstCountryWithNodesOrFirst(const QList<AutoCountrySummary>& countries)
{
    const auto withNodes = std::find_if(countries.cbegin(), countries.cend(), [](const AutoCountrySummary& country) {
        return country.availableCount > 0;
    });
    return withNodes == countries.cend()
        ? (countries.isEmpty() ? QString() : countries.constFirst().countryCode)
        : withNodes->countryCode;
}

const VmessItem* findServerInConfig(const Config& config, const QString& indexId)
{
    if (indexId.trimmed().isEmpty()) {
        return nullptr;
    }
    for (const VmessItem& server : config.collection().servers) {
        if (server.indexId == indexId) {
            return &server;
        }
    }
    return nullptr;
}

void preserveActiveServerForSubscriptionUpdate(Config& config, const QString& activeServerId)
{
    for (VmessItem& server : config.collection().servers) {
        if (server.indexId != activeServerId || server.subId.trimmed().isEmpty()) {
            continue;
        }
        server.subId.clear();
        if (!server.remarks.trimmed().startsWith(QStringLiteral("Active copy |"))) {
            server.remarks = QStringLiteral("Active copy | %1")
                .arg(server.remarks.trimmed().isEmpty() ? server.address.trimmed() : server.remarks.trimmed());
        }
        return;
    }
}

bool reconcilePreservedActiveServer(Config& config, const QString& activeServerId)
{
    const VmessItem* activeServer = findServerInConfig(config, activeServerId);
    if (activeServer == nullptr || !activeServer->subId.trimmed().isEmpty()) {
        return false;
    }

    const QString activeReuseKey = SubscriptionService::serverReuseKey(*activeServer);
    bool hasDuplicateSubscriptionServer = false;
    for (VmessItem& server : config.collection().servers) {
        if (server.subId.trimmed().isEmpty()) {
            continue;
        }
        if (SubscriptionService::serverReuseKey(server) == activeReuseKey) {
            server.indexId = activeServerId;
            if (server.testResult.trimmed().isEmpty()) {
                server.testResult = activeServer->testResult;
            }
            hasDuplicateSubscriptionServer = true;
            break;
        }
    }

    if (hasDuplicateSubscriptionServer) {
        config.collection().servers.erase(
            std::remove_if(
                config.collection().servers.begin(),
                config.collection().servers.end(),
                [&activeServerId](const VmessItem& server) {
                    return server.indexId == activeServerId && server.subId.trimmed().isEmpty();
                }),
            config.collection().servers.end());
        config.currentIndexId = activeServerId;
        return true;
    }

    if (config.currentIndexId == activeServerId) {
        return false;
    }
    config.currentIndexId = activeServerId;
    return true;
}

} // namespace AutoCoordinatorLogic
