#include <QtTest>

#include <cmath>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QStackedLayout>
#include <QTextEdit>
#include <QWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTranslator>

#include "ui/dialogs/RoutingCustomRuleSupport.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/dialogs/SubscriptionSettingsPageWidget.h"
#include "ui/theme/AppTheme.h"

class SettingsDialogTests : public QObject {
    Q_OBJECT

private slots:
    void downloadButtonStartsInlineUpdate_data();
    void downloadButtonStartsInlineUpdate();
    void coreUpdateProgressRefreshesInlineStatus();
    void coreUpdateDownloadProgressDrivesProgressBar();
    void coreUpdateCancelRequestsAndDisablesButton();
    void failedCoreUpdateOffersRetryWithReason();
    void coreUpdateProgressBarTooltipReportsBytes();
    void successfulCoreUpdateFinishRestoresIdleStatus();
    void installedCoreVersionShowsUpdateAction();
    void sniffingEnabledCheckboxRoundTripsConfig();
    void sniffingToggleControlsRouteOnlyCheckbox();
    void routeOnlyCheckboxRoundTripsConfig();
    void enableFragmentCheckboxRoundTripsConfig();
    void enableCacheFile4SboxCheckboxRoundTripsConfig();
    void defaultAllowInsecureCheckboxRoundTripsConfig();
    void defaultAllowInsecureTooltipExplainsNodeOverridesAndUsesTranslation();
    void defaultUserAgentComboRoundTripsConfig();
    void defaultFingerprintComboRoundTripsConfig();
    void speedPingTestUrlRoundTripsConfig();
    void skipOutboundCheckRoundTripsConfig();
    void themeComboRoundTripsConfig();
    void themeComboStoresFollowSystemPreference();
    void tunSettingsRoundTripsConfig();
    void tunToggleControlsDependentFields();
    void tunToggleControlsCoreLegacyProtect();
    void mux4SboxProtocolComboRoundTripsConfig();
    void updateTabDoesNotExposeRemovedTls13Option();
    void coreTypeTableIncludesHttpProtocol();
    void coreTypeTableUsesComboForSingleCoreProtocol();
    void coreTypeTableDefaultsUseConfiguredValue();
    void corePageUsesPlainContentWithoutGroupBoxes();
    void corePageShowsProtocolAndCoreSectionTitles();
    void routingRuleNetworkAndProcessRoundTripConfig();
    void routingCustomRuleProcessRoundTripsThroughProcessEditor();
    void routingCustomRuleWithMultipleFieldKindsIsPreservedVerbatim();
    void routingCustomRuleWithMultipleFieldKindsRoundTripsThroughDialog();
    void routingCustomRuleOrderSurvivesPartitionAndCollect();
    void routingCustomRuleOrderSurvivesTheDialog();
    void routingCustomRuleTabsRoundTripConfig();
    void routingPageReportsValuesNoCoreCanHonour();
    void routingCustomRuleTabsDefaultToDirectAndPersistSelection();
    void routingPageUsesCompactCardsAndPlainCustomRuleForms();
    void routingBaseRouteCardsCollapseAroundSelectedCard();
    void routingPageShowsExplicitEmptyStateWithoutBaseRoutes();
    void settingsDialogUsesCompactUiFontBaseline();
    void fakeIpToggleControlsGlobalFakeIpCheckbox();
    void dnsPageRoundTripsFields();
    void dnsPageBootstrapHintFollowsTheAddressesAboveIt();
    void dnsPageKeepsTheSameRowRhythmAsTheOtherFormPages();
    void corePageOwnsTheSingleCoreDnsOptions();
    void subscriptionTableUsesServerTableStyle();
    void subscriptionTableUsesDarkThemePalette();
    void serverTableDarkThemePaintsAlternateRowsDark();
    void applicationThemeLoadsResourceStyleSheet();
    void subscriptionPageRoundTripsSelectedRowsAndItems();
    void subscriptionPageNewRowDefaultsEmptyUserAgentAndAllowsCustomInput();
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

void SettingsDialogTests::coreUpdateDownloadProgressDrivesProgressBar()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.setCoreVersion(CoreType::Xray, QString());

    const int xrayKey = static_cast<int>(CoreType::Xray);
    auto* xrayLabel = dialog.findChild<QLabel*>(QStringLiteral("coreVersionLabel_%1").arg(xrayKey));
    auto* xrayBar = dialog.findChild<QProgressBar*>(QStringLiteral("coreDownloadProgress_%1").arg(xrayKey));
    QVERIFY(xrayLabel != nullptr);
    QVERIFY(xrayBar != nullptr);
    QVERIFY(xrayBar->isHidden());

    // While the release is being resolved no byte count exists yet, so the bar runs as a busy
    // indicator instead of pretending to know a percentage.
    dialog.beginCoreUpdate(CoreType::Xray);
    QVERIFY(!xrayBar->isHidden());
    QCOMPARE(xrayBar->minimum(), 0);
    QCOMPARE(xrayBar->maximum(), 0);
    QVERIFY(xrayLabel->isHidden());

    // First bytes with a known content length switch the bar to a real percentage.
    dialog.setCoreUpdateDownloadProgress(CoreType::Xray, 25, 100);
    QCOMPARE(xrayBar->minimum(), 0);
    QCOMPARE(xrayBar->maximum(), 100);
    QCOMPARE(xrayBar->value(), 25);

    // Progress keeps advancing as more bytes arrive.
    dialog.setCoreUpdateDownloadProgress(CoreType::Xray, 60, 100);
    QCOMPARE(xrayBar->value(), 60);

    // An unknown total falls back to the busy indicator rather than a bogus percent.
    dialog.setCoreUpdateDownloadProgress(CoreType::Xray, 60, 0);
    QCOMPARE(xrayBar->maximum(), 0);

    // Finishing hides the bar, resets it, and brings the version line back.
    dialog.finishCoreUpdate(CoreType::Xray, true);
    QVERIFY(xrayBar->isHidden());
    QCOMPARE(xrayBar->value(), 0);
    QCOMPARE(xrayBar->maximum(), 100);
    QVERIFY(!xrayLabel->isHidden());
}

void SettingsDialogTests::coreUpdateCancelRequestsAndDisablesButton()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    const int xrayKey = static_cast<int>(CoreType::Xray);
    auto* cancelButton = dialog.findChild<QPushButton*>(QStringLiteral("coreCancelButton_%1").arg(xrayKey));
    QVERIFY(cancelButton != nullptr);
    // The cancel affordance is only meaningful mid-update.
    QVERIFY(cancelButton->isHidden());

    QSignalSpy cancelSpy(&dialog, SIGNAL(coreUpdateCancelRequested(int)));
    QVERIFY(cancelSpy.isValid());

    dialog.beginCoreUpdate(CoreType::Xray);
    QVERIFY(!cancelButton->isHidden());
    QVERIFY(cancelButton->isEnabled());

    cancelButton->click();
    QCOMPARE(cancelSpy.count(), 1);
    QCOMPARE(cancelSpy.at(0).at(0).toInt(), xrayKey);

    // Pressing it again would be a no-op, so the UI stops offering it while the
    // worker is still winding down.
    dialog.markCoreUpdateCancelRequested(CoreType::Xray);
    QVERIFY(!cancelButton->isEnabled());

    dialog.finishCoreUpdate(CoreType::Xray, true);
    QVERIFY(cancelButton->isHidden());
}

void SettingsDialogTests::failedCoreUpdateOffersRetryWithReason()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    const int xrayKey = static_cast<int>(CoreType::Xray);
    auto* xrayButton = dialog.findChild<QPushButton*>(
        QStringLiteral("coreDownloadButton_%1").arg(xrayKey));
    QVERIFY(xrayButton != nullptr);

    dialog.beginCoreUpdate(CoreType::Xray);
    dialog.finishCoreUpdate(CoreType::Xray, false, QStringLiteral("HTTP 404"));

    // The button becomes a retry, and its tooltip carries the reason so the user
    // does not have to open the log to find out what went wrong.
    QVERIFY(!xrayButton->isHidden());
    QVERIFY(xrayButton->isEnabled());
    QCOMPARE(xrayButton->text(), QStringLiteral("Retry"));
    QVERIFY(xrayButton->toolTip().contains(QStringLiteral("HTTP 404")));

    // Starting again clears the stale reason.
    dialog.beginCoreUpdate(CoreType::Xray);
    QCOMPARE(xrayButton->text(), QStringLiteral("Install"));
    QVERIFY(xrayButton->toolTip().isEmpty());
}

void SettingsDialogTests::coreUpdateProgressBarTooltipReportsBytes()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    const int xrayKey = static_cast<int>(CoreType::Xray);
    auto* xrayBar = dialog.findChild<QProgressBar*>(QStringLiteral("coreDownloadProgress_%1").arg(xrayKey));
    QVERIFY(xrayBar != nullptr);

    dialog.beginCoreUpdate(CoreType::Xray);
    dialog.setCoreUpdateDownloadProgress(CoreType::Xray, 512 * 1024, 1024 * 1024);

    // The bar itself only has room for the percentage, so the byte counts live in
    // the tooltip -- the only place the download size is visible.
    QVERIFY(xrayBar->toolTip().contains(QStringLiteral("512.0 KB")));
    QVERIFY(xrayBar->toolTip().contains(QStringLiteral("1.0 MB")));
    QVERIFY(xrayBar->toolTip().contains(QStringLiteral("50%")));
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
    config.dns().enableFragment = true;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* enableFragmentCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsEnableFragmentCheck"));
    QVERIFY(enableFragmentCheck != nullptr);
    QVERIFY(enableFragmentCheck->isChecked());

    enableFragmentCheck->setChecked(false);

    const Config updated = dialog.config();
    QVERIFY(!updated.dns().enableFragment);
}

void SettingsDialogTests::enableCacheFile4SboxCheckboxRoundTripsConfig()
{
    Config config;
    config.dns().enableCacheFile4Sbox = false;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* enableCacheFileCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsEnableCacheFile4SboxCheck"));
    QVERIFY(enableCacheFileCheck != nullptr);
    QVERIFY(!enableCacheFileCheck->isChecked());

    enableCacheFileCheck->setChecked(true);

    const Config updated = dialog.config();
    QVERIFY(updated.dns().enableCacheFile4Sbox);
}

void SettingsDialogTests::defaultAllowInsecureCheckboxRoundTripsConfig()
{
    Config config;
    config.dns().defaultAllowInsecure = true;

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* defaultAllowInsecureCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsDefaultAllowInsecureCheck"));
    QVERIFY(defaultAllowInsecureCheck != nullptr);
    QVERIFY(defaultAllowInsecureCheck->isChecked());

    defaultAllowInsecureCheck->setChecked(false);

    const Config updated = dialog.config();
    QVERIFY(!updated.dns().defaultAllowInsecure);
}

void SettingsDialogTests::defaultAllowInsecureTooltipExplainsNodeOverridesAndUsesTranslation()
{
    class TooltipTranslator : public QTranslator {
    public:
        bool isEmpty() const override { return false; }
        QString translate(const char* context, const char* sourceText, const char*, int) const override
        {
            return qstrcmp(context, "GeneralSettingsPageWidget") == 0
                ? QStringLiteral("[zh]%1").arg(QString::fromUtf8(sourceText)) : QString();
        }
    } translator;
    QCoreApplication::installTranslator(&translator);
    SettingsDialog dialog;
    const auto* check = dialog.findChild<QCheckBox*>(QStringLiteral("settingsDefaultAllowInsecureCheck"));
    QVERIFY(check != nullptr);
    QVERIFY(check->toolTip().startsWith(QStringLiteral("[zh]")));
    QVERIFY(check->toolTip().contains(QStringLiteral("node")));
    QVERIFY(check->toolTip().contains(QStringLiteral("Xray")));
    QCoreApplication::removeTranslator(&translator);
}

void SettingsDialogTests::defaultUserAgentComboRoundTripsConfig()
{
    Config config;
    config.dns().defaultUserAgent = QStringLiteral("Mozilla/5.0");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* defaultUserAgentCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsDefaultUserAgentCombo"));
    QVERIFY(defaultUserAgentCombo != nullptr);
    QCOMPARE(defaultUserAgentCombo->currentText(), QStringLiteral("Mozilla/5.0"));

    defaultUserAgentCombo->setCurrentText(QStringLiteral("curl/8.0"));

    const Config updated = dialog.config();
    QCOMPARE(updated.dns().defaultUserAgent, QStringLiteral("curl/8.0"));
}

void SettingsDialogTests::defaultFingerprintComboRoundTripsConfig()
{
    Config config;
    config.dns().defaultFingerprint = QStringLiteral("firefox");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* defaultFingerprintCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsDefaultFingerprintCombo"));
    QVERIFY(defaultFingerprintCombo != nullptr);
    QCOMPARE(defaultFingerprintCombo->currentText(), QStringLiteral("firefox"));

    defaultFingerprintCombo->setCurrentText(QStringLiteral("edge"));

    const Config updated = dialog.config();
    QCOMPARE(updated.dns().defaultFingerprint, QStringLiteral("edge"));
}

// The endpoint the availability checks probe has existed in the config since it was added, but
// there was no field to set it -- so a user whose network blocks the built-in hosts had no way to
// point the check somewhere it works.
void SettingsDialogTests::speedPingTestUrlRoundTripsConfig()
{
    Config config;
    config.defaults().speedPingTestUrl = QStringLiteral("https://example.com/generate_204");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* speedPingTestUrlEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsSpeedPingTestUrlEdit"));
    QVERIFY(speedPingTestUrlEdit != nullptr);
    QCOMPARE(speedPingTestUrlEdit->text(), QStringLiteral("https://example.com/generate_204"));

    // Trimmed on the way back in: a stray space in a URL is a silent failure at probe time.
    speedPingTestUrlEdit->setText(QStringLiteral("  https://mirror.example/probe  "));

    const Config updated = dialog.config();
    QCOMPARE(updated.defaults().speedPingTestUrl, QStringLiteral("https://mirror.example/probe"));
}

// The advanced escape hatch past the startup outbound gate. Off by default; the point of the test is
// that the checkbox is wired to the config field the session actually reads.
void SettingsDialogTests::skipOutboundCheckRoundTripsConfig()
{
    Config config;
    QVERIFY(!config.skipOutboundCheck);

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* skipOutboundCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsSkipOutboundCheck"));
    QVERIFY(skipOutboundCheck != nullptr);
    QVERIFY(!skipOutboundCheck->isChecked());

    skipOutboundCheck->setChecked(true);
    QVERIFY(dialog.config().skipOutboundCheck);

    skipOutboundCheck->setChecked(false);
    QVERIFY(!dialog.config().skipOutboundCheck);
}

void SettingsDialogTests::themeComboRoundTripsConfig()
{
    Config config;
    config.ui().themeName = AppTheme::darkThemeName();

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* themeCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsThemeCombo"));
    QVERIFY(themeCombo != nullptr);
    QCOMPARE(themeCombo->currentData().toString(), AppTheme::darkThemeName());
    // "Follow system" leads the list so it is the visible default, with the two
    // explicit palettes after it. The stored preference round-trips through data,
    // not the index, so the order is a presentation choice.
    QCOMPARE(themeCombo->findData(AppTheme::systemThemeName()), 0);
    QCOMPARE(themeCombo->findData(AppTheme::lightThemeName()), 1);
    QCOMPARE(themeCombo->findData(AppTheme::darkThemeName()), 2);

    themeCombo->setCurrentIndex(themeCombo->findData(AppTheme::lightThemeName()));

    const Config updated = dialog.config();
    QCOMPARE(updated.ui().themeName, AppTheme::lightThemeName());
}

void SettingsDialogTests::themeComboStoresFollowSystemPreference()
{
    SettingsDialog dialog;

    auto* themeCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsThemeCombo"));
    QVERIFY(themeCombo != nullptr);

    themeCombo->setCurrentIndex(themeCombo->findData(AppTheme::systemThemeName()));
    // "System" survives the round-trip: it is a preference, not a palette, so it
    // must not be folded to Light the way an unrecognised value would be.
    QCOMPARE(dialog.config().ui().themeName, AppTheme::systemThemeName());

    Config config;
    config.ui().themeName = AppTheme::systemThemeName();
    dialog.setConfig(config);
    QCOMPARE(themeCombo->currentData().toString(), AppTheme::systemThemeName());
}

void SettingsDialogTests::tunSettingsRoundTripsConfig()
{
    Config config;
    config.tun().tunModeItem.enableTun = true;
    config.tun().tunModeItem.autoRoute = true;
    config.tun().tunModeItem.strictRoute = false;
    config.tun().tunModeItem.mtu = 1400;
    config.tun().tunModeItem.stack = QStringLiteral("gvisor");
    config.tun().tunModeItem.enableIPv6Address = true;
    config.tun().tunModeItem.icmpRouting = QStringLiteral("tcp");
    config.tun().tunModeItem.udpRouting = QStringLiteral("direct");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* enableCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunEnableCheck"));
    auto* autoRouteCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunAutoRouteCheck"));
    auto* strictRouteCheck = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunStrictRouteCheck"));
    auto* mtuSpin = dialog.findChild<QSpinBox*>(QStringLiteral("settingsTunMtuSpin"));
    auto* stackCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsTunStackCombo"));
    auto* ipv6Check = dialog.findChild<QCheckBox*>(QStringLiteral("settingsTunEnableIPv6AddressCheck"));
    auto* icmpCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsTunIcmpRoutingCombo"));
    auto* udpCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsTunUdpRoutingCombo"));
    QVERIFY(enableCheck != nullptr);
    QVERIFY(autoRouteCheck != nullptr);
    QVERIFY(strictRouteCheck != nullptr);
    QVERIFY(mtuSpin != nullptr);
    QVERIFY(stackCombo != nullptr);
    QVERIFY(ipv6Check != nullptr);
    QVERIFY(icmpCombo != nullptr);
    QVERIFY(udpCombo != nullptr);
    QVERIFY(enableCheck->isChecked());
    QVERIFY(autoRouteCheck->isEnabled());
    QCOMPARE(mtuSpin->value(), 1400);
    QCOMPARE(stackCombo->currentText(), QStringLiteral("gvisor"));

    mtuSpin->setValue(1500);
    stackCombo->setCurrentText(QStringLiteral("mixed"));
    icmpCombo->setCurrentText(QStringLiteral("udp"));
    udpCombo->setCurrentText(QStringLiteral("rule"));
    strictRouteCheck->setChecked(true);
    ipv6Check->setChecked(false);

    const Config updated = dialog.config();
    QVERIFY(updated.tun().tunModeItem.enableTun);
    QVERIFY(updated.tun().tunModeItem.autoRoute);
    QVERIFY(updated.tun().tunModeItem.strictRoute);
    QCOMPARE(updated.tun().tunModeItem.mtu, 1500);
    QCOMPARE(updated.tun().tunModeItem.stack, QStringLiteral("mixed"));
    QVERIFY(!updated.tun().tunModeItem.enableIPv6Address);
    QCOMPARE(updated.tun().tunModeItem.icmpRouting, QStringLiteral("udp"));
    QCOMPARE(updated.tun().tunModeItem.udpRouting, QStringLiteral("rule"));
}

void SettingsDialogTests::tunToggleControlsDependentFields()
{
    Config config;
    config.tun().tunModeItem.enableTun = false;

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
    config.tun().tunModeItem.enableTun = false;

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
        updated.policy().coreTypeItems.cbegin(),
        updated.policy().coreTypeItems.cend(),
        [](const CoreTypeItem& item) { return item.configType == static_cast<int>(ConfigType::HTTP); });
    QVERIFY(it != updated.policy().coreTypeItems.cend());
    QCOMPARE(it->coreType, static_cast<int>(CoreType::Xray));
}

void SettingsDialogTests::coreTypeTableUsesComboForSingleCoreProtocol()
{
    // AnyTLS is supported by sing-box only, but it must still render as a
    // combo box so every protocol row uses the same control type.
    SettingsDialog dialog;
    dialog.setConfig(Config());

    auto* anytlsCombo = dialog.findChild<QComboBox*>(
        QStringLiteral("coreTypeCombo_%1").arg(static_cast<int>(ConfigType::AnyTLS)));
    QVERIFY(anytlsCombo != nullptr);
    QCOMPARE(anytlsCombo->count(), 1);
    QCOMPARE(anytlsCombo->currentText(), QStringLiteral("sing-box"));

    const Config updated = dialog.config();
    auto it = std::find_if(
        updated.policy().coreTypeItems.cbegin(),
        updated.policy().coreTypeItems.cend(),
        [](const CoreTypeItem& item) { return item.configType == static_cast<int>(ConfigType::AnyTLS); });
    QVERIFY(it != updated.policy().coreTypeItems.cend());
    QCOMPARE(it->coreType, static_cast<int>(CoreType::SingBox));
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
    config.policy().coreTypeItems = {
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
    config.collection().customRoutingItems = {routing};

    SettingsDialog dialog;
    dialog.setConfig(config);

    const Config updated = dialog.config();
    QCOMPARE(updated.collection().customRoutingItems.size(), 1);
    QCOMPARE(updated.collection().customRoutingItems.constFirst().rules.size(), 1);
    const RoutingRule updatedRule = updated.collection().customRoutingItems.constFirst().rules.constFirst();
    QCOMPARE(updatedRule.network, QStringLiteral("tcp,udp"));
    QCOMPARE(
        updatedRule.process,
        QStringList({QStringLiteral("self/"), QStringLiteral("C:/Program Files/Test/test.exe")}));
}

void SettingsDialogTests::routingCustomRuleProcessRoundTripsThroughProcessEditor()
{
    Config config;
    RoutingRule proxyProcessRule;
    proxyProcessRule.type = QStringLiteral("field");
    proxyProcessRule.enabled = true;
    proxyProcessRule.outboundTag = QStringLiteral("proxy");
    proxyProcessRule.process = QStringList{
        QStringLiteral("chrome.exe"),
        QStringLiteral("C:/Program Files/Test/test.exe")};
    config.collection().routingCustomRules = {proxyProcessRule};

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* processEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomproxyProcessEdit"));
    QVERIFY(processEdit != nullptr);
    QCOMPARE(
        processEdit->toPlainText().trimmed(),
        QStringLiteral("chrome.exe\nC:/Program Files/Test/test.exe"));

    // The process column exists on every action tab, not just PROXY.
    QVERIFY(dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomblockProcessEdit")) != nullptr);
    QVERIFY(dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomdirectProcessEdit")) != nullptr);

    processEdit->setPlainText(QStringLiteral("firefox.exe"));

    const Config updated = dialog.config();
    QCOMPARE(updated.collection().routingCustomRules.size(), 1);
    QCOMPARE(updated.collection().routingCustomRules.constFirst().outboundTag, QStringLiteral("proxy"));
    QCOMPARE(
        updated.collection().routingCustomRules.constFirst().process,
        QStringList{QStringLiteral("firefox.exe")});
}

void SettingsDialogTests::routingCustomRuleWithMultipleFieldKindsIsPreservedVerbatim()
{
    // The custom-rule editor is a flat grid: one list per (action, field kind). A rule that
    // combines kinds cannot be expressed there, and flattening it would turn its AND into an
    // OR -- `{domain: X, process: Y}` would come back as two rules, widening what is routed.
    // Such a rule must therefore survive the partition/collect round trip untouched.
    const QStringList supportedActions{
        QStringLiteral("block"), QStringLiteral("direct"), QStringLiteral("proxy")};

    RoutingRule combined;
    combined.type = QStringLiteral("field");
    combined.enabled = true;
    combined.outboundTag = QStringLiteral("proxy");
    combined.domain = QStringList{QStringLiteral("domain:example.com")};
    combined.process = QStringList{QStringLiteral("chrome.exe")};

    const RoutingCustomRuleSupport::PartitionedRules partitioned =
        RoutingCustomRuleSupport::partitionEditableRules({combined}, supportedActions);
    QVERIFY(partitioned.valuesByAction.isEmpty());
    QCOMPARE(partitioned.preservedRules.size(), 1);

    const QList<RoutingRule> collected =
        RoutingCustomRuleSupport::collectRules(partitioned.preservedRules, partitioned.valuesByAction);
    QCOMPARE(collected.size(), 1);
    QCOMPARE(collected.constFirst().domain, combined.domain);
    QCOMPARE(collected.constFirst().process, combined.process);

    // Dropping one of the two kinds makes it expressible again, so it goes through the
    // editor rather than being preserved -- this is what keeps the process column usable.
    RoutingRule processOnly = combined;
    processOnly.domain.clear();
    const RoutingCustomRuleSupport::PartitionedRules single =
        RoutingCustomRuleSupport::partitionEditableRules({processOnly}, supportedActions);
    QCOMPARE(single.preservedRules.size(), 0);
    QCOMPARE(
        single.valuesByAction.value(QStringLiteral("proxy")).processes,
        QStringList{QStringLiteral("chrome.exe")});

    const QList<RoutingRule> singleCollected =
        RoutingCustomRuleSupport::collectRules(single.preservedRules, single.valuesByAction);
    QCOMPARE(singleCollected.size(), 1);
    QCOMPARE(singleCollected.constFirst().process, QStringList{QStringLiteral("chrome.exe")});
}

void SettingsDialogTests::routingCustomRuleWithMultipleFieldKindsRoundTripsThroughDialog()
{
    // Same guarantee as above, but through the real dialog so the fix is proven to reach the
    // path a user actually takes (open Settings, press OK).
    Config config;
    RoutingRule combined;
    combined.type = QStringLiteral("field");
    combined.enabled = true;
    combined.outboundTag = QStringLiteral("proxy");
    combined.domain = QStringList{QStringLiteral("domain:example.com")};
    combined.process = QStringList{QStringLiteral("chrome.exe")};
    config.collection().routingCustomRules = {combined};

    SettingsDialog dialog;
    dialog.setConfig(config);

    const Config updated = dialog.config();
    QCOMPARE(updated.collection().routingCustomRules.size(), 1);
    const RoutingRule result = updated.collection().routingCustomRules.constFirst();
    QCOMPARE(result.domain, combined.domain);
    QCOMPARE(result.process, combined.process);
}

namespace {

// A rule reduced to the fields the round trip is expected to preserve, so the assertions can
// compare whole sequences rather than one field at a time -- order is the thing under test here,
// and a per-field comparison would pass just as happily against a reordered list.
QStringList ruleSignatures(const QList<RoutingRule>& rules)
{
    QStringList signatures;
    for (const RoutingRule& rule : rules) {
        signatures.append(QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8")
                              .arg(rule.type,
                                   rule.enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"),
                                   rule.outboundTag,
                                   rule.protocol.join(QChar(',')),
                                   rule.port,
                                   rule.ip.join(QChar(',')),
                                   rule.domain.join(QChar(',')),
                                   rule.process.join(QChar(','))));
    }
    return signatures;
}

// One editable domain rule, one rule that combines two field kinds (so the grid cannot express it
// and has to preserve it), and one editable process rule -- all on the same action, so the only
// thing that can move is the preserved one.
QList<RoutingRule> mixedEditableAndPreservedRules()
{
    RoutingRule editableDomain;
    editableDomain.type = QStringLiteral("field");
    editableDomain.enabled = true;
    editableDomain.outboundTag = QStringLiteral("direct");
    editableDomain.domain = QStringList{QStringLiteral("domain:first.example.com")};

    RoutingRule preservedBoth;
    preservedBoth.type = QStringLiteral("field");
    preservedBoth.enabled = true;
    preservedBoth.outboundTag = QStringLiteral("direct");
    preservedBoth.domain = QStringList{QStringLiteral("domain:middle.example.com")};
    preservedBoth.process = QStringList{QStringLiteral("middle.exe")};

    RoutingRule editableProcess;
    editableProcess.type = QStringLiteral("field");
    editableProcess.enabled = true;
    editableProcess.outboundTag = QStringLiteral("direct");
    editableProcess.process = QStringList{QStringLiteral("last.exe")};

    return {editableDomain, preservedBoth, editableProcess};
}

} // namespace

void SettingsDialogTests::routingCustomRuleOrderSurvivesPartitionAndCollect()
{
    // The grid cannot express a rule that combines field kinds, so those are preserved verbatim
    // -- but they used to be re-emitted ahead of every editable rule. Route rules are matched in
    // order, so a preserved rule that sat between two editable ones started being evaluated
    // before the rule it used to follow.
    const QStringList supportedActions{
        QStringLiteral("block"), QStringLiteral("direct"), QStringLiteral("proxy")};

    const QList<RoutingRule> input = mixedEditableAndPreservedRules();
    const RoutingCustomRuleSupport::PartitionedRules partitioned =
        RoutingCustomRuleSupport::partitionEditableRules(input, supportedActions);

    QCOMPARE(partitioned.preservedRules.size(), 1);
    QCOMPARE(partitioned.preservedRules.constFirst().originalIndex, 1);
    QCOMPARE(partitioned.valuesByAction.value(QStringLiteral("direct")).domains,
             QStringList{QStringLiteral("domain:first.example.com")});

    const QList<RoutingRule> collected =
        RoutingCustomRuleSupport::collectRules(partitioned.preservedRules, partitioned.valuesByAction);
    QCOMPARE(ruleSignatures(collected), ruleSignatures(input));
}

void SettingsDialogTests::routingCustomRuleOrderSurvivesTheDialog()
{
    // Same guarantee as above, through the path a user actually takes (open Settings, press OK),
    // so the ordering fix is proven to reach the widget's own partition/collect pair.
    const QList<RoutingRule> input = mixedEditableAndPreservedRules();
    Config config;
    config.collection().routingCustomRules = input;

    SettingsDialog dialog;
    dialog.setConfig(config);

    const Config updated = dialog.config();
    QCOMPARE(ruleSignatures(updated.collection().routingCustomRules), ruleSignatures(input));
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
    config.collection().routingCustomRules = {blockRule, blockDomainRule, directRule, directIpRule, proxyRule};

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("routingCustomRuleTabs"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("BLOCK"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("DIRECT"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("PROXY"));

    auto* blockDomainEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomblockDomainEdit"));
    auto* directIpEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomdirectIpEdit"));
    auto* proxyProtocolEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomproxyProtocolEdit"));
    auto* proxyPortEdit = dialog.findChild<QLineEdit*>(QStringLiteral("routingCustomproxyPortEdit"));
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
    QCOMPARE(updated.collection().routingCustomRules.size(), 3);
    QCOMPARE(updated.collection().routingCustomRules.at(0).outboundTag, QStringLiteral("block"));
    QCOMPARE(updated.collection().routingCustomRules.at(0).domain, QStringList{QStringLiteral("domain:ads.example.com")});
    QCOMPARE(updated.collection().routingCustomRules.at(1).outboundTag, QStringLiteral("direct"));
    QCOMPARE(updated.collection().routingCustomRules.at(1).ip, QStringList{QStringLiteral("192.168.0.0/16")});
    QCOMPARE(updated.collection().routingCustomRules.at(2).outboundTag, QStringLiteral("proxy"));
    QCOMPARE(updated.collection().routingCustomRules.at(2).protocol, QStringList{QStringLiteral("bittorrent")});
    QCOMPARE(updated.collection().routingCustomRules.at(2).port, QStringLiteral("6881-6999"));
}

void SettingsDialogTests::routingPageReportsValuesNoCoreCanHonour()
{
    Config config;
    RoutingRule directRule;
    directRule.type = QStringLiteral("field");
    directRule.enabled = true;
    directRule.outboundTag = QStringLiteral("direct");
    directRule.domain = QStringList{QStringLiteral("domain:example.com")};
    config.collection().routingCustomRules = {directRule};

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* warnings = dialog.findChild<QLabel*>(QStringLiteral("routingCustomRuleWarnings"));
    auto* directDomainEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomdirectDomainEdit"));
    auto* directIpEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomdirectIpEdit"));
    QVERIFY(warnings != nullptr);
    QVERIFY(directDomainEdit != nullptr);
    QVERIFY(directIpEdit != nullptr);

    // Every value here is honoured by every core, so nothing is reported. Asserted with
    // isHidden() rather than !isVisible(): the dialog is never shown, so a visible widget would
    // still report isVisible() == false.
    QVERIFY(warnings->isHidden());
    QVERIFY(warnings->text().isEmpty());

    // A "word:" prefix no core defines is reported rather than reaching the generated config as
    // a keyword rule matching its own prefix text, which could never match a host.
    directDomainEdit->setPlainText(QStringLiteral("nosuchprefix:example.com"));
    QVERIFY(!warnings->isHidden());
    QVERIFY(warnings->text().contains(QStringLiteral("nosuchprefix:")));

    // So is a prefix the ip field does not accept.
    directDomainEdit->setPlainText(QStringLiteral("domain:example.com"));
    directIpEdit->setPlainText(QStringLiteral("keyword:1.2.3.4"));
    QVERIFY(!warnings->isHidden());
    QVERIFY(warnings->text().contains(QStringLiteral("keyword:1.2.3.4")));

    // Clearing the offending value clears the warning again.
    directIpEdit->setPlainText(QStringLiteral("geoip:cn"));
    QVERIFY(warnings->isHidden());
    QVERIFY(warnings->text().isEmpty());
}

void SettingsDialogTests::routingCustomRuleTabsDefaultToDirectAndPersistSelection()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("routingCustomRuleTabs"));
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("DIRECT"));
    QCOMPARE(dialog.config().ui().settingsRoutingRuleTabKey, QStringLiteral("direct"));

    Config config;
    config.ui().settingsRoutingRuleTabKey = QStringLiteral("proxy");
    dialog.setConfig(config);
    QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("PROXY"));

    int blockIndex = -1;
    for (int index = 0; index < tabs->count(); ++index) {
        if (tabs->tabText(index) == QStringLiteral("BLOCK")) {
            blockIndex = index;
            break;
        }
    }
    QVERIFY(blockIndex >= 0);
    tabs->setCurrentIndex(blockIndex);
    QCOMPARE(dialog.config().ui().settingsRoutingRuleTabKey, QStringLiteral("block"));
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
    config.collection().customRoutingItems = {routing};
    config.collection().routingModeId = QStringLiteral("custom:1");

    SettingsDialog dialog;
    dialog.setConfig(config);

    const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
    for (QLabel* label : labels) {
        QVERIFY(label->text() != QStringLiteral("Custom rules are applied first in this order: block, direct, proxy, then the selected base route."));
    }

    auto* card = dialog.findChild<QPushButton*>(QStringLiteral("routingBaseRouteCard_3"));
    QVERIFY(card != nullptr);
    QVERIFY(card->styleSheet().isEmpty());
    QVERIFY(card->text().contains(QStringLiteral("BLOCK: geosite:category-ads-all")));
    QVERIFY(card->text().contains(QStringLiteral("DIRECT: geoip:private")));
    QVERIFY(card->text().contains(QStringLiteral("PROXY: Port: 443, Protocol: tls")));
    QVERIFY(card->toolTip().contains(QStringLiteral("BLOCK: geosite:category-ads-all, domain:very-long-domain-name-that-should-not-expand-the-card-width.example.com")));
    QVERIFY(card->toolTip().contains(QStringLiteral("DIRECT: geoip:private")));
    QVERIFY(card->toolTip().contains(QStringLiteral("PROXY: Port: 443, Protocol: tls")));
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
    QVERIFY(dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomproxyProtocolEdit")) != nullptr);
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
        config.collection().customRoutingItems.append(item);
    }
    config.collection().routingModeId = QStringLiteral("custom:1");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* first = dialog.findChild<QPushButton*>(QStringLiteral("routingBaseRouteCard_3"));
    auto* second = dialog.findChild<QPushButton*>(QStringLiteral("routingBaseRouteCard_4"));
    auto* third = dialog.findChild<QPushButton*>(QStringLiteral("routingBaseRouteCard_5"));
    QVERIFY(first != nullptr);
    QVERIFY(second != nullptr);
    QVERIFY(third != nullptr);

    auto* routeLayout = dialog.findChild<QHBoxLayout*>(QStringLiteral("routingBaseRouteLayout"));
    QVERIFY(routeLayout != nullptr);
    QCOMPARE(routeLayout->count(), 6);
    QCOMPARE(routeLayout->stretch(3), 1);
    QCOMPARE(routeLayout->stretch(4), 0);
    QCOMPARE(routeLayout->stretch(5), 0);

    QCOMPARE(first->maximumWidth(), QWIDGETSIZE_MAX);
    QCOMPARE(second->maximumWidth(), 100);
    QCOMPARE(third->maximumWidth(), 100);
    QCOMPARE(first->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(second->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);

    third->click();

    QCOMPARE(first->maximumWidth(), 100);
    QCOMPARE(second->maximumWidth(), 100);
    QCOMPARE(third->maximumWidth(), QWIDGETSIZE_MAX);
    QCOMPARE(routeLayout->count(), 6);
    QCOMPARE(routeLayout->stretch(3), 0);
    QCOMPARE(routeLayout->stretch(4), 0);
    QCOMPARE(routeLayout->stretch(5), 1);
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

    auto* protocolEdit = dialog.findChild<QTextEdit*>(QStringLiteral("routingCustomblockProtocolEdit"));
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

    QCOMPARE(baseRouteTitle->text(), QStringLiteral("Base Route"));
    QCOMPARE(customRulesTitle->text(), QStringLiteral("Custom Rules"));
    QVERIFY(baseRouteTitle->toolTip().isEmpty());
    QCOMPARE(customRuleTabs->toolTip(), customRulesTitle->toolTip());

    Config config;
    RoutingItem routing;
    routing.remarks = QStringLiteral("Builtin");
    config.collection().customRoutingItems = {routing};
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
    auto* languageCombo = dialog.findChild<QComboBox*>(QStringLiteral("settingsLanguageCombo"));
    QVERIFY(settingsTabBar != nullptr);
    QVERIFY(subTable != nullptr);
    QVERIFY(languageCombo != nullptr);

    QVERIFY(settingsTabBar->font().pointSizeF() <= baseline);
    QVERIFY(subTable->font().pointSizeF() <= baseline);
    QVERIFY(languageCombo->font().pointSizeF() <= baseline);
}

void SettingsDialogTests::fakeIpToggleControlsGlobalFakeIpCheckbox()
{
    Config config;
    config.dns().fakeIp = false;
    config.dns().globalFakeIp = true;

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
    config.dns().remoteDns = QStringLiteral("https://one.example/dns-query");
    config.dns().directDns = QStringLiteral("https://two.example/dns-query");
    config.dns().bootstrapDns = QStringLiteral("1.1.1.1");
    config.dns().proxyServerDns = QStringLiteral("https://node-dns.example/dns-query");
    config.dns().domainStrategyForFreedom = QStringLiteral("UseIPv6");
    config.dns().domainStrategyForProxy = QStringLiteral("UseIPv4");
    config.dns().useSystemHosts = true;
    config.dns().addCommonHosts = true;
    config.dns().blockBindingQuery = true;
    config.dns().fakeIp = true;
    config.dns().globalFakeIp = true;
    config.dns().serveStale = true;
    config.dns().parallelQuery = true;
    config.dns().directExpectedIps = QStringLiteral("geoip:private");
    config.dns().dnsHosts = QStringLiteral("example.com 1.2.3.4");

    SettingsDialog dialog;
    dialog.setConfig(config);

    auto* remoteDnsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsRemoteDnsEdit"));
    auto* proxyServerDnsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsProxyServerDnsEdit"));
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
    QVERIFY(proxyServerDnsEdit != nullptr);
    QVERIFY(freedomCombo != nullptr);
    QVERIFY(proxyCombo != nullptr);

    // Xray's routing-only values must not be on offer: this combo's value is written into the
    // outbound strategy verbatim, where Xray does not accept them.
    QCOMPARE(freedomCombo->findText(QStringLiteral("IPIfNonMatch")), -1);
    QCOMPARE(freedomCombo->findText(QStringLiteral("IPOnDemand")), -1);
    QVERIFY(freedomCombo->findText(QStringLiteral("UseIPv4v6")) >= 0);
    QVERIFY(directExpectedIpsEdit != nullptr);
    QVERIFY(dnsHostsEdit != nullptr);
    QVERIFY(useSystemHostsCheck != nullptr);
    QVERIFY(addCommonHostsCheck != nullptr);
    QVERIFY(blockBindingQueryCheck != nullptr);
    QVERIFY(fakeIpCheck != nullptr);
    QVERIFY(globalFakeIpCheck != nullptr);

    QCOMPARE(remoteDnsEdit->text(), QStringLiteral("https://one.example/dns-query"));
    QCOMPARE(proxyServerDnsEdit->text(), QStringLiteral("https://node-dns.example/dns-query"));
    QCOMPARE(freedomCombo->currentText(), QStringLiteral("UseIPv6"));
    QCOMPARE(proxyCombo->currentText(), QStringLiteral("UseIPv4"));
    QVERIFY(useSystemHostsCheck->isChecked());
    QVERIFY(addCommonHostsCheck->isChecked());
    QVERIFY(blockBindingQueryCheck->isChecked());
    QVERIFY(fakeIpCheck->isChecked());
    QVERIFY(globalFakeIpCheck->isChecked());
    QCOMPARE(directExpectedIpsEdit->text(), QStringLiteral("geoip:private"));
    QCOMPARE(dnsHostsEdit->toPlainText(), QStringLiteral("example.com 1.2.3.4"));

    remoteDnsEdit->setText(QStringLiteral("https://changed.example/dns-query"));
    proxyServerDnsEdit->setText(QStringLiteral("https://changed-node.example/dns-query"));
    freedomCombo->setCurrentText(QStringLiteral("ForceIPv4"));
    proxyCombo->setCurrentText(QStringLiteral("ForceIPv6"));
    useSystemHostsCheck->setChecked(false);
    addCommonHostsCheck->setChecked(false);
    blockBindingQueryCheck->setChecked(false);
    directExpectedIpsEdit->setText(QStringLiteral("1.2.3.0/24"));
    dnsHostsEdit->setPlainText(QStringLiteral("full:full.example.com 5.6.7.8"));

    const Config updated = dialog.config();
    QCOMPARE(updated.dns().remoteDns, QStringLiteral("https://changed.example/dns-query"));
    QCOMPARE(updated.dns().proxyServerDns, QStringLiteral("https://changed-node.example/dns-query"));
    QCOMPARE(updated.dns().domainStrategyForFreedom, QStringLiteral("ForceIPv4"));
    QCOMPARE(updated.dns().domainStrategyForProxy, QStringLiteral("ForceIPv6"));
    QVERIFY(!updated.dns().useSystemHosts);
    QVERIFY(!updated.dns().addCommonHosts);
    QVERIFY(!updated.dns().blockBindingQuery);
    QCOMPARE(updated.dns().directExpectedIps, QStringLiteral("1.2.3.0/24"));
    QCOMPARE(updated.dns().dnsHosts, QStringLiteral("full:full.example.com 5.6.7.8"));
}

// A QFormLayout spends a row's surplus height on the rows it thinks are resizable, and the
// hosts editor used to be capped at 120px. That made every gap on this page 35-37px while
// General and TUN read 24-26px, and the leftover space never reached the bottom. The box now
// expands instead, so the rhythm matches and the hosts text gets the room. This measures the
// row pitch on both pages, because a comment cannot catch the next person who adds a
// maximumHeight back.
void SettingsDialogTests::dnsPageKeepsTheSameRowRhythmAsTheOtherFormPages()
{
    SettingsDialog dialog;
    dialog.selectTab(5);
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    auto* hostsEdit = dialog.findChild<QTextEdit*>(QStringLiteral("settingsDnsHostsEdit"));
    QVERIFY(hostsEdit != nullptr);
    QVERIFY(hostsEdit->maximumHeight() == QWIDGETSIZE_MAX);
    QCOMPARE(hostsEdit->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);

    // The filler carries the leftover once the hosts box has grown, so it must stay
    // zero-height and expanding or the slack comes back as row gaps.
    auto* filler = hostsEdit->parentWidget()->findChild<QWidget*>(QStringLiteral("settingsBottomFiller"));
    QVERIFY(filler != nullptr);
    QCOMPARE(filler->height(), 0);
    QCOMPARE(filler->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);

    QWidget* dnsPage = hostsEdit->parentWidget();
    auto* remoteDnsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsRemoteDnsEdit"));
    auto* directDnsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsDirectDnsEdit"));
    QVERIFY(remoteDnsEdit != nullptr);
    QVERIFY(directDnsEdit != nullptr);
    const int dnsPitch = directDnsEdit->mapTo(dnsPage, QPoint(0, 0)).y()
                       - remoteDnsEdit->mapTo(dnsPage, QPoint(0, 0)).y();

    SettingsDialog generalDialog;
    generalDialog.selectTab(0);
    generalDialog.setConfig(Config());
    generalDialog.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    auto* generalPortSpin = generalDialog.findChild<QSpinBox*>(QStringLiteral("settingsLocalPortSpin"));
    auto* generalLanguageCombo = generalDialog.findChild<QComboBox*>(QStringLiteral("settingsLanguageCombo"));
    QVERIFY(generalPortSpin != nullptr);
    QVERIFY(generalLanguageCombo != nullptr);
    QWidget* generalPage = generalPortSpin->parentWidget();
    const int generalPitch = generalLanguageCombo->mapTo(generalPage, QPoint(0, 0)).y()
                           - generalPortSpin->mapTo(generalPage, QPoint(0, 0)).y();

    // The pitch is the row height plus the layout's spacing, and the row height is the
    // taller of the label and the field. QLineEdit, QSpinBox and QComboBox do not share a
    // height on every platform -- on the macOS Qt5 style the line-edit rows read 2px taller
    // than the spin/combo rows -- so comparing raw pitches only holds on the platform the
    // tolerance was measured on. Subtracting the field height leaves the spacing (plus any
    // label surplus) that the rhythm is actually about, which is what the original bug
    // inflated.
    const int dnsGap = dnsPitch - remoteDnsEdit->height();
    const int generalGap = generalPitch - generalPortSpin->height();

    qWarning("row rhythm: dns=%d general=%d (gap %d vs %d)", dnsPitch, generalPitch, dnsGap, generalGap);
    // Before the fix the DNS page measured 35-37 here and this comparison was off by 9-11px.
    QVERIFY2(std::abs(dnsGap - generalGap) <= 1,
             qPrintable(QStringLiteral("DNS row gap %1 does not match General %2")
                            .arg(dnsGap)
                            .arg(generalGap)));

    // The hosts editor must reach the bottom rather than leave a dead void behind it.
    const int hostsBottom = hostsEdit->mapTo(dnsPage, QPoint(0, 0)).y() + hostsEdit->height();
    QVERIFY2(dnsPage->height() - hostsBottom <= 16,
             qPrintable(QStringLiteral("DNS page leaves %1px of unused space below the hosts editor")
                            .arg(dnsPage->height() - hostsBottom)));
}


// The DNS page used to show every option to every user, including six that only one core
// reads: the sing-box ones (Proxy Server DNS / Block binding query / Global FakeIP) and the
// Xray ones (Serve stale / Parallel query / Direct Expected IPs). They now sit on the Core
// page under the core that honours them, next to the sing-box and Xray options that were
// already there. This pins that split so a future edit cannot quietly move them back.
void SettingsDialogTests::corePageOwnsTheSingleCoreDnsOptions()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());

    auto* coreTabs = dialog.findChild<QTabWidget*>(QStringLiteral("coreDetailTabs"));
    QVERIFY(coreTabs != nullptr);

    // Each core's options live on the tab page that also carries that core's Install button.
    const auto pageForCore = [coreTabs](CoreType core) -> QWidget* {
        auto* button = coreTabs->findChild<QPushButton*>(
            QStringLiteral("coreDownloadButton_%1").arg(static_cast<int>(core)));
        if (button == nullptr) {
            return nullptr;
        }
        for (int index = 0; index < coreTabs->count(); ++index) {
            if (coreTabs->widget(index)->isAncestorOf(button)) {
                return coreTabs->widget(index);
            }
        }
        return nullptr;
    };

    QWidget* singBoxPage = pageForCore(CoreType::SingBox);
    QWidget* xrayPage = pageForCore(CoreType::Xray);
    QVERIFY(singBoxPage != nullptr);
    QVERIFY(xrayPage != nullptr);

    const auto onPage = [](QWidget* page, QWidget* widget) {
        return page != nullptr && widget != nullptr && page->isAncestorOf(widget);
    };

    auto* proxyServerDns = dialog.findChild<QLineEdit*>(QStringLiteral("settingsProxyServerDnsEdit"));
    auto* blockBinding = dialog.findChild<QCheckBox*>(QStringLiteral("settingsBlockBindingQueryCheck"));
    auto* globalFakeIp = dialog.findChild<QCheckBox*>(QStringLiteral("dnsGlobalFakeIpCheck"));
    auto* serveStale = dialog.findChild<QCheckBox*>(QStringLiteral("settingsServeStaleCheck"));
    auto* parallelQuery = dialog.findChild<QCheckBox*>(QStringLiteral("settingsParallelQueryCheck"));
    auto* expectedIps = dialog.findChild<QLineEdit*>(QStringLiteral("settingsDirectExpectedIpsEdit"));
    QVERIFY(proxyServerDns != nullptr);
    QVERIFY(blockBinding != nullptr);
    QVERIFY(globalFakeIp != nullptr);
    QVERIFY(serveStale != nullptr);
    QVERIFY(parallelQuery != nullptr);
    QVERIFY(expectedIps != nullptr);

    // The sing-box rows live under the sing-box page, the Xray rows under the Xray page.
    QVERIFY(onPage(singBoxPage, proxyServerDns));
    QVERIFY(onPage(singBoxPage, blockBinding));
    QVERIFY(onPage(singBoxPage, globalFakeIp));
    QVERIFY(onPage(xrayPage, serveStale));
    QVERIFY(onPage(xrayPage, parallelQuery));
    QVERIFY(onPage(xrayPage, expectedIps));

    // FakeIP stayed shared -- two cores read it -- so its checkbox is still the DNS page's.
    auto* fakeIp = dialog.findChild<QCheckBox*>(QStringLiteral("dnsFakeIpCheck"));
    QVERIFY(fakeIp != nullptr);
    QVERIFY(!onPage(singBoxPage, fakeIp));
    QVERIFY(!onPage(xrayPage, fakeIp));
}

void SettingsDialogTests::subscriptionTableUsesServerTableStyle()
{
    AppTheme::applyApplicationTheme(*qApp, AppTheme::lightThemeName());

    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();

    auto* subTable = dialog.findChild<QTableWidget*>();
    QVERIFY(subTable != nullptr);

    QVERIFY(subTable->property("serverTableStyle").toBool());
    QCOMPARE(
        subTable->palette().color(QPalette::AlternateBase),
        QColor(AppTheme::tableAlternateBaseColor(AppTheme::lightThemeName())));
    QVERIFY(!subTable->showGrid());
}

void SettingsDialogTests::subscriptionTableUsesDarkThemePalette()
{
    AppTheme::applyApplicationTheme(*qApp, AppTheme::darkThemeName());

    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();

    auto* subTable = dialog.findChild<QTableWidget*>();
    QVERIFY(subTable != nullptr);

    QCOMPARE(
        subTable->palette().color(QPalette::AlternateBase),
        QColor(AppTheme::tableAlternateBaseColor(AppTheme::darkThemeName())));

    AppTheme::applyApplicationTheme(*qApp, AppTheme::lightThemeName());
}

void SettingsDialogTests::serverTableDarkThemePaintsAlternateRowsDark()
{
    struct ThemeResetter {
        ~ThemeResetter()
        {
            AppTheme::applyApplicationTheme(*qApp, AppTheme::lightThemeName());
        }
    } resetter;

    AppTheme::applyApplicationTheme(*qApp, AppTheme::darkThemeName());

    QTableWidget table(2, 1);
    table.setObjectName(QStringLiteral("serverTableView"));
    table.setItem(0, 0, new QTableWidgetItem(QStringLiteral("First")));
    table.setItem(1, 0, new QTableWidgetItem(QStringLiteral("Second")));
    table.horizontalHeader()->hide();
    table.verticalHeader()->hide();
    table.setColumnWidth(0, 180);
    table.setRowHeight(0, 28);
    table.setRowHeight(1, 28);
    table.setFixedSize(180, 56);
    AppTheme::applyServerTableStyle(&table);

    table.show();
    QCoreApplication::processEvents();

    const QImage rendered = table.viewport()->grab().toImage();
    QVERIFY(!rendered.isNull());

    const QModelIndex alternateIndex = table.model()->index(1, 0);
    const QRect alternateRect = table.visualRect(alternateIndex);
    QVERIFY(alternateRect.isValid());

    const QPoint samplePoint(alternateRect.right() - 8, alternateRect.center().y());
    const QColor alternatePixel = rendered.pixelColor(samplePoint);
    const QColor expectedAlternate(AppTheme::tableAlternateBaseColor(AppTheme::darkThemeName()));
    QCOMPARE(alternatePixel, expectedAlternate);
}

void SettingsDialogTests::applicationThemeLoadsResourceStyleSheet()
{
    AppTheme::applyApplicationTheme(*qApp, AppTheme::darkThemeName());

    QVERIFY(qApp->styleSheet().contains(QStringLiteral("QMainWindow")));
    QVERIFY(qApp->styleSheet().contains(QStringLiteral("#20262d")));
    QVERIFY(!qApp->styleSheet().contains(QStringLiteral("@surface@")));

    AppTheme::applyApplicationTheme(*qApp, AppTheme::lightThemeName());
}

void SettingsDialogTests::subscriptionPageRoundTripsSelectedRowsAndItems()
{
    Config config;
    config.collection().subscriptions = {
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
    QCOMPARE(updated.collection().subscriptions.size(), 2);
    QCOMPARE(updated.collection().subscriptions.at(0).id, QStringLiteral("sub-1"));
    QCOMPARE(updated.collection().subscriptions.at(0).remarks, QStringLiteral("A"));
    QCOMPARE(updated.collection().subscriptions.at(1).id, QStringLiteral("sub-2"));
    QCOMPARE(updated.collection().subscriptions.at(1).url, QStringLiteral("https://b.example"));
    QVERIFY(!updated.collection().subscriptions.at(1).enabled);
}

void SettingsDialogTests::subscriptionPageNewRowDefaultsEmptyUserAgentAndAllowsCustomInput()
{
    SettingsDialog dialog;
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();

    const auto addButtons = dialog.findChildren<QPushButton*>();
    QPushButton* addButton = nullptr;
    for (QPushButton* button : addButtons) {
        if (button != nullptr && button->text() == QStringLiteral("Add")) {
            addButton = button;
            break;
        }
    }
    QVERIFY(addButton != nullptr);
    addButton->click();
    QCoreApplication::processEvents();

    auto* subTable = dialog.findChild<QTableWidget*>();
    QVERIFY(subTable != nullptr);
    QCOMPARE(subTable->rowCount(), 1);

    auto* userAgentCombo = qobject_cast<QComboBox*>(subTable->cellWidget(0, 3));
    QVERIFY(userAgentCombo != nullptr);
    QCOMPARE(userAgentCombo->currentText(), QString());
    QVERIFY(userAgentCombo->isEditable());
    QCOMPARE(userAgentCombo->findText(QStringLiteral("clash verge rev")), 2);
    QCOMPARE(userAgentCombo->findText(QStringLiteral("loon")), 8);
    QCOMPARE(userAgentCombo->findText(QStringLiteral("mihomo")), 9);
    QCOMPARE(userAgentCombo->findText(QStringLiteral("flclash")), 11);
    QCOMPARE(userAgentCombo->findText(QStringLiteral("nekobox android")), 15);
    QCOMPARE(userAgentCombo->findText(QStringLiteral("hiddifynext")), 16);
    QCOMPARE(userAgentCombo->objectName(), QStringLiteral("uaCombo"));
    QVERIFY(userAgentCombo->styleSheet().isEmpty());
    QVERIFY(userAgentCombo->lineEdit() != nullptr);
    QCOMPARE(userAgentCombo->lineEdit()->objectName(), QStringLiteral("uaComboLineEdit"));
    QVERIFY(userAgentCombo->lineEdit()->styleSheet().isEmpty());

    QCOMPARE(SubscriptionSettingsPageWidget::resolveUserAgent(userAgentCombo->currentText()),
             QStringLiteral("mihomo/1.19.28"));
    QCOMPARE(SubscriptionSettingsPageWidget::resolveUserAgent(QStringLiteral("v2rayn")),
             QStringLiteral("v2rayN/7.10.4"));
    QCOMPARE(SubscriptionSettingsPageWidget::resolveUserAgent(QStringLiteral("mihomo")),
             QStringLiteral("mihomo/1.19.28"));
    QCOMPARE(SubscriptionSettingsPageWidget::resolveUserAgent(QStringLiteral("clash meta")),
             QStringLiteral("ClashMeta"));
    auto* urlItem = subTable->item(0, 1);
    QVERIFY(urlItem != nullptr);
    urlItem->setText(QStringLiteral("https://example.com/sub"));
    userAgentCombo->setEditText(QStringLiteral("CustomUA/9.9"));

    const Config updated = dialog.config();
    QCOMPARE(updated.collection().subscriptions.size(), 1);
    QCOMPARE(updated.collection().subscriptions.at(0).userAgent, QStringLiteral("CustomUA/9.9"));
    QCOMPARE(updated.collection().subscriptions.at(0).url, QStringLiteral("https://example.com/sub"));
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

// The three DNS address fields are not three interchangeable servers, and Bootstrap DNS only
// has a job when one of the two above it is addressed by hostname. The page now says so in the
// placeholder instead of leaving the user to work out why an apparently identical trio exists:
// with IP addresses it reads "not needed", and it asks for a value as soon as a hostname shows
// up. Default Direct DNS is a hostname, so the shipped state legitimately still asks for one.
void SettingsDialogTests::dnsPageBootstrapHintFollowsTheAddressesAboveIt()
{
    SettingsDialog dialog;
    dialog.selectTab(5);
    dialog.setConfig(Config());
    dialog.show();
    QCoreApplication::processEvents();

    auto* remoteDnsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsRemoteDnsEdit"));
    auto* directDnsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsDirectDnsEdit"));
    auto* bootstrapDnsEdit = dialog.findChild<QLineEdit*>(QStringLiteral("settingsBootstrapDnsEdit"));
    QVERIFY(remoteDnsEdit != nullptr);
    QVERIFY(directDnsEdit != nullptr);
    QVERIFY(bootstrapDnsEdit != nullptr);

    // Both two servers are addressed by IP, so Bootstrap has nothing to do.
    remoteDnsEdit->setText(QStringLiteral("https://1.1.1.1/dns-query"));
    directDnsEdit->setText(QStringLiteral("https://223.5.5.5/dns-query"));
    QVERIFY(bootstrapDnsEdit->placeholderText().startsWith(QStringLiteral("Not needed")));

    // A hostname in either field brings the need back.
    remoteDnsEdit->setText(QStringLiteral("https://dns.example.com/dns-query"));
    QVERIFY(!bootstrapDnsEdit->placeholderText().startsWith(QStringLiteral("Not needed")));
    remoteDnsEdit->setText(QStringLiteral("https://1.1.1.1/dns-query"));
    QVERIFY(bootstrapDnsEdit->placeholderText().startsWith(QStringLiteral("Not needed")));

    directDnsEdit->setText(QStringLiteral("https://doh.pub/dns-query"));
    QVERIFY(!bootstrapDnsEdit->placeholderText().startsWith(QStringLiteral("Not needed")));
    directDnsEdit->setText(QStringLiteral("223.5.5.5"));
    QVERIFY(bootstrapDnsEdit->placeholderText().startsWith(QStringLiteral("Not needed")));

    // The role of each field has to be discoverable without reading the source.
    QVERIFY(!remoteDnsEdit->toolTip().isEmpty());
    QVERIFY(!directDnsEdit->toolTip().isEmpty());
    QVERIFY(!bootstrapDnsEdit->toolTip().isEmpty());
    QVERIFY(remoteDnsEdit->toolTip() != directDnsEdit->toolTip());
    QVERIFY(remoteDnsEdit->toolTip() != bootstrapDnsEdit->toolTip());
    QVERIFY(directDnsEdit->toolTip() != bootstrapDnsEdit->toolTip());
}

QTEST_MAIN(SettingsDialogTests)

#include "SettingsDialogTests.moc"
