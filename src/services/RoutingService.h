#pragma once

#include <QList>
#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"
#include "persistence/IConfigRepository.h"

class RoutingService {
public:
    explicit RoutingService(IConfigRepository& repository);

    OperationResult saveRouting(
        Config& config,
        QList<RoutingItem> items,
        bool enableAdvanced,
        int selectedIndex,
        const QString& domainStrategy,
        const QString& domainMatcher);
    OperationResult setRoutingMode(Config& config, const QString& routingModeId);
    OperationResult selectRouting(Config& config, const QString& routingModeId);

private:
    static void normalizeRoutingItems(QList<RoutingItem>& items);
    static void normalizeRoutingItem(RoutingItem& item);
    static void normalizeRoutingRule(RoutingRule& rule);
    static QStringList normalizeValues(const QStringList& values);
    static bool isMeaningfulRule(const RoutingRule& rule);
    static int resolveSelectedIndex(int selectedIndex, int itemCount);

    IConfigRepository& repository_;
};
