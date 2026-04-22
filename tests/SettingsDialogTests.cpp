#include <QtTest>

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QLabel>

#include "ui/dialogs/SettingsDialog.h"

class SettingsDialogTests : public QObject {
    Q_OBJECT

private slots:
    void downloadButtonStartsInlineUpdate_data();
    void downloadButtonStartsInlineUpdate();
    void coreUpdateProgressRefreshesInlineStatus();
    void sniffingEnabledCheckboxRoundTripsConfig();
    void routeOnlyCheckboxRoundTripsConfig();
    void enableFragmentCheckboxRoundTripsConfig();
    void enableCacheFile4SboxCheckboxRoundTripsConfig();
    void defaultAllowInsecureCheckboxRoundTripsConfig();
    void defaultUserAgentComboRoundTripsConfig();
    void defaultFingerprintComboRoundTripsConfig();
    void mux4SboxProtocolComboRoundTripsConfig();
    void coreTypeTableIncludesHttpProtocol();
    void routingRuleNetworkAndProcessRoundTripConfig();
};

void SettingsDialogTests::downloadButtonStartsInlineUpdate_data()
{
    QTest::addColumn<QString>("buttonObjectName");
    QTest::addColumn<int>("expectedCoreType");

    QTest::newRow("xray") << QStringLiteral("downloadXrayButton") << static_cast<int>(CoreType::Xray);
    QTest::newRow("sing-box") << QStringLiteral("downloadSingBoxButton") << static_cast<int>(CoreType::SingBox);
}

void SettingsDialogTests::downloadButtonStartsInlineUpdate()
{
    QFETCH(QString, buttonObjectName);
    QFETCH(int, expectedCoreType);

    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.setCoreVersions(QString(), QString());

    auto* button = dialog.findChild<QPushButton*>(buttonObjectName);
    QVERIFY(button != nullptr);
    QSignalSpy spy(&dialog, SIGNAL(coreDownloadRequested(int)));
    QVERIFY(spy.isValid());

    button->click();

    QCOMPARE(spy.count(), 1);
    QVERIFY(dialog.wasCoreDownloadRequested());
    QCOMPARE(static_cast<int>(dialog.requestedCoreDownload()), expectedCoreType);
    QCOMPARE(dialog.result(), 0);
    QVERIFY(!button->isVisible());
}

void SettingsDialogTests::coreUpdateProgressRefreshesInlineStatus()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.setCoreVersions(QString(), QString());

    auto* xrayButton = dialog.findChild<QPushButton*>(QStringLiteral("downloadXrayButton"));
    auto* xrayLabel = dialog.findChild<QLabel*>(QStringLiteral("coreVersionLabel"));
    auto* xrayStatusLabel = dialog.findChild<QLabel*>(QStringLiteral("coreStatusLabel"));
    QVERIFY(xrayButton != nullptr);
    QVERIFY(xrayLabel != nullptr);
    QVERIFY(xrayStatusLabel != nullptr);

    dialog.beginCoreUpdate(CoreType::Xray);
    dialog.setCoreUpdateProgress(CoreType::Xray, QStringLiteral("Downloading Xray package..."));

    QVERIFY(!xrayButton->isVisible());
    QCOMPARE(xrayLabel->text(), QStringLiteral("Downloading..."));
    QVERIFY(!xrayStatusLabel->isVisible());
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

void SettingsDialogTests::coreTypeTableIncludesHttpProtocol()
{
    Config config;
    CoreTypeItem httpItem;
    httpItem.configType = static_cast<int>(ConfigType::HTTP);
    httpItem.coreType = static_cast<int>(CoreType::SingBox);
    config.coreTypeItems = {httpItem};

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* httpCombo = dialog.findChild<QComboBox*>(QStringLiteral("coreTypeCombo_11"));
    QVERIFY(httpCombo != nullptr);
    QCOMPARE(httpCombo->currentText(), QStringLiteral("sing_box"));

    httpCombo->setCurrentText(QStringLiteral("Xray"));

    const Config updated = dialog.config();
    auto it = std::find_if(
        updated.coreTypeItems.cbegin(),
        updated.coreTypeItems.cend(),
        [](const CoreTypeItem& item) { return item.configType == static_cast<int>(ConfigType::HTTP); });
    QVERIFY(it != updated.coreTypeItems.cend());
    QCOMPARE(it->coreType, static_cast<int>(CoreType::Xray));
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

QTEST_MAIN(SettingsDialogTests)

#include "SettingsDialogTests.moc"
