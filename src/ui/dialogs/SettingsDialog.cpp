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
}

void SettingsDialog::setConfig(const Config& config)
{
    config_ = config;

    if (generalSettingsPage_ != nullptr) {
        generalSettingsPage_->setConfig(config);
    }
    if (proxySettingsPage_ != nullptr) {
        proxySettingsPage_->setConfig(config);
    }
    if (updateSettingsPage_ != nullptr) {
        updateSettingsPage_->setConfig(config);
    }

    if (routingSettingsPage_ != nullptr) {
        routingSettingsPage_->setConfig(config);
    }

    if (subscriptionSettingsPage_ != nullptr) {
        subscriptionSettingsPage_->setSubscriptions(config.subscriptions);
    }

    if (coreSettingsPage_ != nullptr) {
        coreSettingsPage_->setConfig(config);
    }

    if (tunSettingsPage_ != nullptr) {
        tunSettingsPage_->setConfig(config);
    }

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
    if (coreSettingsPage_ != nullptr && tunSettingsPage_ != nullptr) {
        coreSettingsPage_->setTunOptionsEnabled(tunSettingsPage_->tunEnabled());
    }
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
    if (proxySettingsPage_ != nullptr) {
        proxySettingsPage_->applyToConfig(updated);
    }
    if (updateSettingsPage_ != nullptr) {
        updateSettingsPage_->applyToConfig(updated);
    }
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
    if (tunSettingsPage_ != nullptr) {
        tunSettingsPage_->applyToConfig(updated);
    }
    if (coreSettingsPage_ != nullptr) {
        updated.tunModeItem.enableLegacyProtect = coreSettingsPage_->legacyProtectEnabled();
    }

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
    auto* proxyLayout = new QVBoxLayout(proxyTab);
    proxySettingsPage_ = new ProxySettingsPageWidget(proxyTab);
    proxyLayout->addWidget(proxySettingsPage_);

    // === Update Tab ===
    auto* updateTab = new QWidget(this);
    auto* updateLayout = new QVBoxLayout(updateTab);
    updateSettingsPage_ = new UpdateSettingsPageWidget(updateTab);
    updateLayout->addWidget(updateSettingsPage_);

    // === TUN Tab ===
    auto* tunTab = new QWidget(this);
    auto* tunLayout = new QVBoxLayout(tunTab);
    tunSettingsPage_ = new TunSettingsPageWidget(tunTab);
    tunLayout->addWidget(tunSettingsPage_);
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

    connect(buttonBox_, &QDialogButtonBox::accepted, this, [this]() {
        accept();
    });
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(subscriptionSettingsPage_, &SubscriptionSettingsPageWidget::updateRequested, this, [this]() {
        updateSubRequested_ = true;
    });
    connect(tunSettingsPage_, &TunSettingsPageWidget::tunEnabledChanged, this, [this](bool enabled) {
        if (coreSettingsPage_ != nullptr) {
            coreSettingsPage_->setTunOptionsEnabled(enabled);
        }
    });
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
