#include "ui/dialogs/SettingsDialog.h"
#include "runtime/ProtocolCoreCompat.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QItemSelectionModel>

#include <QWidget>

#include <algorithm>

namespace {

QStringList singBoxMuxProtocolOptions()
{
    return {
        QStringLiteral("h2mux"),
        QStringLiteral("smux"),
        QStringLiteral("yamux"),
        QString()};
}

QStringList defaultUserAgentOptions()
{
    return {
        QString(),
        QStringLiteral("chrome"),
        QStringLiteral("firefox"),
        QStringLiteral("edge"),
        QStringLiteral("golang")};
}

QStringList defaultFingerprintOptions()
{
    return {
        QStringLiteral("chrome"),
        QStringLiteral("firefox"),
        QStringLiteral("safari"),
        QStringLiteral("ios"),
        QStringLiteral("android"),
        QStringLiteral("edge"),
        QStringLiteral("360"),
        QStringLiteral("qq"),
        QStringLiteral("random"),
        QStringLiteral("randomized"),
        QString()};
}

constexpr int RuleNetworkRole = Qt::UserRole + 1;
constexpr int RuleProcessRole = Qt::UserRole + 2;

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    updateFieldState();
}

void SettingsDialog::setConfig(const Config& config)
{
    config_ = config;

    showMainOnStartupCheck_->setChecked(config.showMainOnStartup);
    autoRunCheck_->setChecked(config.autoRunEnabled);
    allowLanConnectionCheck_->setChecked(config.allowLanConnection);
    sniffingEnabledCheck_->setChecked(config.sniffingEnabled);
    routeOnlyCheck_->setChecked(config.routeOnly);
    localPortSpin_->setValue(config.localPort > 0 ? config.localPort : 10808);
    enableFragmentCheck_->setChecked(config.enableFragment);
    enableCacheFile4SboxCheck_->setChecked(config.enableCacheFile4Sbox);
    defaultAllowInsecureCheck_->setChecked(config.defaultAllowInsecure);
    defaultFingerprintCombo_->setCurrentText(config.defaultFingerprint);
    defaultUserAgentCombo_->setCurrentText(config.defaultUserAgent);
    const int muxProtocolIndex = mux4SboxProtocolCombo_->findText(config.mux4SboxProtocol);
    mux4SboxProtocolCombo_->setCurrentIndex(muxProtocolIndex >= 0 ? muxProtocolIndex : 0);
    enableStatisticsCheck_->setChecked(config.enableStatistics);
    statisticsFreshRateSpin_->setValue(qMax(1, config.statisticsFreshRate));
    trayMenuServersLimitSpin_->setValue(qMax(0, config.trayMenuServersLimit));
    systemProxyExceptionsEdit_->setText(config.systemProxyExceptions);
    systemProxyAdvancedProtocolCombo_->setCurrentText(config.systemProxyAdvancedProtocol);
    pacUrlEdit_->setText(config.pacUrl);
    autoUpdateIntervalSpin_->setValue(qMax(0, config.autoUpdateInterval));
    autoUpdateSubIntervalSpin_->setValue(qMax(0, config.autoUpdateSubInterval));
    checkPreReleaseUpdateCheck_->setChecked(config.checkPreReleaseUpdate);
    ignoreGeoUpdateCoreCheck_->setChecked(config.ignoreGeoUpdateCore);
    enableSecurityProtocolTls13Check_->setChecked(config.enableSecurityProtocolTls13);

    const QString normalizedLanguage = config.languageCode.trimmed();
    const int languageIndex = languageCombo_->findData(normalizedLanguage);
    languageCombo_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);

    routingItems_ = config.routingItems;
    reloadRoutingTable(findInitialRouteIndex(routingItems_, config));

    subItems_ = config.subscriptions;
    reloadSubTable();

    coreTypeItems_ = config.coreTypeItems;
    reloadCoreTypeTable();

    const TunModeItem& tun = config.tunModeItem;
    tunEnableCheck_->setChecked(tun.enableTun);
    tunAutoRouteCheck_->setChecked(tun.autoRoute);
    tunStrictRouteCheck_->setChecked(tun.strictRoute);
    tunMtuSpin_->setValue(tun.mtu > 0 ? tun.mtu : 9000);
    tunStackCombo_->setCurrentText(tun.stack.isEmpty() ? QStringLiteral("system") : tun.stack);
    tunEnableIPv6AddressCheck_->setChecked(tun.enableIPv6Address);
    tunIcmpRoutingEdit_->setText(tun.icmpRouting);
    tunEnableLegacyProtectCheck_->setChecked(tun.enableLegacyProtect);

    // DNS tab
    remoteDnsEdit_->setText(config.remoteDns);
    directDnsEdit_->setText(config.directDns);
    bootstrapDnsEdit_->setText(config.bootstrapDns);
    domainStrategyForFreedomCombo_->setCurrentText(config.domainStrategyForFreedom);
    domainStrategyForProxyCombo_->setCurrentText(config.domainStrategyForProxy);
    useSystemHostsCheck_->setChecked(config.useSystemHosts);
    addCommonHostsCheck_->setChecked(config.addCommonHosts);
    blockBindingQueryCheck_->setChecked(config.blockBindingQuery);
    fakeIpCheck_->setChecked(config.fakeIp);
    globalFakeIpCheck_->setChecked(config.globalFakeIp);
    serveStaleCheck_->setChecked(config.serveStale);
    parallelQueryCheck_->setChecked(config.parallelQuery);
    directExpectedIpsEdit_->setText(config.directExpectedIps);
    dnsHostsEdit_->setPlainText(config.dnsHosts);

    tabWidget_->setCurrentIndex(requestedTabIndex_);
    requestedTabIndex_ = 0;
    updateFieldState();
}

Config SettingsDialog::config() const
{
    Config updated = config_;
    updated.showMainOnStartup = showMainOnStartupCheck_->isChecked();
    updated.autoRunEnabled = autoRunCheck_->isChecked();
    updated.allowLanConnection = allowLanConnectionCheck_->isChecked();
    updated.sniffingEnabled = sniffingEnabledCheck_->isChecked();
    updated.routeOnly = routeOnlyCheck_->isChecked();
    updated.localPort = localPortSpin_->value();
    updated.enableFragment = enableFragmentCheck_->isChecked();
    updated.enableCacheFile4Sbox = enableCacheFile4SboxCheck_->isChecked();
    updated.defaultAllowInsecure = defaultAllowInsecureCheck_->isChecked();
    updated.defaultFingerprint = defaultFingerprintCombo_->currentText().trimmed();
    updated.defaultUserAgent = defaultUserAgentCombo_->currentText().trimmed();
    updated.mux4SboxProtocol = mux4SboxProtocolCombo_->currentText();
    updated.enableStatistics = enableStatisticsCheck_->isChecked();
    updated.statisticsFreshRate = statisticsFreshRateSpin_->value();
    updated.trayMenuServersLimit = trayMenuServersLimitSpin_->value();
    updated.systemProxyExceptions = systemProxyExceptionsEdit_->text().trimmed();
    updated.systemProxyAdvancedProtocol = systemProxyAdvancedProtocolCombo_->currentText().trimmed();
    updated.pacUrl = pacUrlEdit_->text().trimmed();
    updated.autoUpdateInterval = autoUpdateIntervalSpin_->value();
    updated.autoUpdateSubInterval = autoUpdateSubIntervalSpin_->value();
    updated.checkPreReleaseUpdate = checkPreReleaseUpdateCheck_->isChecked();
    updated.ignoreGeoUpdateCore = ignoreGeoUpdateCoreCheck_->isChecked();
    updated.enableSecurityProtocolTls13 = enableSecurityProtocolTls13Check_->isChecked();
    updated.languageCode = languageCombo_->currentData().toString().trimmed();
    updated.routingItems = collectRoutingItems();
    updated.subscriptions = collectSubItems();
    updated.coreTypeItems = collectCoreTypeItems();
    TunModeItem tun;
    tun.enableTun = tunEnableCheck_->isChecked();
    tun.autoRoute = tunAutoRouteCheck_->isChecked();
    tun.strictRoute = tunStrictRouteCheck_->isChecked();
    tun.mtu = tunMtuSpin_->value();
    tun.stack = tunStackCombo_->currentText().trimmed();
    tun.enableIPv6Address = tunEnableIPv6AddressCheck_->isChecked();
    tun.icmpRouting = tunIcmpRoutingEdit_->text().trimmed();
    tun.enableLegacyProtect = tunEnableLegacyProtectCheck_->isChecked();
    updated.tunModeItem = tun;

    // DNS
    updated.remoteDns = remoteDnsEdit_->text().trimmed();
    updated.directDns = directDnsEdit_->text().trimmed();
    updated.bootstrapDns = bootstrapDnsEdit_->text().trimmed();
    updated.domainStrategyForFreedom = domainStrategyForFreedomCombo_->currentText().trimmed();
    updated.domainStrategyForProxy = domainStrategyForProxyCombo_->currentText().trimmed();
    updated.useSystemHosts = useSystemHostsCheck_->isChecked();
    updated.addCommonHosts = addCommonHostsCheck_->isChecked();
    updated.blockBindingQuery = blockBindingQueryCheck_->isChecked();
    updated.fakeIp = fakeIpCheck_->isChecked();
    updated.globalFakeIp = globalFakeIpCheck_->isChecked();
    updated.serveStale = serveStaleCheck_->isChecked();
    updated.parallelQuery = parallelQueryCheck_->isChecked();
    updated.directExpectedIps = directExpectedIpsEdit_->text().trimmed();
    updated.dnsHosts = dnsHostsEdit_->toPlainText().trimmed();

    return updated;
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Settings"));
    resize(620, 540);

    // === General Tab ===
    auto* generalTab = new QWidget(this);
    auto* generalLayout = new QFormLayout(generalTab);

    showMainOnStartupCheck_ = new QCheckBox(tr("Show the main window after startup"), generalTab);
    autoRunCheck_ = new QCheckBox(tr("Enable auto run"), generalTab);
    allowLanConnectionCheck_ = new QCheckBox(tr("Allow LAN connections"), generalTab);
    sniffingEnabledCheck_ = new QCheckBox(tr("Enable traffic sniffing"), generalTab);
    sniffingEnabledCheck_->setObjectName(QStringLiteral("settingsSniffingEnabledCheck"));
    routeOnlyCheck_ = new QCheckBox(tr("Route only when sniffing"), generalTab);
    routeOnlyCheck_->setObjectName(QStringLiteral("settingsRouteOnlyCheck"));

    localPortSpin_ = new QSpinBox(generalTab);
    localPortSpin_->setObjectName(QStringLiteral("settingsLocalPortSpin"));
    localPortSpin_->setRange(1, 65535);

    enableFragmentCheck_ = new QCheckBox(tr("Enable fragmentation for TLS outbounds"), generalTab);
    enableFragmentCheck_->setObjectName(QStringLiteral("settingsEnableFragmentCheck"));

    enableCacheFile4SboxCheck_ = new QCheckBox(tr("Enable sing-box cache file"), generalTab);
    enableCacheFile4SboxCheck_->setObjectName(QStringLiteral("settingsEnableCacheFile4SboxCheck"));

    defaultAllowInsecureCheck_ = new QCheckBox(tr("Allow insecure TLS by default"), generalTab);
    defaultAllowInsecureCheck_->setObjectName(QStringLiteral("settingsDefaultAllowInsecureCheck"));

    defaultFingerprintCombo_ = new QComboBox(generalTab);
    defaultFingerprintCombo_->setObjectName(QStringLiteral("settingsDefaultFingerprintCombo"));
    defaultFingerprintCombo_->setEditable(true);
    defaultFingerprintCombo_->addItems(defaultFingerprintOptions());

    defaultUserAgentCombo_ = new QComboBox(generalTab);
    defaultUserAgentCombo_->setObjectName(QStringLiteral("settingsDefaultUserAgentCombo"));
    defaultUserAgentCombo_->setEditable(true);
    defaultUserAgentCombo_->addItems(defaultUserAgentOptions());

    mux4SboxProtocolCombo_ = new QComboBox(generalTab);
    mux4SboxProtocolCombo_->setObjectName(QStringLiteral("settingsMux4SboxProtocolCombo"));
    mux4SboxProtocolCombo_->addItems(singBoxMuxProtocolOptions());

    enableStatisticsCheck_ = new QCheckBox(tr("Enable traffic statistics"), generalTab);
    statisticsFreshRateSpin_ = new QSpinBox(generalTab);
    statisticsFreshRateSpin_->setObjectName(QStringLiteral("settingsStatisticsFreshRateSpin"));
    statisticsFreshRateSpin_->setRange(1, 100);
    statisticsFreshRateSpin_->setSuffix(tr(" s"));

    trayMenuServersLimitSpin_ = new QSpinBox(generalTab);
    trayMenuServersLimitSpin_->setObjectName(QStringLiteral("settingsTrayMenuServersLimitSpin"));
    trayMenuServersLimitSpin_->setRange(0, 999);
    trayMenuServersLimitSpin_->setSpecialValueText(tr("Auto"));

    languageCombo_ = new QComboBox(generalTab);
    languageCombo_->setObjectName(QStringLiteral("settingsLanguageCombo"));
    languageCombo_->addItem(tr("System Default"), QString());
    languageCombo_->addItem(tr("English"), QStringLiteral("en"));
    languageCombo_->addItem(tr("Chinese (Simplified)"), QStringLiteral("zh_CN"));

    generalLayout->addRow(showMainOnStartupCheck_);
    generalLayout->addRow(autoRunCheck_);
    generalLayout->addRow(allowLanConnectionCheck_);
    generalLayout->addRow(sniffingEnabledCheck_);
    generalLayout->addRow(routeOnlyCheck_);
    generalLayout->addRow(tr("Local Port"), localPortSpin_);
    generalLayout->addRow(enableFragmentCheck_);
    generalLayout->addRow(enableCacheFile4SboxCheck_);
    generalLayout->addRow(defaultAllowInsecureCheck_);
    generalLayout->addRow(tr("Default Fingerprint"), defaultFingerprintCombo_);
    generalLayout->addRow(tr("Default User-Agent"), defaultUserAgentCombo_);
    generalLayout->addRow(tr("sing-box Mux Protocol"), mux4SboxProtocolCombo_);
    generalLayout->addRow(enableStatisticsCheck_);
    generalLayout->addRow(tr("Statistics Refresh Rate"), statisticsFreshRateSpin_);
    generalLayout->addRow(tr("Tray Menu Server Limit"), trayMenuServersLimitSpin_);
    generalLayout->addRow(tr("Language"), languageCombo_);

    // === Subscriptions Tab ===
    auto* subTab = new QWidget(this);
    auto* subLayout = new QVBoxLayout(subTab);

    subTable_ = new QTableWidget(subTab);
    subTable_->setColumnCount(4);
    subTable_->setHorizontalHeaderLabels({
        QStringLiteral("Enabled"),
        QStringLiteral("Remarks"),
        QStringLiteral("URL"),
        QStringLiteral("User Agent")
    });
    subTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    subTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    subTable_->verticalHeader()->setVisible(false);
    subTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    subTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    subTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    subTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    addSubButton_ = new QPushButton(QStringLiteral("Add"), subTab);
    removeSubButton_ = new QPushButton(QStringLiteral("Remove"), subTab);
    updateSubButton_ = new QPushButton(QStringLiteral("Update Selected"), subTab);

    auto* subButtonLayout = new QHBoxLayout();
    subButtonLayout->addWidget(addSubButton_);
    subButtonLayout->addWidget(removeSubButton_);
    subButtonLayout->addWidget(updateSubButton_);
    subButtonLayout->addStretch(1);

    subLayout->addLayout(subButtonLayout);
    subLayout->addWidget(subTable_);

    // === Routing Tab ===
    auto* routingTab = new QWidget(this);
    auto* routingLayout = new QVBoxLayout(routingTab);

    routeHintLabel_ = new QLabel(routingTab);
    routeHintLabel_->setWordWrap(true);

    routingTable_ = new QTableWidget(routingTab);
    routingTable_->setColumnCount(5);
    routingTable_->setHorizontalHeaderLabels({
        QStringLiteral("Enabled"),
        QStringLiteral("Remarks"),
        QStringLiteral("URL"),
        QStringLiteral("Icon"),
        QStringLiteral("Rules")
    });
    routingTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routingTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    routingTable_->verticalHeader()->setVisible(false);
    routingTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    routingTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    routingTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    routingTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    routingTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    addRouteButton_ = new QPushButton(QStringLiteral("Add Route"), routingTab);
    removeRouteButton_ = new QPushButton(QStringLiteral("Remove Route"), routingTab);

    auto* routeButtonLayout = new QHBoxLayout();
    routeButtonLayout->addWidget(addRouteButton_);
    routeButtonLayout->addWidget(removeRouteButton_);
    routeButtonLayout->addStretch(1);

    ruleTable_ = new QTableWidget(routingTab);
    ruleTable_->setColumnCount(7);
    ruleTable_->setHorizontalHeaderLabels({
        QStringLiteral("Enabled"),
        QStringLiteral("Outbound"),
        QStringLiteral("Port"),
        QStringLiteral("Protocol"),
        QStringLiteral("Inbound"),
        QStringLiteral("Domain"),
        QStringLiteral("IP")
    });
    ruleTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    ruleTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    ruleTable_->verticalHeader()->setVisible(false);
    ruleTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ruleTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ruleTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ruleTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ruleTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    ruleTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    ruleTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    const int narrowColWidth = ruleTable_->fontMetrics().horizontalAdvance(QLatin1Char('x')) * 10 + 16;
    ruleTable_->setColumnWidth(1, narrowColWidth);
    ruleTable_->setColumnWidth(3, narrowColWidth);
    ruleTable_->setColumnWidth(4, narrowColWidth);

    addRuleButton_ = new QPushButton(QStringLiteral("Add Rule"), routingTab);
    removeRuleButton_ = new QPushButton(QStringLiteral("Remove Rule"), routingTab);

    auto* ruleButtonLayout = new QHBoxLayout();
    ruleButtonLayout->addWidget(addRuleButton_);
    ruleButtonLayout->addWidget(removeRuleButton_);
    ruleButtonLayout->addStretch(1);

    routingLayout->addWidget(routeHintLabel_);
    routingLayout->addLayout(routeButtonLayout);
    routingLayout->addWidget(routingTable_, 3);
    routingLayout->addLayout(ruleButtonLayout);
    routingLayout->addWidget(ruleTable_, 4);

    // === Core Tab ===
    auto* coreTab = new QWidget(this);
    auto* coreLayout = new QVBoxLayout(coreTab);

    // EConfigType values: VMess=1, Custom=2, Shadowsocks=3, Socks=4, VLESS=5, Trojan=6, HTTP=11
    const QStringList protocolNames = {
        QStringLiteral("VMess"),
        QStringLiteral("Custom"),
        QStringLiteral("Shadowsocks"),
        QStringLiteral("Socks"),
        QStringLiteral("VLESS"),
        QStringLiteral("Trojan"),
        QStringLiteral("HTTP")
    };
    const QList<int> configTypes = { 1, 2, 3, 4, 5, 6, 11 };
    const QList<CoreType> allCores = availableCoreTypes();

    // Per-protocol core selection
    auto* coreTypeGroup = new QGroupBox(tr("Per-Protocol Override"), coreTab);
    auto* coreTypeForm = new QFormLayout(coreTypeGroup);

    for (int i = 0; i < protocolNames.size(); ++i) {
        const QList<CoreType> cores = supportedCoreTypes(static_cast<ConfigType>(configTypes.at(i)));
        if (cores.size() > 1) {
            auto* coreCombo = new QComboBox(coreTab);
            for (const CoreType core : cores) {
                coreCombo->addItem(coreTypeDisplayName(core), static_cast<int>(core));
            }
            coreCombo->setObjectName(QStringLiteral("coreTypeCombo_%1").arg(configTypes.at(i)));
            coreTypeForm->addRow(protocolNames.at(i), coreCombo);
            coreTypeCombos_.append(coreCombo);
        } else if (cores.size() == 1) {
            auto* fixedLabel = new QLabel(coreTypeDisplayName(cores.first()), coreTab);
            fixedLabel->setObjectName(QStringLiteral("coreTypeCombo_%1").arg(configTypes.at(i)));
            coreTypeForm->addRow(protocolNames.at(i), fixedLabel);
            coreTypeCombos_.append(nullptr);
        }
    }

    // Installed Cores — one row per available core type, with download + "set all" actions
    auto* coreStatusGroup = new QGroupBox(tr("Installed Cores"), coreTab);
    auto* coreStatusLayout = new QFormLayout(coreStatusGroup);

    for (const CoreType core : allCores) {
        const int coreKey = static_cast<int>(core);
        auto& row = coreStatusRows_[coreKey];

        row.versionLabel = new QLabel(coreTab);
        row.versionLabel->setObjectName(QStringLiteral("coreVersionLabel_%1").arg(coreKey));
        row.statusLabel = new QLabel(coreTab);
        row.statusLabel->setObjectName(QStringLiteral("coreStatusLabel_%1").arg(coreKey));
        row.downloadButton = new QPushButton(tr("Download"), coreTab);
        row.downloadButton->setObjectName(QStringLiteral("coreDownloadButton_%1").arg(coreKey));

        row.setAllButton = new QPushButton(tr("Set All"), coreTab);
        row.setAllButton->setToolTip(tr("Set all protocols to %1").arg(coreTypeDisplayName(core)));
        connect(row.setAllButton, &QPushButton::clicked, this, [this, configTypes, core]() {
            for (int i = 0; i < coreTypeCombos_.size() && i < configTypes.size(); ++i) {
                auto* combo = coreTypeCombos_.at(i);
                if (combo == nullptr) continue;
                if (protocolSupportsCore(static_cast<ConfigType>(configTypes.at(i)), core)) {
                    const int idx = combo->findData(static_cast<int>(core));
                    if (idx >= 0) combo->setCurrentIndex(idx);
                }
            }
        });

        auto* infoLayout = new QVBoxLayout();
        infoLayout->setContentsMargins(0, 0, 0, 0);
        infoLayout->setSpacing(2);
        infoLayout->addWidget(row.versionLabel);
        infoLayout->addWidget(row.statusLabel);

        auto* rowLayout = new QHBoxLayout();
        rowLayout->addLayout(infoLayout);
        rowLayout->addWidget(row.downloadButton);
        rowLayout->addWidget(row.setAllButton);
        rowLayout->addStretch();

        coreStatusLayout->addRow(coreTypeDisplayName(core) + QStringLiteral(":"), rowLayout);

        connect(row.downloadButton, &QPushButton::clicked, this, [this, core]() {
            coreDownloadRequested_ = true;
            requestedCoreDownload_ = core;
            beginCoreUpdate(core);
            emit coreDownloadRequested(static_cast<int>(core));
        });
    }

    coreLayout->addWidget(coreTypeGroup);
    coreLayout->addWidget(coreStatusGroup);

    // === Proxy Tab ===
    auto* proxyTab = new QWidget(this);
    auto* proxyLayout = new QFormLayout(proxyTab);

    systemProxyExceptionsEdit_ = new QLineEdit(proxyTab);
    systemProxyExceptionsEdit_->setObjectName(QStringLiteral("settingsSystemProxyExceptionsEdit"));
    systemProxyExceptionsEdit_->setPlaceholderText(QStringLiteral("localhost;127.*;192.168.*"));

    systemProxyAdvancedProtocolCombo_ = new QComboBox(proxyTab);
    systemProxyAdvancedProtocolCombo_->setObjectName(QStringLiteral("settingsSystemProxyAdvancedProtocolCombo"));
    systemProxyAdvancedProtocolCombo_->setEditable(true);
    systemProxyAdvancedProtocolCombo_->addItems({
        QString(),
        QStringLiteral("{ip}:{http_port}"),
        QStringLiteral("socks={ip}:{socks_port}"),
        QStringLiteral("http={ip}:{http_port};https={ip}:{http_port};ftp={ip}:{http_port};socks={ip}:{socks_port}"),
        QStringLiteral("http=http://{ip}:{http_port};https=http://{ip}:{http_port}")
    });

    proxyLayout->addRow(tr("System Proxy Exceptions"), systemProxyExceptionsEdit_);
    proxyLayout->addRow(tr("Advanced Proxy Protocol"), systemProxyAdvancedProtocolCombo_);

    pacUrlEdit_ = new QLineEdit(proxyTab);
    pacUrlEdit_->setObjectName(QStringLiteral("settingsPacUrlEdit"));
    pacUrlEdit_->setPlaceholderText(tr("Leave empty to use built-in PAC server (http://127.0.0.1:10870/pac)"));
    proxyLayout->addRow(tr("Custom PAC URL"), pacUrlEdit_);

    // === Update Tab ===
    auto* updateTab = new QWidget(this);
    auto* updateLayout = new QFormLayout(updateTab);

    autoUpdateIntervalSpin_ = new QSpinBox(updateTab);
    autoUpdateIntervalSpin_->setObjectName(QStringLiteral("settingsAutoUpdateIntervalSpin"));
    autoUpdateIntervalSpin_->setRange(0, 3650);
    autoUpdateIntervalSpin_->setSpecialValueText(tr("Disabled"));

    autoUpdateSubIntervalSpin_ = new QSpinBox(updateTab);
    autoUpdateSubIntervalSpin_->setObjectName(QStringLiteral("settingsAutoUpdateSubIntervalSpin"));
    autoUpdateSubIntervalSpin_->setRange(0, 3650);
    autoUpdateSubIntervalSpin_->setSpecialValueText(tr("Disabled"));

    checkPreReleaseUpdateCheck_ = new QCheckBox(tr("Include pre-release builds"), updateTab);
    ignoreGeoUpdateCoreCheck_ = new QCheckBox(tr("Do not overwrite geo files during core updates"), updateTab);
    enableSecurityProtocolTls13Check_ = new QCheckBox(tr("Enable TLS 1.3 for downloads"), updateTab);

    auto* updateNote = new QLabel(
        tr("Intervals are stored for compatibility. A value of 0 disables the scheduled check."),
        updateTab);
    updateNote->setWordWrap(true);
    updateNote->setObjectName(QStringLiteral("settingsUpdateNoteLabel"));

    updateLayout->addRow(tr("GUI / Core Update Interval"), autoUpdateIntervalSpin_);
    updateLayout->addRow(tr("Subscription Update Interval"), autoUpdateSubIntervalSpin_);
    updateLayout->addRow(checkPreReleaseUpdateCheck_);
    updateLayout->addRow(ignoreGeoUpdateCoreCheck_);
    updateLayout->addRow(enableSecurityProtocolTls13Check_);
    updateLayout->addRow(updateNote);

    // === TUN Tab ===
    auto* tunTab = new QWidget(this);
    auto* tunLayout = new QFormLayout(tunTab);

    tunEnableCheck_ = new QCheckBox(tr("Enable TUN mode (requires admin privileges)"), tunTab);

    tunAutoRouteCheck_ = new QCheckBox(tr("Auto route (route all traffic through TUN)"), tunTab);
    tunStrictRouteCheck_ = new QCheckBox(tr("Strict route (apply strict routing rules)"), tunTab);

    tunMtuSpin_ = new QSpinBox(tunTab);
    tunMtuSpin_->setObjectName(QStringLiteral("settingsTunMtuSpin"));
    tunMtuSpin_->setRange(0, 99999);
    tunMtuSpin_->setSpecialValueText(tr("Auto (9000)"));

    tunStackCombo_ = new QComboBox(tunTab);
    tunStackCombo_->setObjectName(QStringLiteral("settingsTunStackCombo"));
    tunStackCombo_->addItems({
        QStringLiteral("system"),
        QStringLiteral("gvisor"),
        QStringLiteral("mixed")
    });

    tunEnableIPv6AddressCheck_ = new QCheckBox(tr("Enable IPv6 address"), tunTab);
    tunIcmpRoutingEdit_ = new QLineEdit(tunTab);
    tunIcmpRoutingEdit_->setObjectName(QStringLiteral("settingsTunIcmpRoutingEdit"));
    tunIcmpRoutingEdit_->setPlaceholderText(tr("Optional ICMP routing preference"));
    tunEnableLegacyProtectCheck_ = new QCheckBox(tr("Enable legacy protection (pre-socks relay for Xray)"), tunTab);

    tunLayout->addRow(tunEnableCheck_);
    tunLayout->addRow(tunAutoRouteCheck_);
    tunLayout->addRow(tunStrictRouteCheck_);
    tunLayout->addRow(tr("MTU"), tunMtuSpin_);
    tunLayout->addRow(tr("Stack"), tunStackCombo_);
    tunLayout->addRow(tunEnableIPv6AddressCheck_);
    tunLayout->addRow(tr("ICMP Routing"), tunIcmpRoutingEdit_);
    tunLayout->addRow(tunEnableLegacyProtectCheck_);

    // === DNS Tab ===
    auto* dnsTab = new QWidget(this);
    auto* dnsLayout = new QFormLayout(dnsTab);

    remoteDnsEdit_ = new QLineEdit(dnsTab);
    remoteDnsEdit_->setPlaceholderText(QStringLiteral("https://cloudflare-dns.com/dns-query"));

    directDnsEdit_ = new QLineEdit(dnsTab);
    directDnsEdit_->setPlaceholderText(QStringLiteral("https://dns.alidns.com/dns-query"));

    bootstrapDnsEdit_ = new QLineEdit(dnsTab);
    bootstrapDnsEdit_->setPlaceholderText(QStringLiteral("223.5.5.5"));

    domainStrategyForFreedomCombo_ = new QComboBox(dnsTab);
    domainStrategyForFreedomCombo_->setEditable(true);
    domainStrategyForFreedomCombo_->addItems({
        QString(),
        QStringLiteral("AsIs"),
        QStringLiteral("IPIfNonMatch"),
        QStringLiteral("IPOnDemand"),
        QStringLiteral("PreferIPv4"),
        QStringLiteral("PreferIPv6"),
        QStringLiteral("PreferIPv4v6"),
        QStringLiteral("PreferIPv6v4"),
        QStringLiteral("OnlyIPv4"),
        QStringLiteral("OnlyIPv6")
    });

    domainStrategyForProxyCombo_ = new QComboBox(dnsTab);
    domainStrategyForProxyCombo_->setEditable(true);
    domainStrategyForProxyCombo_->addItems({
        QString(),
        QStringLiteral("AsIs"),
        QStringLiteral("IPIfNonMatch"),
        QStringLiteral("IPOnDemand"),
        QStringLiteral("PreferIPv4"),
        QStringLiteral("PreferIPv6"),
        QStringLiteral("PreferIPv4v6"),
        QStringLiteral("PreferIPv6v4"),
        QStringLiteral("OnlyIPv4"),
        QStringLiteral("OnlyIPv6")
    });

    useSystemHostsCheck_ = new QCheckBox(tr("Use system hosts file"), dnsTab);
    addCommonHostsCheck_ = new QCheckBox(tr("Add common hosts (google, github, etc.)"), dnsTab);
    blockBindingQueryCheck_ = new QCheckBox(tr("Block binding query"), dnsTab);
    fakeIpCheck_ = new QCheckBox(tr("Enable FakeIP"), dnsTab);
    globalFakeIpCheck_ = new QCheckBox(tr("Global FakeIP"), dnsTab);
    serveStaleCheck_ = new QCheckBox(tr("Serve stale DNS cache"), dnsTab);
    parallelQueryCheck_ = new QCheckBox(tr("Parallel DNS query"), dnsTab);

    directExpectedIpsEdit_ = new QLineEdit(dnsTab);
    directExpectedIpsEdit_->setPlaceholderText(QStringLiteral("Comma-separated IPs"));

    dnsHostsEdit_ = new QTextEdit(dnsTab);
    dnsHostsEdit_->setTabChangesFocus(true);
    dnsHostsEdit_->setMaximumHeight(120);
    dnsHostsEdit_->setPlaceholderText(QStringLiteral("domain ip\nexample.com 1.2.3.4"));

    dnsLayout->addRow(tr("Remote DNS"), remoteDnsEdit_);
    dnsLayout->addRow(tr("Direct DNS"), directDnsEdit_);
    dnsLayout->addRow(tr("Bootstrap DNS"), bootstrapDnsEdit_);
    dnsLayout->addRow(tr("Domain Strategy (Freedom)"), domainStrategyForFreedomCombo_);
    dnsLayout->addRow(tr("Domain Strategy (Proxy)"), domainStrategyForProxyCombo_);
    dnsLayout->addRow(useSystemHostsCheck_);
    dnsLayout->addRow(addCommonHostsCheck_);
    dnsLayout->addRow(blockBindingQueryCheck_);
    dnsLayout->addRow(fakeIpCheck_);
    dnsLayout->addRow(globalFakeIpCheck_);
    dnsLayout->addRow(serveStaleCheck_);
    dnsLayout->addRow(parallelQueryCheck_);
    dnsLayout->addRow(tr("Direct Expected IPs"), directExpectedIpsEdit_);
    dnsLayout->addRow(tr("DNS Hosts"), dnsHostsEdit_);

    // === Tab Widget ===
    tabWidget_ = new QTabWidget(this);
    tabWidget_->setObjectName(QStringLiteral("settingsTabWidget"));
    tabWidget_->addTab(generalTab, tr("General"));
    tabWidget_->addTab(subTab, tr("Subscriptions"));
    tabWidget_->addTab(routingTab, tr("Routing"));
    tabWidget_->addTab(coreTab, tr("Core"));
    tabWidget_->addTab(proxyTab, tr("Proxy"));
    tabWidget_->addTab(tunTab, QStringLiteral("TUN"));
    tabWidget_->addTab(dnsTab, QStringLiteral("DNS"));
    tabWidget_->addTab(updateTab, tr("Update"));

    // === Button Box with Restore ===
    restoreBackupButton_ = new QPushButton(tr("Restore Backup"), this);
    restoreBackupButton_->setObjectName(QStringLiteral("settingsRestoreBackupButton"));
    connect(restoreBackupButton_, &QPushButton::clicked, this, [this]() {
        restoreBackupRequested_ = true;
    });

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(restoreBackupButton_);
    bottomLayout->addStretch();
    bottomLayout->addWidget(buttonBox_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->addWidget(tabWidget_);
    rootLayout->addLayout(bottomLayout);

    connect(enableStatisticsCheck_, &QCheckBox::toggled, this, [this](bool) {
        updateFieldState();
    });
    connect(tunEnableCheck_, &QCheckBox::toggled, this, [this](bool) {
        updateFieldState();
    });
    connect(routingTable_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int previousRow, int) {
        Q_UNUSED(previousRow)

        if (currentRow == currentRoutingRow_) {
            return;
        }

        persistCurrentRuleTable();
        currentRoutingRow_ = currentRow;
        reloadRuleTable(currentRoutingRow_);
        updateRoutingActionState();
    });
    connect(addRouteButton_, &QPushButton::clicked, this, [this]() {
        persistCurrentRuleTable();

        RoutingItem item;
        item.remarks = QStringLiteral("Route %1").arg(routingItems_.size() + 1);
        routingItems_.append(item);
        reloadRoutingTable(routingItems_.size() - 1);
    });
    connect(removeRouteButton_, &QPushButton::clicked, this, [this]() {
        const int row = routingTable_->currentRow();
        if (row < 0 || row >= routingItems_.size()) {
            return;
        }

        persistCurrentRuleTable();
        routingItems_.removeAt(row);
        reloadRoutingTable(row >= routingItems_.size() ? routingItems_.size() - 1 : row);
    });
    connect(addRuleButton_, &QPushButton::clicked, this, [this]() {
        RoutingRule rule;
        rule.enabled = true;
        rule.outboundTag = QStringLiteral("proxy");
        appendRuleRow(rule);
        updateRoutingActionState();
    });
    connect(removeRuleButton_, &QPushButton::clicked, this, [this]() {
        const int row = ruleTable_->currentRow();
        if (row < 0 || row >= ruleTable_->rowCount()) {
            return;
        }

        ruleTable_->removeRow(row);
        updateRoutingActionState();
    });
    connect(buttonBox_, &QDialogButtonBox::accepted, this, [this]() {
        persistCurrentRuleTable();
        accept();
    });
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Subscriptions tab connections
    connect(addSubButton_, &QPushButton::clicked, this, [this]() {
        appendSubRow(SubItem{ QString(), QStringLiteral("Subscription"), QStringLiteral("https://"), true, QString() });
        updateSubActionState();
    });
    connect(removeSubButton_, &QPushButton::clicked, this, [this]() {
        QModelIndexList rows = subTable_->selectionModel() == nullptr
            ? QModelIndexList()
            : subTable_->selectionModel()->selectedRows();
        std::sort(rows.begin(), rows.end(), [](const QModelIndex& lhs, const QModelIndex& rhs) {
            return lhs.row() > rhs.row();
        });
        for (const QModelIndex& row : rows) {
            subTable_->removeRow(row.row());
        }
        updateSubActionState();
    });
    connect(updateSubButton_, &QPushButton::clicked, this, [this]() {
        updateSubRequested_ = true;
    });
    connect(subTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        updateSubActionState();
    });

    reloadRoutingTable();
}

void SettingsDialog::updateFieldState()
{
    const bool statisticsEnabled = enableStatisticsCheck_ != nullptr && enableStatisticsCheck_->isChecked();
    if (statisticsFreshRateSpin_ != nullptr) {
        statisticsFreshRateSpin_->setEnabled(statisticsEnabled);
    }

    const bool tunEnabled = tunEnableCheck_ != nullptr && tunEnableCheck_->isChecked();
    if (tunAutoRouteCheck_ != nullptr) {
        tunAutoRouteCheck_->setEnabled(tunEnabled);
    }
    if (tunStrictRouteCheck_ != nullptr) {
        tunStrictRouteCheck_->setEnabled(tunEnabled);
    }
    if (tunMtuSpin_ != nullptr) {
        tunMtuSpin_->setEnabled(tunEnabled);
    }
    if (tunStackCombo_ != nullptr) {
        tunStackCombo_->setEnabled(tunEnabled);
    }
    if (tunEnableIPv6AddressCheck_ != nullptr) {
        tunEnableIPv6AddressCheck_->setEnabled(tunEnabled);
    }
    if (tunIcmpRoutingEdit_ != nullptr) {
        tunIcmpRoutingEdit_->setEnabled(tunEnabled);
    }
    if (tunEnableLegacyProtectCheck_ != nullptr) {
        tunEnableLegacyProtectCheck_->setEnabled(tunEnabled);
    }
}

void SettingsDialog::reloadRoutingTable(int selectedRow)
{
    const QSignalBlocker blocker(routingTable_);
    routingTable_->setRowCount(0);

    for (const RoutingItem& item : routingItems_) {
        appendRoutingRow(item);
    }

    if (routingItems_.isEmpty()) {
        currentRoutingRow_ = -1;
        reloadRuleTable(-1);
        updateRoutingActionState();
        return;
    }

    int rowToSelect = selectedRow;
    if (rowToSelect < 0 || rowToSelect >= routingItems_.size()) {
        rowToSelect = 0;
    }

    currentRoutingRow_ = rowToSelect;
    routingTable_->selectRow(rowToSelect);
    reloadRuleTable(rowToSelect);
    updateRoutingActionState();
}

void SettingsDialog::reloadRuleTable(int routeRow)
{
    const QSignalBlocker blocker(ruleTable_);
    ruleTable_->setRowCount(0);

    if (routeRow < 0 || routeRow >= routingItems_.size()) {
        updateRoutingActionState();
        return;
    }

    const QList<RoutingRule>& rules = routingItems_.at(routeRow).rules;
    for (const RoutingRule& rule : rules) {
        appendRuleRow(rule);
    }

    if (ruleTable_->rowCount() > 0) {
        ruleTable_->selectRow(0);
    }

    updateRoutingActionState();
}

void SettingsDialog::persistCurrentRuleTable()
{
    if (currentRoutingRow_ < 0 || currentRoutingRow_ >= routingItems_.size()) {
        return;
    }

    routingItems_[currentRoutingRow_].rules = collectRulesFromTable();

    if (routingTable_->item(currentRoutingRow_, 4) != nullptr) {
        routingTable_->item(currentRoutingRow_, 4)
            ->setText(QString::number(routingItems_.at(currentRoutingRow_).rules.size()));
    }
}

QList<RoutingItem> SettingsDialog::collectRoutingItems() const
{
    QList<RoutingItem> items = routingItems_;
    const int rowCount = routingTable_ == nullptr ? 0 : routingTable_->rowCount();
    for (int row = 0; row < rowCount && row < items.size(); ++row) {
        auto* enabledItem = routingTable_->item(row, 0);
        auto* remarksItem = routingTable_->item(row, 1);
        auto* urlItem = routingTable_->item(row, 2);
        auto* iconItem = routingTable_->item(row, 3);

        items[row].enabled = enabledItem == nullptr || enabledItem->checkState() == Qt::Checked;
        items[row].remarks = remarksItem == nullptr ? QString() : remarksItem->text().trimmed();
        items[row].url = urlItem == nullptr ? QString() : urlItem->text().trimmed();
        items[row].customIcon = iconItem == nullptr ? QString() : iconItem->text().trimmed();
    }

    if (currentRoutingRow_ >= 0 && currentRoutingRow_ < items.size()) {
        items[currentRoutingRow_].rules = collectRulesFromTable();
    }

    return items;
}

QList<RoutingRule> SettingsDialog::collectRulesFromTable() const
{
    QList<RoutingRule> rules;
    if (ruleTable_ == nullptr) {
        return rules;
    }

    for (int row = 0; row < ruleTable_->rowCount(); ++row) {
        auto* enabledItem = ruleTable_->item(row, 0);
        auto* outboundItem = ruleTable_->item(row, 1);
        auto* portItem = ruleTable_->item(row, 2);
        auto* protocolItem = ruleTable_->item(row, 3);
        auto* inboundItem = ruleTable_->item(row, 4);
        auto* domainItem = ruleTable_->item(row, 5);
        auto* ipItem = ruleTable_->item(row, 6);

        RoutingRule rule;
        rule.type = QStringLiteral("field");
        rule.enabled = enabledItem == nullptr || enabledItem->checkState() == Qt::Checked;
        rule.outboundTag = outboundItem == nullptr ? QString() : outboundItem->text().trimmed();
        rule.port = portItem == nullptr ? QString() : portItem->text().trimmed();
        rule.network = outboundItem == nullptr ? QString() : outboundItem->data(RuleNetworkRole).toString();
        rule.protocol = splitValues(protocolItem == nullptr ? QString() : protocolItem->text());
        rule.inboundTag = splitValues(inboundItem == nullptr ? QString() : inboundItem->text());
        rule.domain = splitValues(domainItem == nullptr ? QString() : domainItem->text());
        rule.ip = splitValues(ipItem == nullptr ? QString() : ipItem->text());
        rule.process = outboundItem == nullptr
            ? QStringList()
            : outboundItem->data(RuleProcessRole).toStringList();
        rules.append(rule);
    }

    return rules;
}

namespace {

QTableWidgetItem* createCheckItem(bool checked)
{
    auto* item = new QTableWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

QTableWidgetItem* createTextItem(const QString& text)
{
    return new QTableWidgetItem(text);
}

QTableWidgetItem* createReadOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

void SettingsDialog::appendRoutingRow(const RoutingItem& item)
{
    const int row = routingTable_->rowCount();
    routingTable_->insertRow(row);
    routingTable_->setItem(row, 0, createCheckItem(item.enabled));
    routingTable_->setItem(row, 1, createTextItem(item.remarks));
    routingTable_->setItem(row, 2, createTextItem(item.url));
    routingTable_->setItem(row, 3, createTextItem(item.customIcon));
    routingTable_->setItem(row, 4, createReadOnlyItem(QString::number(item.rules.size())));
}

void SettingsDialog::appendRuleRow(const RoutingRule& rule)
{
    const int row = ruleTable_->rowCount();
    ruleTable_->insertRow(row);
    ruleTable_->setItem(row, 0, createCheckItem(rule.enabled));
    auto* outboundItem = createTextItem(rule.outboundTag);
    outboundItem->setData(RuleNetworkRole, rule.network);
    outboundItem->setData(RuleProcessRole, rule.process);
    ruleTable_->setItem(row, 1, outboundItem);
    ruleTable_->setItem(row, 2, createTextItem(rule.port));
    ruleTable_->setItem(row, 3, createTextItem(joinValues(rule.protocol)));
    ruleTable_->setItem(row, 4, createTextItem(joinValues(rule.inboundTag)));
    ruleTable_->setItem(row, 5, createTextItem(joinValues(rule.domain)));
    ruleTable_->setItem(row, 6, createTextItem(joinValues(rule.ip)));
}

void SettingsDialog::updateRoutingActionState()
{
    const bool hasRouteSelection = routingTable_ != nullptr
        && routingTable_->currentRow() >= 0
        && routingTable_->currentRow() < routingTable_->rowCount();
    const bool hasRuleSelection = ruleTable_ != nullptr
        && ruleTable_->currentRow() >= 0
        && ruleTable_->currentRow() < ruleTable_->rowCount();

    if (removeRouteButton_ != nullptr) {
        removeRouteButton_->setEnabled(hasRouteSelection);
    }

    if (addRuleButton_ != nullptr) {
        addRuleButton_->setEnabled(hasRouteSelection);
    }

    if (removeRuleButton_ != nullptr) {
        removeRuleButton_->setEnabled(hasRuleSelection);
    }

    if (routeHintLabel_ != nullptr) {
        routeHintLabel_->setText(QStringLiteral("The selected row becomes the active routing profile."));
    }
}

int SettingsDialog::findInitialRouteIndex(const QList<RoutingItem>& items, const Config& config) const
{
    if (items.isEmpty()) {
        return -1;
    }

    if (config.enableRoutingAdvanced && config.routingIndex >= 0 && config.routingIndex < items.size()) {
        return config.routingIndex;
    }

    for (int index = 0; index < items.size(); ++index) {
        if (items.at(index).locked) {
            return index;
        }
    }

    return 0;
}

QString SettingsDialog::joinValues(const QStringList& values)
{
    return values.join(QStringLiteral(", "));
}

QStringList SettingsDialog::splitValues(const QString& value)
{
    QStringList values = value.split(QRegularExpression(QStringLiteral("[,\\n\\r]")), Qt::SkipEmptyParts);
    for (QString& item : values) {
        item = item.trimmed();
    }

    values.removeAll(QString());
    return values;
}

void SettingsDialog::reloadSubTable()
{
    const QSignalBlocker blocker(subTable_);
    subTable_->setRowCount(0);

    for (const SubItem& item : subItems_) {
        appendSubRow(item);
    }

    updateSubActionState();
}

void SettingsDialog::appendSubRow(const SubItem& item)
{
    const int row = subTable_->rowCount();
    subTable_->insertRow(row);

    auto* enabledItem = new QTableWidgetItem();
    enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable);
    enabledItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);

    auto* remarksItem = new QTableWidgetItem(item.remarks);
    remarksItem->setData(Qt::UserRole, item.id);

    auto* urlItem = new QTableWidgetItem(item.url);
    auto* userAgentItem = new QTableWidgetItem(item.userAgent);

    subTable_->setItem(row, 0, enabledItem);
    subTable_->setItem(row, 1, remarksItem);
    subTable_->setItem(row, 2, urlItem);
    subTable_->setItem(row, 3, userAgentItem);
}

void SettingsDialog::updateSubActionState()
{
    const bool hasSelection = !selectedSubRows().isEmpty();
    if (removeSubButton_ != nullptr) {
        removeSubButton_->setEnabled(hasSelection);
    }
    if (updateSubButton_ != nullptr) {
        updateSubButton_->setEnabled(hasSelection);
    }
}

QList<int> SettingsDialog::selectedSubRows() const
{
    QList<int> rows;
    if (subTable_ == nullptr || subTable_->selectionModel() == nullptr) {
        return rows;
    }

    const QModelIndexList indexes = subTable_->selectionModel()->selectedRows();
    for (const QModelIndex& index : indexes) {
        rows.append(index.row());
    }

    std::sort(rows.begin(), rows.end());
    return rows;
}

QList<SubItem> SettingsDialog::collectSubItems() const
{
    QList<SubItem> items;
    const int rowCount = subTable_ == nullptr ? 0 : subTable_->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        auto* enabledItem = subTable_->item(row, 0);
        auto* remarksItem = subTable_->item(row, 1);
        auto* urlItem = subTable_->item(row, 2);
        auto* userAgentItem = subTable_->item(row, 3);

        SubItem item;
        if (remarksItem != nullptr) {
            item.id = remarksItem->data(Qt::UserRole).toString();
            item.remarks = remarksItem->text().trimmed();
        }
        item.enabled = enabledItem == nullptr || enabledItem->checkState() == Qt::Checked;
        item.url = urlItem == nullptr ? QString() : urlItem->text().trimmed();
        item.userAgent = userAgentItem == nullptr ? QString() : userAgentItem->text().trimmed();

        if (!item.remarks.isEmpty() || !item.url.isEmpty()) {
            items.append(item);
        }
    }

    return items;
}

void SettingsDialog::reloadCoreTypeTable()
{
    const QList<int> configTypes = { 1, 2, 3, 4, 5, 6, 11 };

    for (int i = 0; i < coreTypeCombos_.size() && i < configTypes.size(); ++i) {
        auto* combo = coreTypeCombos_.at(i);
        if (combo == nullptr) {
            continue;
        }

        int configType = configTypes.at(i);
        int coreType = static_cast<int>(CoreType::Xray);

        for (const CoreTypeItem& item : coreTypeItems_) {
            if (item.configType == configType) {
                coreType = item.coreType;
                break;
            }
        }

        const int dataIndex = combo->findData(coreType);
        if (dataIndex >= 0) {
            combo->setCurrentIndex(dataIndex);
        }
    }
}

QList<CoreTypeItem> SettingsDialog::collectCoreTypeItems() const
{
    QList<CoreTypeItem> items;
    const QList<int> configTypes = { 1, 2, 3, 4, 5, 6, 11 };

    for (int i = 0; i < coreTypeCombos_.size() && i < configTypes.size(); ++i) {
        const int configType = configTypes.at(i);
        auto* combo = coreTypeCombos_.at(i);

        CoreTypeItem item;
        item.configType = configType;
        if (combo != nullptr) {
            bool ok = false;
            const int coreTypeValue = combo->currentData().toInt(&ok);
            item.coreType = ok ? coreTypeValue : static_cast<int>(CoreType::Xray);
        } else {
            const QList<CoreType> cores = supportedCoreTypes(static_cast<ConfigType>(configType));
            item.coreType = cores.isEmpty()
                ? static_cast<int>(CoreType::Xray)
                : static_cast<int>(cores.first());
        }
        items.append(item);
    }

    return items;
}

void SettingsDialog::setCoreVersion(CoreType coreType, const QString& version)
{
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        coreStatusRows_[key].versionText = version.trimmed();
        refreshCoreStatusPresentation();
    }
}

void SettingsDialog::beginCoreUpdate(CoreType coreType)
{
    updatingCoreType_ = coreType;
    tabWidget_->setCurrentIndex(3);
    setCoreUpdateProgress(coreType, tr("Checking..."));
}

void SettingsDialog::setCoreUpdateProgress(CoreType coreType, const QString& message)
{
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        coreStatusRows_[key].progressText = message.trimmed();
        refreshCoreStatusPresentation();
    }
}

void SettingsDialog::finishCoreUpdate(CoreType coreType, bool success, const QString& message)
{
    Q_UNUSED(message)
    updatingCoreType_ = CoreType::Unknown;
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        coreStatusRows_[key].progressText = success ? QString() : tr("Failed");
        refreshCoreStatusPresentation();
    }
}

void SettingsDialog::refreshCoreStatusPresentation()
{
    const bool updateRunning = updatingCoreType_ != CoreType::Unknown;

    for (auto it = coreStatusRows_.begin(); it != coreStatusRows_.end(); ++it) {
        const int key = it.key();
        const bool isActive = static_cast<CoreType>(key) == updatingCoreType_;
        auto& row = it.value();

        if (row.versionLabel != nullptr) {
            if (isActive) {
                row.versionLabel->setText(tr("Downloading..."));
            } else {
                row.versionLabel->setText(row.versionText.isEmpty() ? tr("Not found") : row.versionText);
            }
        }
        if (row.statusLabel != nullptr) {
            row.statusLabel->setText(coreStatusTextForProgress(row.progressText));
            row.statusLabel->setVisible(!isActive && !row.statusLabel->text().isEmpty());
        }
        if (row.downloadButton != nullptr) {
            row.downloadButton->setVisible(!isActive && row.versionText.isEmpty());
            row.downloadButton->setEnabled(!updateRunning);
        }
        if (row.setAllButton != nullptr) {
            row.setAllButton->setVisible(!isActive && !row.versionText.isEmpty());
        }
    }
}

QString SettingsDialog::coreStatusTextForProgress(const QString& message) const
{
    const QString trimmedMessage = message.trimmed();
    if (trimmedMessage.isEmpty()) {
        return QString();
    }

    const QString lowerMessage = trimmedMessage.toLower();
    if (lowerMessage == tr("Failed").toLower()) {
        return tr("Failed");
    }
    if (lowerMessage.contains(QStringLiteral("download"))
        || lowerMessage.contains(QStringLiteral("extract"))
        || lowerMessage.contains(QStringLiteral("install"))
        || lowerMessage.contains(QStringLiteral("preparing"))) {
        return tr("Downloading...");
    }
    if (lowerMessage.contains(QStringLiteral("checking"))
        || lowerMessage.contains(QStringLiteral("selected"))
        || lowerMessage.contains(QStringLiteral("current"))
        || lowerMessage.contains(QStringLiteral("latest"))
        || lowerMessage.contains(QStringLiteral("release metadata"))) {
        return tr("Checking...");
    }

    return tr("Checking...");
}
