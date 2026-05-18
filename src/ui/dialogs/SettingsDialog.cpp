#include "ui/dialogs/SettingsDialog.h"
#include "common/DialogUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFontMetrics>
#include <QSpinBox>
#include <QStackedLayout>
#include <QTabBar>
#include <QVBoxLayout>

#include <QWidget>

#include "ui/theme/AppTheme.h"

namespace {

class SettingsTabBar final : public QTabBar
{
public:
    explicit SettingsTabBar(QWidget* parent = nullptr)
        : QTabBar(parent)
    {
    }

    QSize tabSizeHint(int index) const override
    {
        QSize hint = QTabBar::tabSizeHint(index);
        constexpr int boldTextSafetyPadding = 8;

        QFont baseFont = font();
        QFont boldFont = baseFont;
        boldFont.setWeight(QFont::DemiBold);

        const QString label = tabText(index);
        const int baseWidth = QFontMetrics(baseFont).horizontalAdvance(label);
        const int boldWidth = QFontMetrics(boldFont).horizontalAdvance(label);
        hint.rwidth() += qMax(0, boldWidth - baseWidth) + boldTextSafetyPadding;

        return hint;
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

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setMinimumWidth(720);
    setupUi();
    updateFieldState();
}

void SettingsDialog::setConfig(const Config& config)
{
    config_ = config;

    if (generalSettingsPage_ != nullptr) {
        generalSettingsPage_->setConfig(config);
    }
    systemProxyExceptionsEdit_->setText(config.systemProxyExceptions);
    systemProxyAdvancedProtocolCombo_->setCurrentText(config.systemProxyAdvancedProtocol);
    pacUrlEdit_->setText(config.pacUrl);
    checkPreReleaseUpdateCheck_->setChecked(config.checkPreReleaseUpdate);
    ignoreGeoUpdateCoreCheck_->setChecked(config.ignoreGeoUpdateCore);

    if (routingSettingsPage_ != nullptr) {
        routingSettingsPage_->setConfig(config);
    }

    if (subscriptionSettingsPage_ != nullptr) {
        subscriptionSettingsPage_->setSubscriptions(config.subscriptions);
    }

    if (coreSettingsPage_ != nullptr) {
        coreSettingsPage_->setConfig(config);
    }

    const TunModeItem& tun = config.tunModeItem;
    tunEnableCheck_->setChecked(tun.enableTun);
    tunAutoRouteCheck_->setChecked(tun.autoRoute);
    tunStrictRouteCheck_->setChecked(tun.strictRoute);
    tunMtuSpin_->setValue(tun.mtu > 0 ? tun.mtu : 9000);
    tunStackCombo_->setCurrentText(tun.stack.isEmpty() ? QStringLiteral("system") : tun.stack);
    tunEnableIPv6AddressCheck_->setChecked(tun.enableIPv6Address);
    tunIcmpRoutingEdit_->setText(tun.icmpRouting);

    if (dnsSettingsPage_ != nullptr) {
        dnsSettingsPage_->setConfig(config);
    }

    const int targetTabIndex = settingsTabBar_ == nullptr || settingsTabBar_->count() == 0
        ? 0
        : qBound(0, requestedTabIndex_, settingsTabBar_->count() - 1);
    if (settingsTabBar_ != nullptr) {
        settingsTabBar_->setCurrentIndex(targetTabIndex);
    }
    if (settingsStackLayout_ != nullptr) {
        settingsStackLayout_->setCurrentIndex(targetTabIndex);
    }
    requestedTabIndex_ = 0;
    updateFieldState();
}

void SettingsDialog::setExistingCoreTypes(const QList<CoreType>& coreTypes)
{
    if (coreSettingsPage_ != nullptr) {
        coreSettingsPage_->setExistingCoreTypes(coreTypes);
    }
}

Config SettingsDialog::config() const
{
    Config updated = config_;
    if (generalSettingsPage_ != nullptr) {
        generalSettingsPage_->applyToConfig(updated);
    }
    updated.systemProxyExceptions = systemProxyExceptionsEdit_->text().trimmed();
    updated.systemProxyAdvancedProtocol = systemProxyAdvancedProtocolCombo_->currentText().trimmed();
    updated.pacUrl = pacUrlEdit_->text().trimmed();
    updated.checkPreReleaseUpdate = checkPreReleaseUpdateCheck_->isChecked();
    updated.ignoreGeoUpdateCore = ignoreGeoUpdateCoreCheck_->isChecked();
    if (routingSettingsPage_ != nullptr) {
        updated.routingItems = routingSettingsPage_->routingItems();
        updated.routingCustomRules = routingSettingsPage_->routingCustomRules();
        updated.enableRoutingAdvanced = !updated.routingItems.isEmpty();
        updated.routingIndex = 0;
        for (int index = 0; index < updated.routingItems.size(); ++index) {
            if (updated.routingItems.at(index).locked) {
                updated.routingIndex = index;
                break;
            }
        }
        updated.settingsRoutingRuleTabKey = routingSettingsPage_->settingsRoutingRuleTabKey();
    }
    if (subscriptionSettingsPage_ != nullptr) {
        updated.subscriptions = subscriptionSettingsPage_->subscriptions();
    }
    if (coreSettingsPage_ != nullptr) {
        updated.enableCacheFile4Sbox = coreSettingsPage_->enableCacheFile4Sbox();
        updated.mux4SboxProtocol = coreSettingsPage_->mux4SboxProtocol();
        updated.coreTypeItems = coreSettingsPage_->collectCoreTypeItems();
    }
    TunModeItem tun;
    tun.enableTun = tunEnableCheck_->isChecked();
    tun.autoRoute = tunAutoRouteCheck_->isChecked();
    tun.strictRoute = tunStrictRouteCheck_->isChecked();
    tun.mtu = tunMtuSpin_->value();
    tun.stack = tunStackCombo_->currentText().trimmed();
    tun.enableIPv6Address = tunEnableIPv6AddressCheck_->isChecked();
    tun.icmpRouting = tunIcmpRoutingEdit_->text().trimmed();
    if (coreSettingsPage_ != nullptr) {
        tun.enableLegacyProtect = coreSettingsPage_->legacyProtectEnabled();
    }
    updated.tunModeItem = tun;

    if (dnsSettingsPage_ != nullptr) {
        dnsSettingsPage_->applyToConfig(updated);
    }

    return updated;
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Settings"));
    resize(720, 540);
    AppTheme::applyCompactFont(this);

    // === General Tab ===
    auto* generalTab = new QWidget(this);
    auto* generalLayout = new QVBoxLayout(generalTab);
    generalSettingsPage_ = new GeneralSettingsPageWidget(generalTab);
    generalLayout->addWidget(generalSettingsPage_);

    // === Subscriptions Tab ===
    auto* subTab = new QWidget(this);
    auto* subLayout = new QVBoxLayout(subTab);
    subscriptionSettingsPage_ = new SubscriptionSettingsPageWidget(subTab);
    subLayout->addWidget(subscriptionSettingsPage_);

    // === Routing Tab ===
    auto* routingTab = new QWidget(this);
    auto* routingLayout = new QVBoxLayout(routingTab);
    routingSettingsPage_ = new RoutingSettingsPageWidget(routingTab);
    routingLayout->addWidget(routingSettingsPage_);

    // === Core Tab ===
    auto* coreTab = new QWidget(this);
    auto* coreLayout = new QVBoxLayout(coreTab);
    coreSettingsPage_ = new CoreSettingsPageWidget(coreTab);
    coreSettingsPage_->setCoreDownloadHandler([this](CoreType core) {
        coreDownloadRequested_ = true;
        requestedCoreDownload_ = core;
        emit coreDownloadRequested(static_cast<int>(core));
    });
    coreLayout->addWidget(coreSettingsPage_);

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
    pacUrlEdit_->setVisible(false);

    // === Update Tab ===
    auto* updateTab = new QWidget(this);
    auto* updateLayout = new QFormLayout(updateTab);

    checkPreReleaseUpdateCheck_ = new QCheckBox(tr("Include pre-release builds"), updateTab);
    ignoreGeoUpdateCoreCheck_ = new QCheckBox(tr("Do not overwrite geo files during core updates"), updateTab);

    AppTheme::applyCompactFont({
        checkPreReleaseUpdateCheck_,
        ignoreGeoUpdateCoreCheck_});

    updateLayout->setWidget(updateLayout->rowCount(), QFormLayout::SpanningRole, checkPreReleaseUpdateCheck_);
    updateLayout->setWidget(updateLayout->rowCount(), QFormLayout::SpanningRole, ignoreGeoUpdateCoreCheck_);

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
    AppTheme::applyCompactFont({
        tunEnableCheck_,
        tunAutoRouteCheck_,
        tunStrictRouteCheck_,
        tunMtuSpin_,
        tunStackCombo_,
        tunEnableIPv6AddressCheck_,
        tunIcmpRoutingEdit_});

    tunLayout->setWidget(tunLayout->rowCount(), QFormLayout::SpanningRole, tunEnableCheck_);
    tunLayout->setWidget(tunLayout->rowCount(), QFormLayout::SpanningRole, tunAutoRouteCheck_);
    tunLayout->setWidget(tunLayout->rowCount(), QFormLayout::SpanningRole, tunStrictRouteCheck_);
    tunLayout->addRow(tr("MTU"), tunMtuSpin_);
    tunLayout->addRow(tr("Stack"), tunStackCombo_);
    tunLayout->setWidget(tunLayout->rowCount(), QFormLayout::SpanningRole, tunEnableIPv6AddressCheck_);
    tunLayout->addRow(tr("ICMP Routing"), tunIcmpRoutingEdit_);
    // === DNS Tab ===
    auto* dnsTab = new QWidget(this);
    auto* dnsLayout = new QVBoxLayout(dnsTab);
    dnsSettingsPage_ = new DnsSettingsPageWidget(dnsTab);
    dnsLayout->addWidget(dnsSettingsPage_);

    // === Settings Pages ===
    settingsTabBar_ = new SettingsTabBar(this);
    settingsTabBar_->setObjectName(QStringLiteral("settingsTabBar"));
    settingsTabBar_->setExpanding(false);
    settingsTabBar_->setDrawBase(false);
    AppTheme::applyCompactFont(settingsTabBar_);

    auto* settingsStackContainer = new QWidget(this);
    settingsStackContainer->setObjectName(QStringLiteral("settingsStackContainer"));
    settingsStackLayout_ = new QStackedLayout(settingsStackContainer);
    settingsStackLayout_->setObjectName(QStringLiteral("settingsStackLayout"));
    settingsStackLayout_->setContentsMargins(0, 0, 0, 0);
    auto addSettingsTab = [this](QWidget* page, const QString& title) {
        settingsStackLayout_->addWidget(page);
        settingsTabBar_->addTab(title);
    };
    addSettingsTab(generalTab, tr("General"));
    addSettingsTab(subTab, tr("Subscriptions"));
    addSettingsTab(routingTab, tr("Routing"));
    addSettingsTab(coreTab, tr("Core"));
    addSettingsTab(proxyTab, tr("Proxy"));
    addSettingsTab(tunTab, QStringLiteral("TUN"));
    addSettingsTab(dnsTab, QStringLiteral("DNS"));
    addSettingsTab(updateTab, tr("Update"));

    connect(settingsTabBar_, &QTabBar::currentChanged, settingsStackLayout_, &QStackedLayout::setCurrentIndex);
    connect(settingsStackLayout_, &QStackedLayout::currentChanged, settingsTabBar_, &QTabBar::setCurrentIndex);

    auto* settingsTabBarContainer = new QWidget(this);
    settingsTabBarContainer->setObjectName(QStringLiteral("settingsTabBarContainer"));
    auto* settingsTabBarLayout = new QVBoxLayout(settingsTabBarContainer);
    settingsTabBarLayout->setContentsMargins(0, 0, 0, 0);
    settingsTabBarLayout->setSpacing(0);
    settingsTabBarLayout->addWidget(settingsTabBar_);

    // === Button Box with Restore ===
    restoreBackupButton_ = new QPushButton(tr("Restore Backup"), this);
    restoreBackupButton_->setObjectName(QStringLiteral("settingsRestoreBackupButton"));
    AppTheme::applyCompactFont(restoreBackupButton_);
    connect(restoreBackupButton_, &QPushButton::clicked, this, [this]() {
        restoreBackupRequested_ = true;
    });

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    DialogUtils::localizeStandardDialogButtonBox(buttonBox_);
    AppTheme::applyCompactFont(buttonBox_);

    auto* settingsActionBar = new QWidget(this);
    settingsActionBar->setObjectName(QStringLiteral("settingsActionBar"));
    auto* bottomLayout = new QHBoxLayout(settingsActionBar);
    bottomLayout->addWidget(restoreBackupButton_);
    bottomLayout->addStretch();
    bottomLayout->addWidget(buttonBox_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->addWidget(settingsTabBarContainer);
    rootLayout->addWidget(settingsStackContainer);
    rootLayout->addWidget(settingsActionBar);

    connect(tunEnableCheck_, &QCheckBox::toggled, this, [this](bool) {
        updateFieldState();
    });
    connect(buttonBox_, &QDialogButtonBox::accepted, this, [this]() {
        accept();
    });
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(subscriptionSettingsPage_, &SubscriptionSettingsPageWidget::updateRequested, this, [this]() {
        updateSubRequested_ = true;
    });
}

void SettingsDialog::updateFieldState()
{
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
    if (coreSettingsPage_ != nullptr) {
        coreSettingsPage_->setTunOptionsEnabled(tunEnabled);
    }
}

QList<int> SettingsDialog::selectedSubRows() const
{
    return subscriptionSettingsPage_ == nullptr ? QList<int>() : subscriptionSettingsPage_->selectedRows();
}

void SettingsDialog::setCoreVersion(CoreType coreType, const QString& version)
{
    if (coreSettingsPage_ != nullptr) {
        coreSettingsPage_->setCoreVersion(coreType, version);
    }
}

void SettingsDialog::beginCoreUpdate(CoreType coreType)
{
    if (settingsTabBar_ != nullptr) {
        settingsTabBar_->setCurrentIndex(3);
    }
    if (settingsStackLayout_ != nullptr) {
        settingsStackLayout_->setCurrentIndex(3);
    }
    if (coreSettingsPage_ != nullptr) {
        coreSettingsPage_->beginCoreUpdate(coreType);
    }
}

void SettingsDialog::setCoreUpdateProgress(CoreType coreType, const QString& message)
{
    if (coreSettingsPage_ != nullptr) {
        coreSettingsPage_->setCoreUpdateProgress(coreType, message);
    }
}

void SettingsDialog::finishCoreUpdate(CoreType coreType, bool success, const QString& message)
{
    if (coreSettingsPage_ != nullptr) {
        coreSettingsPage_->finishCoreUpdate(coreType, success, message);
    }
}
