#include <QtTest>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QSignalSpy>
#include <QLabel>
#include <QSpinBox>
#include <QStackedLayout>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>

#include "ui/dialogs/SettingsDialog.h"

class SettingsDialogTests : public QObject {
    Q_OBJECT

private slots:
    void downloadButtonStartsInlineUpdate_data();
    void downloadButtonStartsInlineUpdate();
    void coreUpdateProgressRefreshesInlineStatus();
    void successfulCoreUpdateFinishRestoresIdleStatus();
    void installedCoreVersionShowsUpdateAction();
    void sniffingEnabledCheckboxRoundTripsConfig();
    void sniffingToggleControlsRouteOnlyCheckbox();
    void routeOnlyCheckboxRoundTripsConfig();
    void enableFragmentCheckboxRoundTripsConfig();
    void enableCacheFile4SboxCheckboxRoundTripsConfig();
    void defaultAllowInsecureCheckboxRoundTripsConfig();
    void defaultUserAgentComboRoundTripsConfig();
    void defaultFingerprintComboRoundTripsConfig();
    void statisticsToggleControlsRefreshRateSpin();
    void proxySettingsRoundTripsConfig();
    void updateSettingsRoundTripsConfig();
    void tunSettingsRoundTripsConfig();
    void tunToggleControlsDependentFields();
    void tunToggleControlsCoreLegacyProtect();
    void mux4SboxProtocolComboRoundTripsConfig();
    void updateTabDoesNotExposeRemovedTls13Option();
    void coreTypeTableIncludesHttpProtocol();
    void coreTypeTableDefaultsUseConfiguredValue();
    void corePageUsesPlainContentWithoutGroupBoxes();
    void corePageShowsProtocolAndCoreSectionTitles();
    void routingRuleNetworkAndProcessRoundTripConfig();
    void routingCustomRuleTabsRoundTripConfig();
    void routingCustomRuleTabsDefaultToDirectAndPersistSelection();
    void routingPageUsesCompactCardsAndPlainCustomRuleForms();
    void routingBaseRouteCardsCollapseAroundSelectedCard();
    void routingPageShowsExplicitEmptyStateWithoutBaseRoutes();
    void settingsDialogUsesCompactUiFontBaseline();
    void fakeIpToggleControlsGlobalFakeIpCheckbox();
    void dnsPageRoundTripsFields();
    void subscriptionTableUsesServerTableStyle();
    void subscriptionPageRoundTripsSelectedRowsAndItems();
    void settingsDialogTabBarUsesImageReferenceStyleHooks();
    void settingsDialogSelectedTabReservesBoldTextWidth();
    void settingsDialogHasMinimumWidth720();
    void selectTabOpensRoutingPage();
};

void SettingsDialogTests::downloadButtonStartsInlineUpdate_data()
{
    QTest::addColumn<QString>("buttonObjectName");
    QTest::addColumn<int>("expectedCoreType");

    QTest::newRow("xray") << QStringLiteral("coreDownloadButton_%1").arg(static_cast<int>(CoreType::Xray)) << static_cast<int>(CoreType::Xray);
    QTest::newRow("sing-box") << QStringLiteral("coreDownloadButton_%1").arg(static_cast<int>(CoreType::SingBox)) << static_cast<int>(CoreType::SingBox);
}

void SettingsDialogTests::downloadButtonStartsInlineUpdate()
{
    QFETCH(QString, buttonObjectName);
    QFETCH(int, expectedCoreType);

    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.setCoreVersion(CoreType::Xray, QString());
    dialog.setCoreVersion(CoreType::SingBox, QString());

    auto* button = dialog.findChild<QPushButton*>(buttonObjectName);
    QVERIFY(button != nullptr);
    QSignalSpy spy(&dialog, SIGNAL(coreDownloadRequested(int)));
    QVERIFY(spy.isValid());

    button->click();

    QCOMPARE(spy.count(), 1);
    QVERIFY(dialog.wasCoreDownloadRequested());
    QCOMPARE(static_cast<int>(dialog.requestedCoreDownload()), expectedCoreType);
    QCOMPARE(dialog.result(), 0);
    QVERIFY(!button->isHidden());
    QVERIFY(button->isEnabled());
}

void SettingsDialogTests::coreUpdateProgressRefreshesInlineStatus()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.setCoreVersion(CoreType::Xray, QString());
    dialog.setCoreVersion(CoreType::SingBox, QString());

    auto* xrayButton = dialog.findChild<QPushButton*>(QStringLiteral("coreDownloadButton_%1").arg(static_cast<int>(CoreType::Xray)));
    auto* xrayLabel = dialog.findChild<QLabel*>(QStringLiteral("coreVersionLabel_%1").arg(static_cast<int>(CoreType::Xray)));
    auto* xrayStatusLabel = dialog.findChild<QLabel*>(QStringLiteral("coreStatusLabel_%1").arg(static_cast<int>(CoreType::Xray)));
    QVERIFY(xrayButton != nullptr);
    QVERIFY(xrayLabel != nullptr);
    QVERIFY(xrayStatusLabel != nullptr);

    dialog.beginCoreUpdate(CoreType::Xray);
    dialog.setCoreUpdateProgress(CoreType::Xray, QStringLiteral("Downloading Xray package..."));

    QVERIFY(!xrayButton->isVisible());
    QCOMPARE(xrayLabel->text(), QStringLiteral("Downloading..."));
    QVERIFY(!xrayStatusLabel->isVisible());
}

void SettingsDialogTests::successfulCoreUpdateFinishRestoresIdleStatus()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.setCoreVersion(CoreType::Xray, QString());

    auto* xrayButton = dialog.findChild<QPushButton*>(
        QStringLiteral("coreDownloadButton_%1").arg(static_cast<int>(CoreType::Xray)));
    auto* xrayLabel = dialog.findChild<QLabel*>(
        QStringLiteral("coreVersionLabel_%1").arg(static_cast<int>(CoreType::Xray)));
    auto* xrayStatusLabel = dialog.findChild<QLabel*>(
        QStringLiteral("coreStatusLabel_%1").arg(static_cast<int>(CoreType::Xray)));
    QVERIFY(xrayButton != nullptr);
    QVERIFY(xrayLabel != nullptr);
    QVERIFY(xrayStatusLabel != nullptr);

    dialog.beginCoreUpdate(CoreType::Xray);
    dialog.finishCoreUpdate(CoreType::Xray, true, QStringLiteral("Xray update was canceled."));

    QVERIFY(!xrayButton->isHidden());
    QVERIFY(xrayButton->isEnabled());
    QCOMPARE(xrayLabel->text(), QStringLiteral("Not found"));
    QVERIFY(xrayStatusLabel->isHidden());
    QVERIFY(xrayStatusLabel->text().isEmpty());
}

void SettingsDialogTests::installedCoreVersionShowsUpdateAction()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.setExistingCoreTypes({CoreType::SingBox});
    dialog.setCoreVersion(CoreType::SingBox, QStringLiteral("v1.11.0"));

    auto* singBoxButton = dialog.findChild<QPushButton*>(
        QStringLiteral("coreDownloadButton_%1").arg(static_cast<int>(CoreType::SingBox)));
    auto* singBoxLabel = dialog.findChild<QLabel*>(
        QStringLiteral("coreVersionLabel_%1").arg(static_cast<int>(CoreType::SingBox)));

    QVERIFY(singBoxButton != nullptr);
    QVERIFY(singBoxLabel != nullptr);
    QCOMPARE(singBoxLabel->text(), QStringLiteral("v1.11.0"));
    QCOMPARE(singBoxButton->text(), QStringLiteral("Update"));
}

void SettingsDialogTests::selectTabOpensRoutingPage()
{
    SettingsDialog dialog;
    dialog.selectTab(2);
    dialog.setConfig(Config());

    auto* tabBar = dialog.findChild<QTabBar*>(QStringLiteral("settingsTabBar"));
    auto* stackLayout = dialog.findChild<QStackedLayout*>(QStringLiteral("settingsStackLayout"));
    QVERIFY(tabBar != nullptr);
    QVERIFY(stackLayout != nullptr);

    QCOMPARE(tabBar->currentIndex(), 2);
    QCOMPARE(tabBar->tabText(2), QStringLiteral("Routing"));
    QCOMPARE(stackLayout->currentIndex(), 2);
}

void SettingsDialogTests::sniffingEnabledCheckboxRoundTripsConfig()
{
    Config config;
    config.sniffingEnabled = false;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* sniffingCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsSniffingEnabledCheck"));
    QVERIFY(sniffingCheck != nullptr);
    QVERIFY(!sniffingCheck->isChecked());

    sniffingCheck->setChecked(true);

    const Config updated = dialog.config();
    QVERIFY(updated.sniffingEnabled);
}

void SettingsDialogTests::sniffingToggleControlsRouteOnlyCheckbox()
{
    Config config;
    config.sniffingEnabled = false;
    config.routeOnly = true;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* sniffingCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsSniffingEnabledCheck"));
    auto* routeOnlyCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsRouteOnlyCheck"));
    QVERIFY(sniffingCheck != nullptr);
    QVERIFY(routeOnlyCheck != nullptr);

    QVERIFY(!sniffingCheck->isChecked());
    QVERIFY(!routeOnlyCheck->isEnabled());

    sniffingCheck->setChecked(true);
    QVERIFY(routeOnlyCheck->isEnabled());
}

void SettingsDialogTests::routeOnlyCheckboxRoundTripsConfig()
{
    Config config;
    config.routeOnly = true;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* routeOnlyCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsRouteOnlyCheck"));
    QVERIFY(routeOnlyCheck != nullptr);
    QVERIFY(routeOnlyCheck->isChecked());

    routeOnlyCheck->setChecked(false);

    const Config updated = dialog.config();
    QVERIFY(!updated.routeOnly);
}

void SettingsDialogTests::enableFragmentCheckboxRoundTripsConfig()
{
    Config config;
    config.enableFragment = true;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* enableFragmentCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsEnableFragmentCheck"));
    QVERIFY(enableFragmentCheck != nullptr);
    QVERIFY(enableFragmentCheck->isChecked());

    enableFragmentCheck->setChecked(false);

    const Config updated = dialog.config();
    QVERIFY(!updated.enableFragment);
}

void SettingsDialogTests::enableCacheFile4SboxCheckboxRoundTripsConfig()
{
    Config config;
    config.enableCacheFile4Sbox = false;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* enableCacheFileCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsEnableCacheFile4SboxCheck"));
    QVERIFY(enableCacheFileCheck != nullptr);
    QVERIFY(!enableCacheFileCheck->isChecked());

    enableCacheFileCheck->setChecked(true);

    const Config updated = dialog.config();
    QVERIFY(updated.enableCacheFile4Sbox);
}

void SettingsDialogTests::defaultAllowInsecureCheckboxRoundTripsConfig()
{
    Config config;
    config.defaultAllowInsecure = true;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* defaultAllowInsecureCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsDefaultAllowInsecureCheck"));
    QVERIFY(defaultAllowInsecureCheck != nullptr);
    QVERIFY(defaultAllowInsecureCheck->isChecked());

    defaultAllowInsecureCheck->setChecked(false);

    const Config updated = dialog.config();
    QVERIFY(!updated.defaultAllowInsecure);
}

void SettingsDialogTests::defaultUserAgentComboRoundTripsConfig()
{
    Config config;
    config.defaultUserAgent = QStringLiteral("Mozilla/5.0");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* defaultUserAgentCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsDefaultUserAgentCombo"));
    QVERIFY(defaultUserAgentCombo != nullptr);
    QCOMPARE(defaultUserAgentCombo->currentText(), QStringLiteral("Mozilla/5.0"));

    defaultUserAgentCombo->setCurrentText(QStringLiteral("curl/8.0"));

    const Config updated = dialog.config();
    QCOMPARE(updated.defaultUserAgent, QStringLiteral("curl/8.0"));
}

void SettingsDialogTests::defaultFingerprintComboRoundTripsConfig()
{
    Config config;
    config.defaultFingerprint = QStringLiteral("firefox");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* defaultFingerprintCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsDefaultFingerprintCombo"));
    QVERIFY(defaultFingerprintCombo != nullptr);
    QCOMPARE(defaultFingerprintCombo->currentText(), QStringLiteral("firefox"));

    defaultFingerprintCombo->setCurrentText(QStringLiteral("edge"));

    const Config updated = dialog.config();
    QCOMPARE(updated.defaultFingerprint, QStringLiteral("edge"));
}

void SettingsDialogTests::statisticsToggleControlsRefreshRateSpin()
{
    Config config;
    config.enableStatistics = false;
    config.statisticsFreshRate = 7;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* statisticsCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsEnableStatisticsCheck"));
    auto* refreshRateSpin = dialog.findChild<QSpinBox*>(QStringLiteral("settingsStatisticsFreshRateSpin"));
    QVERIFY(statisticsCheck != nullptr);
    QVERIFY(refreshRateSpin != nullptr);

    QVERIFY(!statisticsCheck->isChecked());
    QVERIFY(!refreshRateSpin->isEnabled());

    statisticsCheck->setChecked(true);
    QVERIFY(refreshRateSpin->isEnabled());
}

void SettingsDialogTests::proxySettingsRoundTripsConfig()
{
    Config config;
    config.systemProxyExceptions = QStringLiteral("localhost;127.*");
    config.systemProxyAdvancedProtocol = QStringLiteral("{ip}:{http_port}");
    config.pacUrl = QStringLiteral("http://proxy.example/pac");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* exceptionsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsSystemProxyExceptionsEdit"));
    auto* protocolCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsSystemProxyAdvancedProtocolCombo"));
    auto* pacUrlEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsPacUrlEdit"));
    QVERIFY(exceptionsEdit != nullptr);
    QVERIFY(protocolCombo != nullptr);
    QVERIFY(pacUrlEdit != nullptr);
    QCOMPARE(exceptionsEdit->text(), QStringLiteral("localhost;127.*"));
    QCOMPARE(protocolCombo->currentText(), QStringLiteral("{ip}:{http_port}"));
    QCOMPARE(pacUrlEdit->text(), QStringLiteral("http://proxy.example/pac"));

    exceptionsEdit->setText(QStringLiteral("  10.0.0.0/8  "));
    protocolCombo->setCurrentText(QStringLiteral("socks={ip}:{socks_port}"));
    pacUrlEdit->setText(QStringLiteral("  http://proxy.example/custom-pac  "));

    const Config updated = dialog.config();
    QCOMPARE(updated.systemProxyExceptions, QStringLiteral("10.0.0.0/8"));
    QCOMPARE(updated.systemProxyAdvancedProtocol, QStringLiteral("socks={ip}:{socks_port}"));
    QCOMPARE(updated.pacUrl, QStringLiteral("http://proxy.example/custom-pac"));
}

void SettingsDialogTests::updateSettingsRoundTripsConfig()
{
    Config config;
    config.checkPreReleaseUpdate = false;
    config.ignoreGeoUpdateCore = true;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* preReleaseCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsCheckPreReleaseUpdateCheck"));
    auto* ignoreGeoCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsIgnoreGeoUpdateCoreCheck"));
    QVERIFY(preReleaseCheck != nullptr);
    QVERIFY(ignoreGeoCheck != nullptr);
    QVERIFY(!preReleaseCheck->isChecked());
    QVERIFY(ignoreGeoCheck->isChecked());

    preReleaseCheck->setChecked(true);
    ignoreGeoCheck->setChecked(false);

    const Config updated = dialog.config();
    QVERIFY(updated.checkPreReleaseUpdate);
    QVERIFY(!updated.ignoreGeoUpdateCore);
}

void SettingsDialogTests::tunSettingsRoundTripsConfig()
{
    Config config;
    config.tunModeItem.enableTun = true;
    config.tunModeItem.autoRoute = true;
    config.tunModeItem.strictRoute = false;
    config.tunModeItem.mtu = 1400;
    config.tunModeItem.stack = QStringLiteral("gvisor");
    config.tunModeItem.enableIPv6Address = true;
    config.tunModeItem.icmpRouting = QStringLiteral("tcp");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* enableCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunEnableCheck"));
    auto* autoRouteCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunAutoRouteCheck"));
    auto* strictRouteCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunStrictRouteCheck"));
    auto* mtuSpin = dialog.findChild<QSpinBox*>(QStringLiteral("settingsTunMtuSpin"));
    auto* stackCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsTunStackCombo"));
    auto* ipv6Check = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunEnableIPv6AddressCheck"));
    auto* icmpEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsTunIcmpRoutingEdit"));
    QVERIFY(enableCheck != nullptr);
    QVERIFY(autoRouteCheck != nullptr);
    QVERIFY(strictRouteCheck != nullptr);
    QVERIFY(mtuSpin != nullptr);
    QVERIFY(stackCombo != nullptr);
    QVERIFY(ipv6Check != nullptr);
    QVERIFY(icmpEdit != nullptr);
    QVERIFY(enableCheck->isChecked());
    QVERIFY(autoRouteCheck->isEnabled());
    QCOMPARE(mtuSpin->value(), 1400);
    QCOMPARE(stackCombo->currentText(), QStringLiteral("gvisor"));

    mtuSpin->setValue(1500);
    stackCombo->setCurrentText(QStringLiteral("mixed"));
    icmpEdit->setText(QStringLiteral("udp"));
    strictRouteCheck->setChecked(true);
    ipv6Check->setChecked(false);

    const Config updated = dialog.config();
    QVERIFY(updated.tunModeItem.enableTun);
    QVERIFY(updated.tunModeItem.autoRoute);
    QVERIFY(updated.tunModeItem.strictRoute);
    QCOMPARE(updated.tunModeItem.mtu, 1500);
    QCOMPARE(updated.tunModeItem.stack, QStringLiteral("mixed"));
    QVERIFY(!updated.tunModeItem.enableIPv6Address);
    QCOMPARE(updated.tunModeItem.icmpRouting, QStringLiteral("udp"));
}

void SettingsDialogTests::tunToggleControlsDependentFields()
{
    Config config;
    config.tunModeItem.enableTun = false;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* enableCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunEnableCheck"));
    auto* autoRouteCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunAutoRouteCheck"));
    auto* mtuSpin = dialog.findChild<QSpinBox*>(QStringLiteral("settingsTunMtuSpin"));
    QVERIFY(enableCheck != nullptr);
    QVERIFY(autoRouteCheck != nullptr);
    QVERIFY(mtuSpin != nullptr);

    QVERIFY(!autoRouteCheck->isEnabled());
    QVERIFY(!mtuSpin->isEnabled());

    enableCheck->setChecked(true);
    QVERIFY(autoRouteCheck->isEnabled());
    QVERIFY(mtuSpin->isEnabled());
}

void SettingsDialogTests::tunToggleControlsCoreLegacyProtect()
{
    Config config;
    config.tunModeItem.enableTun = false;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* enableCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunEnableCheck"));
    auto* legacyProtectCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunEnableLegacyProtectCheck"));
    QVERIFY(enableCheck != nullptr);
    QVERIFY(legacyProtectCheck != nullptr);
    QVERIFY(!legacyProtectCheck->isEnabled());

    enableCheck->setChecked(true);
    QVERIFY(legacyProtectCheck->isEnabled());
}

void SettingsDialogTests::mux4SboxProtocolComboRoundTripsConfig()
{
    Config config;
    config.mux4SboxProtocol = QStringLiteral("yamux");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* muxProtocolCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsMux4SboxProtocolCombo"));
    QVERIFY(muxProtocolCombo != nullptr);
    QCOMPARE(muxProtocolCombo->currentText(), QStringLiteral("yamux"));

    muxProtocolCombo->setCurrentText(QStringLiteral("smux"));

    const Config updated = dialog.config();
    QCOMPARE(updated.mux4SboxProtocol, QStringLiteral("smux"));
}

void SettingsDialogTests::updateTabDoesNotExposeRemovedTls13Option()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    const QList<QCheckBox*> checkboxes = dialog.findChildren<QCheckBox*>();
    for (QCheckBox* checkbox : checkboxes) {
        QVERIFY(checkbox->text() != QStringLiteral("Enable TLS 1.3 for downloads"));
    }
}

void SettingsDialogTests::coreTypeTableIncludesHttpProtocol()
{
    Config config;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* httpCombo = dialog.findChild<QComboBox*>(QStringLiteral("coreTypeCombo_11"));
    QVERIFY(httpCombo != nullptr);
    QCOMPARE(httpCombo->currentText(), QStringLiteral("sing-box"));

    httpCombo->setCurrentText(QStringLiteral("Xray"));

    const Config updated = dialog.config();
    auto it = std::find_if(
        updated.coreTypeItems.cbegin(),
        updated.coreTypeItems.cend(),
        [](const CoreTypeItem& item) { return item.configType == static_cast<int>(ConfigType::HTTP); });
    QVERIFY(it != updated.coreTypeItems.cend());
    QCOMPARE(it->coreType, static_cast<int>(CoreType::Xray));
}

void SettingsDialogTests::coreTypeTableDefaultsUseConfiguredValue()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    auto* vmessCombo = dialog.findChild<QComboBox*>(QStringLiteral("coreTypeCombo_1"));
    QVERIFY(vmessCombo != nullptr);
    QCOMPARE(vmessCombo->currentText(), QStringLiteral("sing-box"));

    dialog.setExistingCoreTypes({CoreType::Xray});
    QCOMPARE(vmessCombo->currentText(), QStringLiteral("sing-box"));

    dialog.setExistingCoreTypes({CoreType::Xray, CoreType::SingBox});
    QCOMPARE(vmessCombo->currentText(), QStringLiteral("sing-box"));

    Config config;
    config.coreTypeItems = {
        CoreTypeItem{static_cast<int>(ConfigType::VMess), static_cast<int>(CoreType::Xray)}
    };
    dialog.setConfig(config);
    QCOMPARE(vmessCombo->currentText(), QStringLiteral("Xray"));
}

void SettingsDialogTests::corePageUsesPlainContentWithoutGroupBoxes()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    auto* vmessCombo = dialog.findChild<QComboBox*>(QStringLiteral("coreTypeCombo_1"));
    QVERIFY(vmessCombo != nullptr);

    const QList<QGroupBox*> groups = dialog.findChildren<QGroupBox*>();
    for (QGroupBox* group : groups) {
        QVERIFY(group->title() != QStringLiteral("Per-Protocol Override"));
        QVERIFY(group->title() != QStringLiteral("Installed Cores"));
    }
}

void SettingsDialogTests::corePageShowsProtocolAndCoreSectionTitles()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    auto* protocolTitle = dialog.findChild<QLabel*>(QStringLiteral("coreProtocolSectionTitle"));
    auto* coreTitle = dialog.findChild<QLabel*>(QStringLiteral("coreStatusSectionTitle"));
    QVERIFY(protocolTitle != nullptr);
    QVERIFY(coreTitle != nullptr);
    QCOMPARE(protocolTitle->text(), QStringLiteral("By Protocol"));
    QCOMPARE(coreTitle->text(), QStringLiteral("By Core"));
    QVERIFY(protocolTitle->font().bold());
    QVERIFY(coreTitle->font().bold());
}

void SettingsDialogTests::routingRuleNetworkAndProcessRoundTripConfig()
{
    Config config;
    RoutingRule rule;
    rule.type = QStringLiteral("field");
    rule.outboundTag = QStringLiteral("proxy");
    rule.network = QStringLiteral("tcp,udp");
    rule.process = QStringList{
        QStringLiteral("self/"),
        QStringLiteral("C:/Program Files/Test/test.exe")};

    RoutingItem routing;
    routing.remarks = QStringLiteral("route");
    routing.locked = true;
    routing.rules = {rule};
    config.routingItems = {routing};

    SettingsDialog dialog;
    dialog.setConfig(config);

    const Config updated = dialog.config();
    QCOMPARE(updated.routingItems.size(), 1);
    QCOMPARE(updated.routingItems.constFirst().rules.size(), 1);
    const RoutingRule updatedRule = updated.routingItems.constFirst().rules.constFirst();
    QCOMPARE(updatedRule.network, QStringLiteral("tcp,udp"));
    QCOMPARE(
        updatedRule.process,
        QStringList({QStringLiteral("self/"), QStringLiteral("C:/Program Files/Test/test.exe")}));
}

void SettingsDialogTests::routingCustomRuleTabsRoundTripConfig()
{
    Config config;
    RoutingRule blockRule;
    blockRule.type = QStringLiteral("field");
    blockRule.outboundTag = QStringLiteral("block");
    blockRule.domain = QStringList{QStringLiteral("geosite:category-ads-all")};
    RoutingRule blockDomainRule;
    blockDomainRule.type = QStringLiteral("field");
    blockDomainRule.outboundTag = QStringLiteral("block");
    blockDomainRule.domain = QStringList{QStringLiteral("domain:tracking.example.com")};
    RoutingRule directRule;
    directRule.type = QStringLiteral("field");
    directRule.outboundTag = QStringLiteral("direct");
    directRule.ip = QStringList{QStringLiteral("geoip:private")};
    RoutingRule directIpRule;
    directIpRule.type = QStringLiteral("field");
    directIpRule.outboundTag = QStringLiteral("direct");
    directIpRule.ip = QStringList{QStringLiteral("10.0.0.0/8")};
    RoutingRule proxyRule;
    proxyRule.type = QStringLiteral("field");
    proxyRule.outboundTag = QStringLiteral("proxy");
    proxyRule.port = QStringLiteral("443");
    proxyRule.protocol = QStringList{QStringLiteral("tls")};
    config.routingCustomRules = {blockRule, blockDomainRule, directRule, directIpRule, proxyRule};

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("routingCustomRuleTabs"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("Block"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("Direct"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("Proxy"));

    auto* blockDomainEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomBlockDomainEdit"));
    auto* directIpEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomDirectIpEdit"));
    auto* proxyProtocolEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomProxyProtocolEdit"));
    auto* proxyPortEdit = dialog.findChild<QLineEdit*>(QStringLiteral("routingCustomProxyPortEdit"));
    QVERIFY(blockDomainEdit != nullptr);
    QVERIFY(directIpEdit != nullptr);
    QVERIFY(proxyProtocolEdit != nullptr);
    QVERIFY(proxyPortEdit != nullptr);

    QCOMPARE(
        blockDomainEdit->toPlainText().trimmed(),
        QStringLiteral("geosite:category-ads-all\ndomain:tracking.example.com"));
    QCOMPARE(directIpEdit->toPlainText().trimmed(), QStringLiteral("geoip:private\n10.0.0.0/8"));
    QCOMPARE(proxyProtocolEdit->toPlainText().trimmed(), QStringLiteral("tls"));
    QCOMPARE(proxyPortEdit->text(), QStringLiteral("443"));

    blockDomainEdit->setPlainText(QStringLiteral("domain:ads.example.com"));
    directIpEdit->setPlainText(QStringLiteral("192.168.0.0/16"));
    proxyProtocolEdit->setPlainText(QStringLiteral("bittorrent"));
    proxyPortEdit->setText(QStringLiteral("6881-6999"));

    const Config updated = dialog.config();
    QCOMPARE(updated.routingCustomRules.size(), 3);
    QCOMPARE(updated.routingCustomRules.at(0).outboundTag, QStringLiteral("block"));
    QCOMPARE(updated.routingCustomRules.at(0).domain, QStringList{QStringLiteral("domain:ads.example.com")});
    QCOMPARE(updated.routingCustomRules.at(1).outboundTag, QStringLiteral("direct"));
    QCOMPARE(updated.routingCustomRules.at(1).ip, QStringList{QStringLiteral("192.168.0.0/16")});
    QCOMPARE(updated.routingCustomRules.at(2).outboundTag, QStringLiteral("proxy"));
    QCOMPARE(updated.routingCustomRules.at(2).protocol, QStringList{QStringLiteral("bittorrent")});
    QCOMPARE(updated.routingCustomRules.at(2).port, QStringLiteral("6881-6999"));
}

void SettingsDialogTests::routingCustomRuleTabsDefaultToDirectAndPersistSelection()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("routingCustomRuleTabs"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("Direct"));
    QCOMPARE(dialog.config().settingsRoutingRuleTabKey, QStringLiteral("direct"));

    Config config;
    config.settingsRoutingRuleTabKey = QStringLiteral("proxy");
    dialog.setConfig(config);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("Proxy"));

    int blockIndex = -1;
    for (int index = 0; index < tabs->count(); ++index) {
        if (tabs->tabText(index) == QStringLiteral("Block")) {
            blockIndex = index;
            break;
        }
    }
    QVERIFY(blockIndex >= 0);
    tabs->setCurrentIndex(blockIndex);
    QCOMPARE(dialog.config().settingsRoutingRuleTabKey, QStringLiteral("block"));
}

void SettingsDialogTests::routingPageUsesCompactCardsAndPlainCustomRuleForms()
{
    Config config;
    RoutingRule blockRule;
    blockRule.type = QStringLiteral("field");
    blockRule.enabled = true;
    blockRule.outboundTag = QStringLiteral("block");
    blockRule.domain = QStringList{
        QStringLiteral("geosite:category-ads-all"),
        QStringLiteral("domain:very-long-domain-name-that-should-not-expand-the-card-width.example.com")};
    RoutingRule directRule;
    directRule.type = QStringLiteral("field");
    directRule.enabled = true;
    directRule.outboundTag = QStringLiteral("direct");
    directRule.ip = QStringList{QStringLiteral("geoip:private")};
    RoutingRule proxyRule;
    proxyRule.type = QStringLiteral("field");
    proxyRule.enabled = true;
    proxyRule.outboundTag = QStringLiteral("proxy");
    proxyRule.protocol = QStringList{QStringLiteral("tls")};
    proxyRule.port = QStringLiteral("443");
    RoutingItem routing;
    routing.remarks = QStringLiteral("Builtin");
    routing.rules = {blockRule, directRule, proxyRule};
    config.routingItems = {routing};

    SettingsDialog dialog;
    dialog.setConfig(config);

    const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
    for (QLabel* label : labels) {
        QVERIFY(label->text() != QStringLiteral("Custom rules are applied first in this order: block, direct, proxy, then the selected base route."));
    }

    auto* card = dialog.findChild<QPushButton*>(QStringLiteral("routingBaseRouteCard_0"));
    QVERIFY(card != nullptr);
    QVERIFY(card->styleSheet().isEmpty());
    QVERIFY(card->text().contains(QStringLiteral("BLOCK: geosite:category-ads-all")));
    QVERIFY(card->text().contains(QStringLiteral("DIRECT: geoip:private")));
    QVERIFY(card->text().contains(QStringLiteral("PROXY: port: 443, protocol: tls")));
    QVERIFY(card->toolTip().contains(QStringLiteral("BLOCK: geosite:category-ads-all, domain:very-long-domain-name-that-should-not-expand-the-card-width.example.com")));
    QVERIFY(card->toolTip().contains(QStringLiteral("DIRECT: geoip:private")));
    QVERIFY(card->toolTip().contains(QStringLiteral("PROXY: port: 443, protocol: tls")));
    QVERIFY(card->toolTip().indexOf(QStringLiteral("BLOCK:")) < card->toolTip().indexOf(QStringLiteral("DIRECT:")));
    QVERIFY(card->toolTip().indexOf(QStringLiteral("DIRECT:")) < card->toolTip().indexOf(QStringLiteral("PROXY:")));
    QVERIFY(card->toolTip().contains(QStringLiteral("domain:very-long-domain-name-that-should-not-expand-the-card-width.example.com")));
    QVERIFY(card->minimumHeight() == 0);
    QVERIFY(card->maximumHeight() == QWIDGETSIZE_MAX);

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("routingCustomRuleTabs"));
    QVERIFY(tabs != nullptr);
    for (int index = 0; index < tabs->count(); ++index) {
        QVERIFY(tabs->widget(index)->findChildren<QGroupBox*>().isEmpty());
    }
    QVERIFY(dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomProxyProtocolEdit")) != nullptr);
}

void SettingsDialogTests::routingBaseRouteCardsCollapseAroundSelectedCard()
{
    Config config;
    for (int index = 0; index < 3; ++index) {
        RoutingItem item;
        item.remarks = QStringLiteral("Route %1").arg(index + 1);
        if (index == 2) {
            RoutingRule rule;
            rule.type = QStringLiteral("field");
            rule.enabled = true;
            rule.outboundTag = QStringLiteral("block");
            rule.domain = QStringList{
                QStringLiteral("geosite:category-ads-all"),
                QStringLiteral("domain:very-long-domain-name-that-should-wrap-in-expanded-card.example.com")};
            item.rules = {rule};
        }
        config.routingItems.append(item);
    }

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* first = dialog.findChild<QPushButton*>(QStringLiteral("routingBaseRouteCard_0"));
    auto* second = dialog.findChild<QPushButton*>(QStringLiteral("routingBaseRouteCard_1"));
    auto* third = dialog.findChild<QPushButton*>(QStringLiteral("routingBaseRouteCard_2"));
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY(third != nullptr);

    auto* routeLayout = dialog.findChild<QHBoxLayout*>(QStringLiteral("routingBaseRouteLayout"));
    QVERIFY(routeLayout != nullptr);
    QCOMPARE(routeLayout->count(), 3);
    QCOMPARE(routeLayout->stretch(0), 1);
    QCOMPARE(routeLayout->stretch(1), 0);
    QCOMPARE(routeLayout->stretch(2), 0);

    QCOMPARE(first->maximumWidth(), QWIDGETSIZE_MAX);
    QCOMPARE(second->maximumWidth(), 100);
    QCOMPARE(third->maximumWidth(), 100);
    QCOMPARE(first->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(second->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);

    third->click();

    QCOMPARE(first->maximumWidth(), 100);
    QCOMPARE(second->maximumWidth(), 100);
    QCOMPARE(third->maximumWidth(), QWIDGETSIZE_MAX);
    QCOMPARE(routeLayout->count(), 3);
    QCOMPARE(routeLayout->stretch(0), 0);
    QCOMPARE(routeLayout->stretch(1), 0);
    QCOMPARE(routeLayout->stretch(2), 1);
    QCOMPARE(third->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QVERIFY(!third->text().contains(QStringLiteral("...")));
    QVERIFY(third->text().contains(QStringLiteral("domain:very-long-domain-name-that-should-wrap-in-expanded-card.example.com")));

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("routingCustomRuleTabs"));
    QVERIFY(tabs != nullptr);
    auto* tabLayout = qobject_cast<QHBoxLayout*>(tabs->widget(0)->layout());
    QVERIFY(tabLayout != nullptr);
    QMargins margins = tabLayout->contentsMargins();
    QCOMPARE(margins.left(), 9);
    QCOMPARE(margins.top(), 9);
    QCOMPARE(margins.right(), 9);
    QCOMPARE(margins.bottom(), 9);
    QCOMPARE(tabLayout->stretch(0), 1);
    QCOMPARE(tabLayout->stretch(1), 1);
    QCOMPARE(tabLayout->stretch(2), 1);

    auto* protocolEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomBlockProtocolEdit"));
    QVERIFY(protocolEdit != nullptr);
    QCOMPARE(protocolEdit->maximumHeight(), QWIDGETSIZE_MAX);
    QVERIFY(protocolEdit->styleSheet().isEmpty());

    auto* baseRouteTitle = dialog.findChild<QLabel*>(QStringLiteral("routingBaseRouteTitle"));
    auto* customRulesTitle = dialog.findChild<QLabel*>(QStringLiteral("routingCustomRulesTitle"));
    QVERIFY(baseRouteTitle != nullptr);
    QVERIFY(customRulesTitle != nullptr);
    QVERIFY(baseRouteTitle->font().bold());
    QVERIFY(customRulesTitle->font().bold());
    QVERIFY(first->property("routeTitleBold").toBool());
}

void SettingsDialogTests::routingPageShowsExplicitEmptyStateWithoutBaseRoutes()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    auto* baseRouteTitle = dialog.findChild<QLabel*>(QStringLiteral("routingBaseRouteTitle"));
    auto* customRulesTitle = dialog.findChild<QLabel*>(QStringLiteral("routingCustomRulesTitle"));
    auto* customRuleTabs = dialog.findChild<QTabWidget*>(QStringLiteral("routingCustomRuleTabs"));
    QVERIFY(baseRouteTitle != nullptr);
    QVERIFY(customRulesTitle != nullptr);
    QVERIFY(customRuleTabs != nullptr);

    QCOMPARE(baseRouteTitle->text(), QStringLiteral("Base Route (No Presets Configured)"));
    QCOMPARE(customRulesTitle->text(), QStringLiteral("Custom Rules (Still Applied)"));
    QVERIFY(baseRouteTitle->toolTip().contains(QStringLiteral("Custom rules below are still applied")));
    QCOMPARE(customRuleTabs->toolTip(), customRulesTitle->toolTip());

    Config config;
    RoutingItem routing;
    routing.remarks = QStringLiteral("Builtin");
    config.routingItems = {routing};
    dialog.setConfig(config);

    QCOMPARE(baseRouteTitle->text(), QStringLiteral("Base Route"));
    QCOMPARE(customRulesTitle->text(), QStringLiteral("Custom Rules"));
}

void SettingsDialogTests::settingsDialogUsesCompactUiFontBaseline()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();

    const qreal baseline = dialog.font().pointSizeF();

    auto* settingsTabBar = dialog.findChild<QTabBar*>(QStringLiteral("settingsTabBar"));
    auto* subTable = dialog.findChild<QTableWidget*>();
    auto* systemProxyExceptionsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsSystemProxyExceptionsEdit"));
    auto* languageCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsLanguageCombo"));
    QVERIFY(settingsTabBar != nullptr);
    QVERIFY(subTable != nullptr);
    QVERIFY(systemProxyExceptionsEdit != nullptr);
    QVERIFY(languageCombo != nullptr);

    QVERIFY(settingsTabBar->font().pointSizeF() <= baseline);
    QVERIFY(subTable->font().pointSizeF() <= baseline);
    QVERIFY(systemProxyExceptionsEdit->font().pointSizeF() <= baseline);
    QVERIFY(languageCombo->font().pointSizeF() <= baseline);
}

void SettingsDialogTests::fakeIpToggleControlsGlobalFakeIpCheckbox()
{
    Config config;
    config.fakeIp = false;
    config.globalFakeIp = true;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* fakeIpCheck = dialog.findChild<QCheckBox*>(QStringLiteral("dnsFakeIpCheck"));
    auto* globalFakeIpCheck = dialog.findChild<QCheckBox*>(QStringLiteral("dnsGlobalFakeIpCheck"));
    QVERIFY(fakeIpCheck != nullptr);
    QVERIFY(globalFakeIpCheck != nullptr);

    QVERIFY(!fakeIpCheck->isChecked());
    QVERIFY(!globalFakeIpCheck->isEnabled());

    fakeIpCheck->setChecked(true);
    QVERIFY(globalFakeIpCheck->isEnabled());

    fakeIpCheck->setChecked(false);
    QVERIFY(!globalFakeIpCheck->isEnabled());
}

void SettingsDialogTests::dnsPageRoundTripsFields()
{
    Config config;
    config.remoteDns = QStringLiteral("https://one.example/dns-query");
    config.directDns = QStringLiteral("https://two.example/dns-query");
    config.bootstrapDns = QStringLiteral("1.1.1.1");
    config.domainStrategyForFreedom = QStringLiteral("PreferIPv6");
    config.domainStrategyForProxy = QStringLiteral("PreferIPv4");
    config.useSystemHosts = true;
    config.addCommonHosts = true;
    config.blockBindingQuery = true;
    config.fakeIp = true;
    config.globalFakeIp = true;
    config.serveStale = true;
    config.parallelQuery = true;
    config.directExpectedIps = QStringLiteral("geoip:private");
    config.dnsHosts = QStringLiteral("example.com 1.2.3.4");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* remoteDnsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsRemoteDnsEdit"));
    auto* freedomCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsDomainStrategyForFreedomCombo"));
    auto* proxyCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsDomainStrategyForProxyCombo"));
    auto* directExpectedIpsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsDirectExpectedIpsEdit"));
    auto* dnsHostsEdit = dialog.findChild<QTextEdit*>(QStringLiteral("settingsDnsHostsEdit"));
    auto* useSystemHostsCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsUseSystemHostsCheck"));
    auto* addCommonHostsCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsAddCommonHostsCheck"));
    auto* blockBindingQueryCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsBlockBindingQueryCheck"));
    auto* fakeIpCheck = dialog.findChild<QCheckBox*>(QStringLiteral("dnsFakeIpCheck"));
    auto* globalFakeIpCheck = dialog.findChild<QCheckBox*>(QStringLiteral("dnsGlobalFakeIpCheck"));
    QVERIFY(remoteDnsEdit != nullptr);
    QVERIFY(freedomCombo != nullptr);
    QVERIFY(proxyCombo != nullptr);
    QVERIFY(directExpectedIpsEdit != nullptr);
    QVERIFY(dnsHostsEdit != nullptr);
    QVERIFY(useSystemHostsCheck != nullptr);
    QVERIFY(addCommonHostsCheck != nullptr);
    QVERIFY(blockBindingQueryCheck != nullptr);
    QVERIFY(fakeIpCheck != nullptr);
    QVERIFY(globalFakeIpCheck != nullptr);

    QCOMPARE(remoteDnsEdit->text(), QStringLiteral("https://one.example/dns-query"));
    QCOMPARE(freedomCombo->currentText(), QStringLiteral("PreferIPv6"));
    QCOMPARE(proxyCombo->currentText(), QStringLiteral("PreferIPv4"));
    QVERIFY(useSystemHostsCheck->isChecked());
    QVERIFY(addCommonHostsCheck->isChecked());
    QVERIFY(blockBindingQueryCheck->isChecked());
    QVERIFY(fakeIpCheck->isChecked());
    QVERIFY(globalFakeIpCheck->isChecked());
    QCOMPARE(directExpectedIpsEdit->text(), QStringLiteral("geoip:private"));
    QCOMPARE(dnsHostsEdit->toPlainText(), QStringLiteral("example.com 1.2.3.4"));

    remoteDnsEdit->setText(QStringLiteral("https://changed.example/dns-query"));
    freedomCombo->setCurrentText(QStringLiteral("OnlyIPv4"));
    proxyCombo->setCurrentText(QStringLiteral("OnlyIPv6"));
    useSystemHostsCheck->setChecked(false);
    addCommonHostsCheck->setChecked(false);
    blockBindingQueryCheck->setChecked(false);
    directExpectedIpsEdit->setText(QStringLiteral("1.2.3.0/24"));
    dnsHostsEdit->setPlainText(QStringLiteral("full:full.example.com 5.6.7.8"));

    const Config updated = dialog.config();
    QCOMPARE(updated.remoteDns, QStringLiteral("https://changed.example/dns-query"));
    QCOMPARE(updated.domainStrategyForFreedom, QStringLiteral("OnlyIPv4"));
    QCOMPARE(updated.domainStrategyForProxy, QStringLiteral("OnlyIPv6"));
    QVERIFY(!updated.useSystemHosts);
    QVERIFY(!updated.addCommonHosts);
    QVERIFY(!updated.blockBindingQuery);
    QCOMPARE(updated.directExpectedIps, QStringLiteral("1.2.3.0/24"));
    QCOMPARE(updated.dnsHosts, QStringLiteral("full:full.example.com 5.6.7.8"));
}

void SettingsDialogTests::subscriptionTableUsesServerTableStyle()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();

    auto* subTable = dialog.findChild<QTableWidget*>();
    QVERIFY(subTable != nullptr);

    QVERIFY(subTable->property("serverTableStyle").toBool());
    QCOMPARE(subTable->palette().color(QPalette::AlternateBase), QColor(QStringLiteral("#f9f9f9")));
    QVERIFY(!subTable->showGrid());
}

void SettingsDialogTests::subscriptionPageRoundTripsSelectedRowsAndItems()
{
    Config config;
    config.subscriptions = {
        SubItem{QStringLiteral("sub-1"), QStringLiteral("A"), QStringLiteral("https://a.example"), true, QStringLiteral("ua-a")},
        SubItem{QStringLiteral("sub-2"), QStringLiteral("B"), QStringLiteral("https://b.example"), false, QStringLiteral("ua-b")}
    };

    SettingsDialog dialog;
    dialog.setConfig(config);
    dialog.show();
    QCoreApplication::processEvents();

    auto* subTable = dialog.findChild<QTableWidget*>();
    QVERIFY(subTable != nullptr);
    subTable->selectRow(1);

    const QList<int> rows = dialog.selectedSubRows();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first(), 1);

    const Config updated = dialog.config();
    QCOMPARE(updated.subscriptions.size(), 2);
    QCOMPARE(updated.subscriptions.at(0).id, QStringLiteral("sub-1"));
    QCOMPARE(updated.subscriptions.at(0).remarks, QStringLiteral("A"));
    QCOMPARE(updated.subscriptions.at(1).id, QStringLiteral("sub-2"));
    QCOMPARE(updated.subscriptions.at(1).url, QStringLiteral("https://b.example"));
    QVERIFY(!updated.subscriptions.at(1).enabled);
}

void SettingsDialogTests::settingsDialogTabBarUsesImageReferenceStyleHooks()
{
    SettingsDialog dialog;
    dialog.resize(880, 540);
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();

    auto* settingsTabBar = dialog.findChild<QTabBar*>(QStringLiteral("settingsTabBar"));
    auto* settingsTabBarContainer = dialog.findChild<QWidget*>(QStringLiteral("settingsTabBarContainer"));
    auto* settingsActionBar = dialog.findChild<QWidget*>(QStringLiteral("settingsActionBar"));
    auto* settingsStackContainer = dialog.findChild<QWidget*>(QStringLiteral("settingsStackContainer"));
    auto* settingsStackLayout = dialog.findChild<QStackedLayout*>(QStringLiteral("settingsStackLayout"));
    auto* routingCustomRuleTabs = dialog.findChild<QTabWidget*>(QStringLiteral("routingCustomRuleTabs"));
    QVERIFY(settingsTabBar != nullptr);
    QVERIFY(settingsTabBarContainer != nullptr);
    QVERIFY(settingsActionBar != nullptr);
    QVERIFY(settingsStackContainer != nullptr);
    QVERIFY(settingsStackLayout != nullptr);
    QVERIFY(routingCustomRuleTabs != nullptr);
    QCOMPARE(settingsTabBar->objectName(), QStringLiteral("settingsTabBar"));
    QVERIFY(!settingsTabBar->expanding());
    QVERIFY(!settingsTabBar->drawBase());
    QCOMPARE(settingsTabBar->count(), settingsStackLayout->count());
    QVERIFY(routingCustomRuleTabs->documentMode());
    QVERIFY(!routingCustomRuleTabs->tabBar()->drawBase());
    QVERIFY(!routingCustomRuleTabs->tabBar()->expanding());

    const QMargins margins = settingsTabBar->contentsMargins();
    QCOMPARE(margins.left(), 0);
    QCOMPARE(margins.top(), 0);
    QCOMPARE(margins.right(), 0);
    QCOMPARE(margins.bottom(), 0);

    auto* containerLayout = settingsTabBarContainer->layout();
    QVERIFY(containerLayout != nullptr);
    const QMargins containerMargins = containerLayout->contentsMargins();
    QCOMPARE(containerMargins.left(), 0);
    QCOMPARE(containerMargins.top(), 0);
    QCOMPARE(containerMargins.right(), 0);
    QCOMPARE(containerMargins.bottom(), 0);
    QVERIFY(settingsTabBar->width() <= settingsTabBarContainer->contentsRect().width());
    QVERIFY(settingsTabBar->tabRect(settingsTabBar->count() - 1).right() < settingsTabBar->width());
    QVERIFY(settingsActionBar->layout() != nullptr);
}

void SettingsDialogTests::settingsDialogSelectedTabReservesBoldTextWidth()
{
    SettingsDialog dialog;
    dialog.resize(880, 540);
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();

    auto* settingsTabBar = dialog.findChild<QTabBar*>(QStringLiteral("settingsTabBar"));
    QVERIFY(settingsTabBar != nullptr);

    for (int index = 0; index < settingsTabBar->count(); ++index) {
        settingsTabBar->setCurrentIndex(index);
        QCoreApplication::processEvents();

        QFont selectedFont = settingsTabBar->font();
        selectedFont.setWeight(QFont::DemiBold);
        const int selectedTextWidth = QFontMetrics(selectedFont).horizontalAdvance(settingsTabBar->tabText(index));
        const int horizontalPadding = settingsTabBar->tabRect(index).width()
            - QFontMetrics(settingsTabBar->font()).horizontalAdvance(settingsTabBar->tabText(index));

        QVERIFY(settingsTabBar->tabRect(index).width() >= selectedTextWidth);
        QVERIFY(horizontalPadding >= 0);
    }
}

void SettingsDialogTests::settingsDialogHasMinimumWidth720()
{
    SettingsDialog dialog;
    QCOMPARE(dialog.minimumWidth(), 720);
}

QTEST_MAIN(SettingsDialogTests)

#include "SettingsDialogTests.moc"
