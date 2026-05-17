#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "domain/models/TunModeItem.h"
#include "persistence/IConfigRepository.h"

class JsonConfigRepository final : public IConfigRepository {
public:
    explicit JsonConfigRepository(QString configPath);

    Config load() override;
    bool save(const Config& config) override;

    QString configPath() const;
    QString lastLoadError() const;

private:
    static Config parseConfig(const QJsonObject& root);
    static void applyConfig(QJsonObject& root, const Config& config);
    static int normalizeSplitPercent(int value, int fallback);

    static QList<VmessItem> parseServers(const QJsonArray& array);
    static QList<SubItem> parseSubscriptions(const QJsonArray& array);
    static QList<GlobalHotkeyItem> parseGlobalHotkeys(const QJsonArray& array);
    static QList<RoutingItem> parseRoutingItems(const QJsonArray& array);
    static QList<RoutingRule> parseRoutingRules(const QJsonArray& array);

    static QJsonArray toServerArray(const QList<VmessItem>& items);
    static QJsonArray toSubscriptionArray(const QList<SubItem>& items);
    static QJsonArray toGlobalHotkeyArray(const QList<GlobalHotkeyItem>& items);
    static QJsonArray toRoutingArray(const QList<RoutingItem>& items);
    static QJsonArray toRoutingRuleArray(const QList<RoutingRule>& items);
    static TunModeItem parseTunModeItem(const QJsonObject& object);
    static QJsonObject toTunModeItem(const TunModeItem& item);
    static QList<PolicyGroupItem> parsePolicyGroups(const QJsonArray& array);
    static QJsonArray toPolicyGroupArray(const QList<PolicyGroupItem>& items);
    static QList<CoreTypeItem> parseCoreTypeItems(const QJsonArray& array);
    static QJsonArray toCoreTypeItemArray(const QList<CoreTypeItem>& items);

    static ConfigType parseConfigType(const QJsonValue& value);
    static CoreType parseCoreType(const QJsonValue& value);
    static int toLegacyConfigTypeValue(ConfigType type);
    static QJsonValue toLegacyCoreTypeValue(CoreType type);
    QJsonObject loadExistingRootObject() const;

    QString configPath_;
    QString lastLoadError_;
};
