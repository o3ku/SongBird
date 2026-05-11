#include "ui/dialogs/SettingsDialog.h"
#include "runtime/ProtocolCoreCompat.h"

#include <QCheckBox>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTabWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QItemSelectionModel>

#include <QWidget>

#include <algorithm>

#include "ui/theme/AppTheme.h"

namespace {

QString elideText(const QString& value, const QFontMetrics& metrics, int width)
{
    const QString trimmed = value.trimmed();
    if (metrics.horizontalAdvance(trimmed) <= width) {
        return trimmed;
    }

    const QString ellipsis = QStringLiteral("...");
    int length = trimmed.size();
    while (length > 0) {
        const QString candidate = trimmed.left(length).trimmed() + ellipsis;
        if (metrics.horizontalAdvance(candidate) <= width) {
            return candidate;
        }
        --length;
    }

    return ellipsis;
}

class BaseRouteCardButton final : public QPushButton
{
public:
    explicit BaseRouteCardButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setProperty("routeTitleBold", true);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);

        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        style()->drawControl(QStyle::CE_PushButtonBevel, &option, &painter, this);

        const QRect textRect = rect().adjusted(8, 8, -8, -8);
        const QStringList lines = text().split(QChar('\n'));
        QFont normalFont = font();
        QFont boldFont = normalFont;
        boldFont.setBold(true);

        painter.setPen(palette().buttonText().color());
        int y = textRect.top();
        for (int index = 0; index < lines.size(); ++index) {
            painter.setFont(index == 0 ? boldFont : normalFont);
            const QFontMetrics metrics(painter.font());
            const int lineHeight = metrics.lineSpacing();
            if (y + lineHeight > textRect.bottom() + 1) {
                break;
            }
            painter.drawText(
                QRect(textRect.left(), y, textRect.width(), lineHeight),
                Qt::AlignLeft | Qt::AlignVCenter,
                elideText(lines.at(index), metrics, textRect.width()));
            y += lineHeight;
        }
    }
};

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

QString normalizedRoutingCustomRuleTabKey(QString key)
{
    key = key.trimmed().toLower();
    return key == QStringLiteral("block") || key == QStringLiteral("proxy")
        ? key
        : QStringLiteral("direct");
}

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
    reloadRoutingPresentation(findInitialRouteIndex(routingItems_, config));
    loadRoutingCustomRules(config.routingCustomRules);
    selectRoutingCustomRuleTab(config.settingsRoutingRuleTabKey);

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

void SettingsDialog::setExistingCoreTypes(const QList<CoreType>& coreTypes)
{
    existingCoreTypes_.clear();
    for (const CoreType coreType : coreTypes) {
        const CoreType runtimeCore = resolveRuntimeCoreType(coreType);
        if (runtimeCore != CoreType::Unknown && !existingCoreTypes_.contains(runtimeCore)) {
            existingCoreTypes_.append(runtimeCore);
        }
    }
    reloadCoreTypeTable();
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
    updated.routingCustomRules = collectRoutingCustomRules();
    updated.enableRoutingAdvanced = !updated.routingItems.isEmpty();
    updated.routingIndex = selectedBaseRouteIndex() < 0 ? 0 : selectedBaseRouteIndex();
    updated.settingsRoutingRuleTabKey = selectedRoutingCustomRuleTabKey();
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
    AppTheme::applyCompactFont(this);

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
    AppTheme::applyCompactFont({
        showMainOnStartupCheck_,
        autoRunCheck_,
        allowLanConnectionCheck_,
        sniffingEnabledCheck_,
        routeOnlyCheck_,
        localPortSpin_,
        enableFragmentCheck_,
        enableCacheFile4SboxCheck_,
        defaultAllowInsecureCheck_,
        defaultFingerprintCombo_,
        defaultUserAgentCombo_,
        mux4SboxProtocolCombo_,
        enableStatisticsCheck_,
        statisticsFreshRateSpin_,
        trayMenuServersLimitSpin_,
        languageCombo_});

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
    AppTheme::applyCompactFont(subTable_);
    AppTheme::applyCompactFont(subTable_->horizontalHeader());

    addSubButton_ = new QPushButton(QStringLiteral("Add"), subTab);
    removeSubButton_ = new QPushButton(QStringLiteral("Remove"), subTab);
    updateSubButton_ = new QPushButton(QStringLiteral("Update Selected"), subTab);
    AppTheme::applyCompactFont({addSubButton_, removeSubButton_, updateSubButton_});

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

    auto* baseRouteTitle = new QLabel(tr("Base Route"), routingTab);
    baseRouteTitle->setObjectName(QStringLiteral("routingBaseRouteTitle"));
    AppTheme::applyCompactFont(baseRouteTitle);
    QFont baseRouteTitleFont = baseRouteTitle->font();
    baseRouteTitleFont.setBold(true);
    baseRouteTitle->setFont(baseRouteTitleFont);
    routingLayout->addWidget(baseRouteTitle);

    baseRouteLayout_ = new QHBoxLayout();
    baseRouteLayout_->setObjectName(QStringLiteral("routingBaseRouteLayout"));
    baseRouteLayout_->setContentsMargins(0, 0, 0, 0);
    baseRouteLayout_->setSpacing(8);
    baseRouteButtonGroup_ = new QButtonGroup(this);
    baseRouteButtonGroup_->setExclusive(true);
    connect(baseRouteButtonGroup_, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int) {
        updateBaseRouteCardGeometry();
    });
    routingLayout->addLayout(baseRouteLayout_);

    auto* customRulesTitle = new QLabel(tr("Custom Rules"), routingTab);
    customRulesTitle->setObjectName(QStringLiteral("routingCustomRulesTitle"));
    AppTheme::applyCompactFont(customRulesTitle);
    QFont customRulesTitleFont = customRulesTitle->font();
    customRulesTitleFont.setBold(true);
    customRulesTitle->setFont(customRulesTitleFont);
    routingLayout->addWidget(customRulesTitle);

    customRuleTabs_ = new QTabWidget(routingTab);
    customRuleTabs_->setObjectName(QStringLiteral("routingCustomRuleTabs"));
    AppTheme::applyCompactFont(customRuleTabs_);

    const QList<QPair<QString, QString>> customTabs{
        {QStringLiteral("block"), QStringLiteral("Block")},
        {QStringLiteral("direct"), QStringLiteral("Direct")},
        {QStringLiteral("proxy"), QStringLiteral("Proxy")}};
    for (const auto& tab : customTabs) {
        const QString action = tab.first;
        const QString title = tab.second;
        auto* tabWidget = new QWidget(customRuleTabs_);
        auto* tabLayout = new QHBoxLayout(tabWidget);
        tabLayout->setContentsMargins(9, 9, 9, 9);
        tabLayout->setSpacing(12);

        CustomRuleEditors editors;

        auto* protocolPortColumn = new QWidget(tabWidget);
        auto* protocolPortLayout = new QVBoxLayout(protocolPortColumn);
        protocolPortLayout->setContentsMargins(0, 0, 0, 0);
        protocolPortLayout->setSpacing(4);
        auto* protocolLabel = new QLabel(tr("Protocol"), protocolPortColumn);
        editors.protocolEdit = new QTextEdit(protocolPortColumn);
        editors.protocolEdit->setObjectName(QStringLiteral("routingCustom%1ProtocolEdit").arg(title));
        editors.protocolEdit->setTabChangesFocus(true);
        editors.protocolEdit->setPlaceholderText(QStringLiteral("tcp\nudp"));
        auto* portLabel = new QLabel(tr("Port"), protocolPortColumn);
        editors.portEdit = new QLineEdit(protocolPortColumn);
        editors.portEdit->setObjectName(QStringLiteral("routingCustom%1PortEdit").arg(title));
        editors.portEdit->setPlaceholderText(QStringLiteral("0-65535"));
        AppTheme::applyCompactFont({protocolLabel, editors.protocolEdit, portLabel, editors.portEdit});
        protocolPortLayout->addWidget(protocolLabel);
        protocolPortLayout->addWidget(editors.protocolEdit, 1);
        protocolPortLayout->addWidget(portLabel);
        protocolPortLayout->addWidget(editors.portEdit);

        auto* ipColumn = new QWidget(tabWidget);
        auto* ipLayout = new QVBoxLayout(ipColumn);
        ipLayout->setContentsMargins(0, 0, 0, 0);
        ipLayout->setSpacing(4);
        auto* ipLabel = new QLabel(QStringLiteral("IP"), ipColumn);
        editors.ipEdit = new QTextEdit(ipColumn);
        editors.ipEdit->setObjectName(QStringLiteral("routingCustom%1IpEdit").arg(title));
        editors.ipEdit->setTabChangesFocus(true);
        editors.ipEdit->setPlaceholderText(QStringLiteral("geoip:private\n10.0.0.0/8"));
        AppTheme::applyCompactFont({ipLabel, editors.ipEdit});
        ipLayout->addWidget(ipLabel);
        ipLayout->addWidget(editors.ipEdit, 1);

        auto* domainColumn = new QWidget(tabWidget);
        auto* domainLayout = new QVBoxLayout(domainColumn);
        domainLayout->setContentsMargins(0, 0, 0, 0);
        domainLayout->setSpacing(4);
        auto* domainLabel = new QLabel(tr("Domain"), domainColumn);
        editors.domainEdit = new QTextEdit(domainColumn);
        editors.domainEdit->setObjectName(QStringLiteral("routingCustom%1DomainEdit").arg(title));
        editors.domainEdit->setTabChangesFocus(true);
        editors.domainEdit->setPlaceholderText(QStringLiteral("geosite:cn\ndomain:example.com"));
        AppTheme::applyCompactFont({domainLabel, editors.domainEdit});
        domainLayout->addWidget(domainLabel);
        domainLayout->addWidget(editors.domainEdit, 1);

        tabLayout->addWidget(protocolPortColumn, 1);
        tabLayout->addWidget(ipColumn, 1);
        tabLayout->addWidget(domainColumn, 1);

        tabWidget->setProperty("routingCustomRuleTabKey", action);
        customRuleTabs_->addTab(tabWidget, title);
        customRuleEditors_.insert(action, editors);
    }
    routingLayout->addWidget(customRuleTabs_, 1);

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
    AppTheme::applyCompactFont(coreTypeGroup);

    for (int i = 0; i < protocolNames.size(); ++i) {
        const QList<CoreType> cores = supportedCoreTypes(static_cast<ConfigType>(configTypes.at(i)));
        if (cores.size() > 1) {
            auto* coreCombo = new QComboBox(coreTab);
            for (const CoreType core : cores) {
                coreCombo->addItem(coreTypeDisplayName(core), static_cast<int>(core));
            }
            coreCombo->setObjectName(QStringLiteral("coreTypeCombo_%1").arg(configTypes.at(i)));
            AppTheme::applyCompactFont(coreCombo);
            coreTypeForm->addRow(protocolNames.at(i), coreCombo);
            coreTypeCombos_.append(coreCombo);
            coreTypeComboConfigTypes_.append(configTypes.at(i));
        } else if (cores.size() == 1) {
            auto* fixedLabel = new QLabel(coreTypeDisplayName(cores.first()), coreTab);
            fixedLabel->setObjectName(QStringLiteral("coreTypeCombo_%1").arg(configTypes.at(i)));
            AppTheme::applyCompactFont(fixedLabel);
            coreTypeForm->addRow(protocolNames.at(i), fixedLabel);
            coreTypeCombos_.append(nullptr);
            coreTypeComboConfigTypes_.append(configTypes.at(i));
        }
    }

    // Installed Cores: one row per available core type, with download + "set all" actions
    auto* coreStatusGroup = new QGroupBox(tr("Installed Cores"), coreTab);
    auto* coreStatusLayout = new QFormLayout(coreStatusGroup);
    AppTheme::applyCompactFont(coreStatusGroup);

    for (const CoreType core : allCores) {
        const int coreKey = static_cast<int>(core);
        auto& row = coreStatusRows_[coreKey];

        row.versionLabel = new QLabel(coreTab);
        row.versionLabel->setObjectName(QStringLiteral("coreVersionLabel_%1").arg(coreKey));
        row.statusLabel = new QLabel(coreTab);
        row.statusLabel->setObjectName(QStringLiteral("coreStatusLabel_%1").arg(coreKey));
        row.downloadButton = new QPushButton(tr("Download"), coreTab);
        row.downloadButton->setObjectName(QStringLiteral("coreDownloadButton_%1").arg(coreKey));
        AppTheme::applyCompactFont({row.versionLabel, row.statusLabel, row.downloadButton});

        row.setAllButton = new QPushButton(tr("Set All"), coreTab);
        row.setAllButton->setToolTip(tr("Set all protocols to %1").arg(coreTypeDisplayName(core)));
        AppTheme::applyCompactFont(row.setAllButton);
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
    AppTheme::applyCompactFont({systemProxyExceptionsEdit_, systemProxyAdvancedProtocolCombo_});

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
    AppTheme::applyCompactFont({
        autoUpdateIntervalSpin_,
        autoUpdateSubIntervalSpin_,
        checkPreReleaseUpdateCheck_,
        ignoreGeoUpdateCoreCheck_,
        enableSecurityProtocolTls13Check_,
        updateNote});

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
    AppTheme::applyCompactFont({
        tunEnableCheck_,
        tunAutoRouteCheck_,
        tunStrictRouteCheck_,
        tunMtuSpin_,
        tunStackCombo_,
        tunEnableIPv6AddressCheck_,
        tunIcmpRoutingEdit_,
        tunEnableLegacyProtectCheck_});

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
    AppTheme::applyCompactFont({
        remoteDnsEdit_,
        directDnsEdit_,
        bootstrapDnsEdit_,
        domainStrategyForFreedomCombo_,
        domainStrategyForProxyCombo_,
        useSystemHostsCheck_,
        addCommonHostsCheck_,
        blockBindingQueryCheck_,
        fakeIpCheck_,
        globalFakeIpCheck_,
        serveStaleCheck_,
        parallelQueryCheck_,
        directExpectedIpsEdit_,
        dnsHostsEdit_});

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
    AppTheme::applyCompactFont(tabWidget_);
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
    AppTheme::applyCompactFont(restoreBackupButton_);
    connect(restoreBackupButton_, &QPushButton::clicked, this, [this]() {
        restoreBackupRequested_ = true;
    });

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    AppTheme::applyCompactFont(buttonBox_);

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
    connect(buttonBox_, &QDialogButtonBox::accepted, this, [this]() {
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

    reloadRoutingPresentation();
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

namespace {

QString compactRuleValues(const QStringList& values)
{
    QStringList normalized;
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            normalized.append(trimmed);
        }
    }
    return normalized.join(QStringLiteral(", "));
}

constexpr int kBaseRouteCardMaxWidth = 220;
constexpr int kBaseRouteCardTextWidth = 210;
constexpr int kBaseRouteCardCollapsedWidth = 100;

QString elideCardLine(const QString& value, const QFontMetrics& metrics)
{
    return elideText(value, metrics, kBaseRouteCardTextWidth);
}

void appendCardRuleLine(QStringList& lines, const QString& action, const QStringList& matches, const QFontMetrics& metrics)
{
    if (matches.isEmpty()) {
        return;
    }
    lines.append(elideCardLine(QStringLiteral("%1: %2").arg(action, matches.join(QStringLiteral(", "))), metrics));
}

QString baseRouteCardText(const RoutingItem& item, int index, const QFontMetrics& metrics)
{
    QStringList lines;
    const QString title = item.remarks.trimmed().isEmpty()
        ? QStringLiteral("Route %1").arg(index + 1)
        : item.remarks.trimmed();
    lines.append(elideCardLine(title, metrics));

    QStringList blockMatches;
    QStringList directMatches;
    QStringList proxyMatches;
    for (const RoutingRule& rule : item.rules) {
        if (!rule.enabled) {
            continue;
        }

        QStringList matches;
        const QString port = rule.port.trimmed();
        if (!port.isEmpty()) {
            matches.append(QStringLiteral("port: %1").arg(port));
        }
        const QString protocols = compactRuleValues(rule.protocol);
        if (!protocols.isEmpty()) {
            matches.append(QStringLiteral("protocol: %1").arg(protocols));
        }
        const QString ips = compactRuleValues(rule.ip);
        if (!ips.isEmpty()) {
            matches.append(ips);
        }
        const QString domains = compactRuleValues(rule.domain);
        if (!domains.isEmpty()) {
            matches.append(domains);
        }
        matches.removeAll(QString());
        if (matches.isEmpty()) {
            continue;
        }

        const QString outbound = rule.outboundTag.trimmed().toLower();
        QStringList* target = &proxyMatches;
        if (outbound == QStringLiteral("block")) {
            target = &blockMatches;
        } else if (outbound == QStringLiteral("direct")) {
            target = &directMatches;
        }
        target->append(matches);
    }

    appendCardRuleLine(lines, QStringLiteral("BLOCK"), blockMatches, metrics);
    appendCardRuleLine(lines, QStringLiteral("DIRECT"), directMatches, metrics);
    appendCardRuleLine(lines, QStringLiteral("PROXY"), proxyMatches, metrics);
    return lines.join(QChar('\n'));
}

QString baseRouteCardFullText(const RoutingItem& item, int index)
{
    QStringList lines;
    lines.append(item.remarks.trimmed().isEmpty()
        ? QStringLiteral("Route %1").arg(index + 1)
        : item.remarks.trimmed());

    QStringList blockMatches;
    QStringList directMatches;
    QStringList proxyMatches;
    for (const RoutingRule& rule : item.rules) {
        if (!rule.enabled) {
            continue;
        }

        QStringList matches;
        const QString port = rule.port.trimmed();
        if (!port.isEmpty()) {
            matches.append(QStringLiteral("port: %1").arg(port));
        }
        const QString protocols = compactRuleValues(rule.protocol);
        if (!protocols.isEmpty()) {
            matches.append(QStringLiteral("protocol: %1").arg(protocols));
        }
        const QString ips = compactRuleValues(rule.ip);
        if (!ips.isEmpty()) {
            matches.append(ips);
        }
        const QString domains = compactRuleValues(rule.domain);
        if (!domains.isEmpty()) {
            matches.append(domains);
        }
        matches.removeAll(QString());
        if (matches.isEmpty()) {
            continue;
        }

        const QString outbound = rule.outboundTag.trimmed().toLower();
        QStringList* target = &proxyMatches;
        if (outbound == QStringLiteral("block")) {
            target = &blockMatches;
        } else if (outbound == QStringLiteral("direct")) {
            target = &directMatches;
        }
        target->append(matches);
    }

    if (!blockMatches.isEmpty()) {
        lines.append(QStringLiteral("BLOCK: %1").arg(blockMatches.join(QStringLiteral(", "))));
    }
    if (!directMatches.isEmpty()) {
        lines.append(QStringLiteral("DIRECT: %1").arg(directMatches.join(QStringLiteral(", "))));
    }
    if (!proxyMatches.isEmpty()) {
        lines.append(QStringLiteral("PROXY: %1").arg(proxyMatches.join(QStringLiteral(", "))));
    }
    return lines.join(QChar('\n'));
}

void clearLayout(QLayout* layout)
{
    if (layout == nullptr) {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

} // namespace

void SettingsDialog::reloadRoutingPresentation(int selectedRow)
{
    if (baseRouteLayout_ == nullptr || baseRouteButtonGroup_ == nullptr) {
        return;
    }

    const QList<QAbstractButton*> oldButtons = baseRouteButtonGroup_->buttons();
    for (QAbstractButton* button : oldButtons) {
        baseRouteButtonGroup_->removeButton(button);
    }
    clearLayout(baseRouteLayout_);

    int rowToSelect = selectedRow;
    if (rowToSelect < 0 || rowToSelect >= routingItems_.size()) {
        rowToSelect = routingItems_.isEmpty() ? -1 : 0;
    }

    for (int index = 0; index < routingItems_.size(); ++index) {
        auto* card = new BaseRouteCardButton(this);
        const QString collapsedText = baseRouteCardText(routingItems_.at(index), index, card->fontMetrics());
        const QString fullText = baseRouteCardFullText(routingItems_.at(index), index);
        card->setObjectName(QStringLiteral("routingBaseRouteCard_%1").arg(index));
        card->setCheckable(true);
        card->setProperty("collapsedText", collapsedText);
        card->setProperty("fullText", fullText);
        card->setProperty("baseRouteCard", true);
        card->setText(index == rowToSelect ? fullText : collapsedText);
        card->setToolTip(fullText);
        card->setMinimumHeight(96);
        card->setMaximumHeight(120);
        card->setChecked(index == rowToSelect);
        baseRouteButtonGroup_->addButton(card, index);
        baseRouteLayout_->addWidget(card, 0);
    }
    updateBaseRouteCardGeometry();
    updateRoutingActionState();
}

void SettingsDialog::updateBaseRouteCardGeometry()
{
    if (baseRouteButtonGroup_ == nullptr) {
        return;
    }

    const int selectedId = baseRouteButtonGroup_->checkedId();
    for (QAbstractButton* button : baseRouteButtonGroup_->buttons()) {
        if (button == nullptr) {
            continue;
        }

        const bool selected = baseRouteButtonGroup_->id(button) == selectedId;
        if (selected) {
            button->setMinimumWidth(kBaseRouteCardMaxWidth);
            button->setMaximumWidth(QWIDGETSIZE_MAX);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            button->setText(button->property("fullText").toString());
        } else {
            button->setMinimumWidth(kBaseRouteCardCollapsedWidth);
            button->setMaximumWidth(kBaseRouteCardCollapsedWidth);
            button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            button->setText(button->property("collapsedText").toString());
        }

        if (baseRouteLayout_ != nullptr) {
            const int layoutIndex = baseRouteLayout_->indexOf(button);
            if (layoutIndex >= 0) {
                baseRouteLayout_->setStretch(layoutIndex, selected ? 1 : 0);
            }
        }
    }

    if (baseRouteLayout_ != nullptr) {
        baseRouteLayout_->invalidate();
    }
}

void SettingsDialog::loadRoutingCustomRules(const QList<RoutingRule>& rules)
{
    for (auto it = customRuleEditors_.begin(); it != customRuleEditors_.end(); ++it) {
        if (it.value().protocolEdit != nullptr) {
            it.value().protocolEdit->clear();
        }
        if (it.value().portEdit != nullptr) {
            it.value().portEdit->clear();
        }
        if (it.value().ipEdit != nullptr) {
            it.value().ipEdit->clear();
        }
        if (it.value().domainEdit != nullptr) {
            it.value().domainEdit->clear();
        }
    }

    struct RuleValues {
        QStringList protocols;
        QStringList ports;
        QStringList ips;
        QStringList domains;
    };
    QMap<QString, RuleValues> valuesByAction;
    const auto appendUnique = [](QStringList& target, const QStringList& values) {
        for (const QString& value : values) {
            const QString trimmed = value.trimmed();
            if (!trimmed.isEmpty() && !target.contains(trimmed)) {
                target.append(trimmed);
            }
        }
    };

    for (const RoutingRule& rule : rules) {
        const QString action = rule.outboundTag.trimmed().toLower();
        if (!customRuleEditors_.contains(action)) {
            continue;
        }

        RuleValues& values = valuesByAction[action];
        appendUnique(values.protocols, rule.protocol);
        appendUnique(values.ips, rule.ip);
        appendUnique(values.domains, rule.domain);
        const QString port = rule.port.trimmed();
        if (!port.isEmpty() && !values.ports.contains(port)) {
            values.ports.append(port);
        }
    }

    for (auto it = valuesByAction.cbegin(); it != valuesByAction.cend(); ++it) {
        auto editorIt = customRuleEditors_.find(it.key());
        if (editorIt == customRuleEditors_.end()) {
            continue;
        }

        const RuleValues& values = it.value();
        CustomRuleEditors& editors = editorIt.value();
        if (editors.protocolEdit != nullptr) {
            editors.protocolEdit->setPlainText(values.protocols.join(QChar('\n')));
        }
        if (editors.portEdit != nullptr) {
            editors.portEdit->setText(joinValues(values.ports));
        }
        if (editors.ipEdit != nullptr) {
            editors.ipEdit->setPlainText(values.ips.join(QChar('\n')));
        }
        if (editors.domainEdit != nullptr) {
            editors.domainEdit->setPlainText(values.domains.join(QChar('\n')));
        }
    }
}

QList<RoutingItem> SettingsDialog::collectRoutingItems() const
{
    QList<RoutingItem> items = routingItems_;
    const int selectedIndex = selectedBaseRouteIndex();
    for (int index = 0; index < items.size(); ++index) {
        items[index].enabled = true;
        items[index].locked = index == selectedIndex;
    }
    return items;
}

QList<RoutingRule> SettingsDialog::collectRoutingCustomRules() const
{
    QList<RoutingRule> rules;
    const QStringList actionOrder{
        QStringLiteral("block"),
        QStringLiteral("direct"),
        QStringLiteral("proxy")};

    for (const QString& action : actionOrder) {
        const CustomRuleEditors editors = customRuleEditors_.value(action);

        const QString protocolText = editors.protocolEdit == nullptr ? QString() : editors.protocolEdit->toPlainText();
        const QString portText = editors.portEdit == nullptr ? QString() : editors.portEdit->text().trimmed();
        const QStringList protocols = splitValues(protocolText);
        if (!protocols.isEmpty() || !portText.isEmpty()) {
            RoutingRule rule;
            rule.type = QStringLiteral("field");
            rule.enabled = true;
            rule.outboundTag = action;
            rule.protocol = protocols;
            rule.port = portText;
            rules.append(rule);
        }

        const QStringList ips = editors.ipEdit == nullptr
            ? QStringList()
            : splitValues(editors.ipEdit->toPlainText());
        if (!ips.isEmpty()) {
            RoutingRule rule;
            rule.type = QStringLiteral("field");
            rule.enabled = true;
            rule.outboundTag = action;
            rule.ip = ips;
            rules.append(rule);
        }

        const QStringList domains = editors.domainEdit == nullptr
            ? QStringList()
            : splitValues(editors.domainEdit->toPlainText());
        if (!domains.isEmpty()) {
            RoutingRule rule;
            rule.type = QStringLiteral("field");
            rule.enabled = true;
            rule.outboundTag = action;
            rule.domain = domains;
            rules.append(rule);
        }
    }

    return rules;
}

void SettingsDialog::selectRoutingCustomRuleTab(const QString& key)
{
    if (customRuleTabs_ == nullptr) {
        return;
    }

    const QString normalizedKey = normalizedRoutingCustomRuleTabKey(key);
    for (int index = 0; index < customRuleTabs_->count(); ++index) {
        QWidget* tab = customRuleTabs_->widget(index);
        if (tab != nullptr && tab->property("routingCustomRuleTabKey").toString() == normalizedKey) {
            customRuleTabs_->setCurrentIndex(index);
            return;
        }
    }
}

QString SettingsDialog::selectedRoutingCustomRuleTabKey() const
{
    if (customRuleTabs_ == nullptr || customRuleTabs_->currentIndex() < 0) {
        return QStringLiteral("direct");
    }

    const QWidget* tab = customRuleTabs_->widget(customRuleTabs_->currentIndex());
    return normalizedRoutingCustomRuleTabKey(
        tab == nullptr ? QString() : tab->property("routingCustomRuleTabKey").toString());
}

void SettingsDialog::updateRoutingActionState()
{
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

int SettingsDialog::selectedBaseRouteIndex() const
{
    if (baseRouteButtonGroup_ == nullptr) {
        return -1;
    }

    return baseRouteButtonGroup_->checkedId();
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
    for (int i = 0; i < coreTypeCombos_.size() && i < coreTypeComboConfigTypes_.size(); ++i) {
        auto* combo = coreTypeCombos_.at(i);
        if (combo == nullptr) {
            continue;
        }

        int configType = coreTypeComboConfigTypes_.at(i);
        int coreType = static_cast<int>(resolveExistingCoreTypeForProtocol(
            static_cast<ConfigType>(configType),
            existingCoreTypes_));

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

    for (int i = 0; i < coreTypeCombos_.size() && i < coreTypeComboConfigTypes_.size(); ++i) {
        const int configType = coreTypeComboConfigTypes_.at(i);
        auto* combo = coreTypeCombos_.at(i);

        CoreTypeItem item;
        item.configType = configType;
        if (combo != nullptr) {
            bool ok = false;
            const int coreTypeValue = combo->currentData().toInt(&ok);
            item.coreType = ok
                ? coreTypeValue
                : static_cast<int>(defaultCoreTypeForProtocol(static_cast<ConfigType>(configType)));
        } else {
            const QList<CoreType> cores = supportedCoreTypes(static_cast<ConfigType>(configType));
            item.coreType = cores.isEmpty()
                ? static_cast<int>(defaultCoreTypeForProtocol(static_cast<ConfigType>(configType)))
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
