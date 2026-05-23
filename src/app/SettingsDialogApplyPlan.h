#pragma once

#include <QString>
#include <QStringList>

#include "app/TunSettingsApplyDecision.h"
#include "domain/models/Config.h"
#include "domain/models/SubItem.h"

struct SettingsDialogApplyPlan {
    TunSettingsSaveBehavior tunSaveBehavior;
    bool dirty = false;
    bool autoRunChanged = false;
    bool systemProxySettingsChanged = false;
    bool languageChanged = false;
    bool themeChanged = false;
    bool runtimeSettingsChanged = false;
};

inline QString normalizeSettingsLanguageCode(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QChar('-'), QChar('_'));
    return value;
}

inline QString normalizeSettingsThemeName(QString value)
{
    value = value.trimmed();
    if (value.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Dark");
    }
    return QStringLiteral("Light");
}

inline QStringList normalizedSettingsStringList(QStringList values)
{
    for (QString& value : values) {
        value = value.trimmed();
    }
    values.removeAll(QString());
    return values;
}

inline bool areSettingsSubItemsEqual(const SubItem& lhs, const SubItem& rhs)
{
    return lhs.id.trimmed() == rhs.id.trimmed()
        && lhs.remarks.trimmed() == rhs.remarks.trimmed()
        && lhs.url.trimmed() == rhs.url.trimmed()
        && lhs.enabled == rhs.enabled
        && lhs.userAgent.trimmed() == rhs.userAgent.trimmed();
}

inline bool areSettingsSubscriptionsEqual(const QList<SubItem>& lhs, const QList<SubItem>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (int index = 0; index < lhs.size(); ++index) {
        if (!areSettingsSubItemsEqual(lhs.at(index), rhs.at(index))) {
            return false;
        }
    }
    return true;
}

inline bool areSettingsDnsFieldsEqual(const DnsConfigState& lhs, const DnsConfigState& rhs)
{
    return lhs.enableFragment == rhs.enableFragment
        && lhs.enableCacheFile4Sbox == rhs.enableCacheFile4Sbox
        && lhs.defaultFingerprint.trimmed() == rhs.defaultFingerprint.trimmed()
        && lhs.defaultUserAgent.trimmed() == rhs.defaultUserAgent.trimmed()
        && lhs.directDns.trimmed() == rhs.directDns.trimmed()
        && lhs.remoteDns.trimmed() == rhs.remoteDns.trimmed()
        && lhs.bootstrapDns.trimmed() == rhs.bootstrapDns.trimmed()
        && lhs.fakeIp == rhs.fakeIp
        && lhs.globalFakeIp == rhs.globalFakeIp
        && lhs.serveStale == rhs.serveStale
        && lhs.parallelQuery == rhs.parallelQuery
        && lhs.directExpectedIps.trimmed() == rhs.directExpectedIps.trimmed()
        && lhs.useSystemHosts == rhs.useSystemHosts
        && lhs.addCommonHosts == rhs.addCommonHosts
        && lhs.blockBindingQuery == rhs.blockBindingQuery
        && lhs.domainStrategyForFreedom.trimmed() == rhs.domainStrategyForFreedom.trimmed()
        && lhs.domainStrategyForProxy.trimmed() == rhs.domainStrategyForProxy.trimmed()
        && lhs.dnsHosts.trimmed() == rhs.dnsHosts.trimmed()
        && lhs.defaultAllowInsecure == rhs.defaultAllowInsecure;
}

inline bool areSettingsRoutingRulesEqual(const RoutingRule& lhs, const RoutingRule& rhs)
{
    return lhs.type.trimmed() == rhs.type.trimmed()
        && lhs.enabled == rhs.enabled
        && lhs.outboundTag.trimmed() == rhs.outboundTag.trimmed()
        && normalizedSettingsStringList(lhs.domain) == normalizedSettingsStringList(rhs.domain)
        && normalizedSettingsStringList(lhs.ip) == normalizedSettingsStringList(rhs.ip)
        && normalizedSettingsStringList(lhs.protocol) == normalizedSettingsStringList(rhs.protocol)
        && normalizedSettingsStringList(lhs.process) == normalizedSettingsStringList(rhs.process)
        && lhs.port.trimmed() == rhs.port.trimmed()
        && lhs.network.trimmed() == rhs.network.trimmed()
        && normalizedSettingsStringList(lhs.inboundTag) == normalizedSettingsStringList(rhs.inboundTag);
}

inline bool areSettingsRoutingRuleListsEqual(const QList<RoutingRule>& lhs, const QList<RoutingRule>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (int index = 0; index < lhs.size(); ++index) {
        if (!areSettingsRoutingRulesEqual(lhs.at(index), rhs.at(index))) {
            return false;
        }
    }
    return true;
}

inline bool areSettingsRoutingItemsEqual(const RoutingItem& lhs, const RoutingItem& rhs)
{
    return lhs.remarks.trimmed() == rhs.remarks.trimmed()
        && lhs.url.trimmed() == rhs.url.trimmed()
        && lhs.enabled == rhs.enabled
        && lhs.locked == rhs.locked
        && lhs.customIcon.trimmed() == rhs.customIcon.trimmed()
        && lhs.domainStrategy4Singbox.trimmed() == rhs.domainStrategy4Singbox.trimmed()
        && areSettingsRoutingRuleListsEqual(lhs.rules, rhs.rules);
}

inline bool areSettingsRoutingItemListsEqual(const QList<RoutingItem>& lhs, const QList<RoutingItem>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (int index = 0; index < lhs.size(); ++index) {
        if (!areSettingsRoutingItemsEqual(lhs.at(index), rhs.at(index))) {
            return false;
        }
    }
    return true;
}

inline bool areSettingsGeneralFieldsEqual(const Config& previousConfig, const Config& updatedConfig)
{
    const int previousLocalPort = previousConfig.localPort > 0 ? previousConfig.localPort : 10808;
    const int updatedLocalPort = updatedConfig.localPort > 0 ? updatedConfig.localPort : 10808;
    return previousConfig.ui().showMainOnStartup == updatedConfig.ui().showMainOnStartup
        && previousConfig.ui().autoRunEnabled == updatedConfig.ui().autoRunEnabled
        && previousConfig.allowLanConnection == updatedConfig.allowLanConnection
        && previousConfig.sniffingEnabled == updatedConfig.sniffingEnabled
        && previousConfig.routeOnly == updatedConfig.routeOnly
        && previousLocalPort == updatedLocalPort
        && previousConfig.dns().defaultAllowInsecure == updatedConfig.dns().defaultAllowInsecure
        && previousConfig.dns().defaultFingerprint.trimmed() == updatedConfig.dns().defaultFingerprint.trimmed()
        && normalizeSettingsLanguageCode(previousConfig.ui().languageCode)
            == normalizeSettingsLanguageCode(updatedConfig.ui().languageCode)
        && normalizeSettingsThemeName(previousConfig.ui().themeName)
            == normalizeSettingsThemeName(updatedConfig.ui().themeName);
}

inline bool areSettingsCoreFieldsEqual(const Config& previousConfig, const Config& updatedConfig)
{
    return previousConfig.dns().enableCacheFile4Sbox == updatedConfig.dns().enableCacheFile4Sbox
        && previousConfig.mux4SboxProtocol.trimmed() == updatedConfig.mux4SboxProtocol.trimmed()
        && previousConfig.dns().enableFragment == updatedConfig.dns().enableFragment
        && previousConfig.dns().defaultUserAgent.trimmed() == updatedConfig.dns().defaultUserAgent.trimmed()
        && normalizedCoreTypeItemsForComparison(previousConfig)
            == normalizedCoreTypeItemsForComparison(updatedConfig)
        && previousConfig.tun().tunModeItem.enableLegacyProtect
            == updatedConfig.tun().tunModeItem.enableLegacyProtect;
}

inline bool areSettingsTunFieldsEqual(const Config& previousConfig, const Config& updatedConfig)
{
    return areTunModeItemsEqual(previousConfig.tun().tunModeItem, updatedConfig.tun().tunModeItem);
}

inline bool areSettingsRoutingFieldsEqual(const Config& previousConfig, const Config& updatedConfig)
{
    return previousConfig.collection().enableRoutingAdvanced == updatedConfig.collection().enableRoutingAdvanced
        && previousConfig.collection().routingIndex == updatedConfig.collection().routingIndex
        && areSettingsRoutingItemListsEqual(
            previousConfig.collection().routingItems,
            updatedConfig.collection().routingItems)
        && areSettingsRoutingRuleListsEqual(
            previousConfig.collection().routingCustomRules,
            updatedConfig.collection().routingCustomRules)
        && previousConfig.ui().settingsRoutingRuleTabKey.trimmed()
            == updatedConfig.ui().settingsRoutingRuleTabKey.trimmed();
}

inline bool areSettingsOwnedFieldsEqual(const Config& previousConfig, const Config& updatedConfig)
{
    return areSettingsGeneralFieldsEqual(previousConfig, updatedConfig)
        && areSettingsCoreFieldsEqual(previousConfig, updatedConfig)
        && areSettingsTunFieldsEqual(previousConfig, updatedConfig)
        && areSettingsDnsFieldsEqual(previousConfig.dns(), updatedConfig.dns())
        && areSettingsRoutingFieldsEqual(previousConfig, updatedConfig)
        && areSettingsSubscriptionsEqual(
            previousConfig.collection().subscriptions,
            updatedConfig.collection().subscriptions);
}

inline SettingsDialogApplyPlan evaluateSettingsDialogApplyPlan(
    const Config& previousConfig,
    const Config& updatedConfig,
    bool isWindows,
    bool isProcessElevated,
    bool isCoreRunning)
{
    SettingsDialogApplyPlan plan;
    plan.dirty = !areSettingsOwnedFieldsEqual(previousConfig, updatedConfig);
    plan.tunSaveBehavior = evaluateTunSettingsSaveBehavior(
        previousConfig,
        updatedConfig,
        isWindows,
        isProcessElevated,
        isCoreRunning);
    plan.runtimeSettingsChanged = shouldHotApplyRuntimeSettings(
        previousConfig,
        updatedConfig,
        plan.tunSaveBehavior.applyDecision);
    plan.autoRunChanged = previousConfig.ui().autoRunEnabled != updatedConfig.ui().autoRunEnabled;
    plan.systemProxySettingsChanged = previousConfig.localPort != updatedConfig.localPort
        || previousConfig.systemProxyAdvancedProtocol != updatedConfig.systemProxyAdvancedProtocol;
    plan.languageChanged =
        normalizeSettingsLanguageCode(previousConfig.ui().languageCode)
        != normalizeSettingsLanguageCode(updatedConfig.ui().languageCode);
    plan.themeChanged =
        normalizeSettingsThemeName(previousConfig.ui().themeName)
        != normalizeSettingsThemeName(updatedConfig.ui().themeName);
    return plan;
}
