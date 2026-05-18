#include "ui/dialogs/GeneralSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

#include "ui/theme/AppTheme.h"

namespace {

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

} // namespace

GeneralSettingsPageWidget::GeneralSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    updateFieldState();
}

void GeneralSettingsPageWidget::setConfig(const Config& config)
{
    showMainOnStartupCheck_->setChecked(config.showMainOnStartup);
    autoRunCheck_->setChecked(config.autoRunEnabled);
    allowLanConnectionCheck_->setChecked(config.allowLanConnection);
    sniffingEnabledCheck_->setChecked(config.sniffingEnabled);
    routeOnlyCheck_->setChecked(config.routeOnly);
    localPortSpin_->setValue(config.localPort > 0 ? config.localPort : 10808);
    enableFragmentCheck_->setChecked(config.enableFragment);
    defaultAllowInsecureCheck_->setChecked(config.defaultAllowInsecure);
    defaultFingerprintCombo_->setCurrentText(config.defaultFingerprint);
    defaultUserAgentCombo_->setCurrentText(config.defaultUserAgent);
    enableStatisticsCheck_->setChecked(config.enableStatistics);
    statisticsFreshRateSpin_->setValue(qMax(1, config.statisticsFreshRate));
    trayMenuServersLimitSpin_->setValue(qMax(0, config.trayMenuServersLimit));

    const QString normalizedLanguage = config.languageCode.trimmed();
    const int languageIndex = languageCombo_->findData(normalizedLanguage);
    languageCombo_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);

    updateFieldState();
}

void GeneralSettingsPageWidget::applyToConfig(Config& config) const
{
    config.showMainOnStartup = showMainOnStartupCheck_->isChecked();
    config.autoRunEnabled = autoRunCheck_->isChecked();
    config.allowLanConnection = allowLanConnectionCheck_->isChecked();
    config.sniffingEnabled = sniffingEnabledCheck_->isChecked();
    config.routeOnly = routeOnlyCheck_->isChecked();
    config.localPort = localPortSpin_->value();
    config.enableFragment = enableFragmentCheck_->isChecked();
    config.defaultAllowInsecure = defaultAllowInsecureCheck_->isChecked();
    config.defaultFingerprint = defaultFingerprintCombo_->currentText().trimmed();
    config.defaultUserAgent = defaultUserAgentCombo_->currentText().trimmed();
    config.enableStatistics = enableStatisticsCheck_->isChecked();
    config.statisticsFreshRate = statisticsFreshRateSpin_->value();
    config.trayMenuServersLimit = trayMenuServersLimitSpin_->value();
    config.languageCode = languageCombo_->currentData().toString().trimmed();
}

void GeneralSettingsPageWidget::setupUi()
{
    auto* generalLayout = new QFormLayout(this);

    showMainOnStartupCheck_ = new QCheckBox(tr("Show the main window after startup"), this);
    autoRunCheck_ = new QCheckBox(tr("Enable auto run"), this);
    allowLanConnectionCheck_ = new QCheckBox(tr("Allow LAN connections"), this);
    sniffingEnabledCheck_ = new QCheckBox(tr("Enable traffic sniffing"), this);
    sniffingEnabledCheck_->setObjectName(QStringLiteral("settingsSniffingEnabledCheck"));
    routeOnlyCheck_ = new QCheckBox(tr("Route only when sniffing"), this);
    routeOnlyCheck_->setObjectName(QStringLiteral("settingsRouteOnlyCheck"));

    localPortSpin_ = new QSpinBox(this);
    localPortSpin_->setObjectName(QStringLiteral("settingsLocalPortSpin"));
    localPortSpin_->setRange(1, 65535);

    enableFragmentCheck_ = new QCheckBox(tr("Enable fragmentation for TLS outbounds"), this);
    enableFragmentCheck_->setObjectName(QStringLiteral("settingsEnableFragmentCheck"));

    defaultAllowInsecureCheck_ = new QCheckBox(tr("Allow insecure TLS by default"), this);
    defaultAllowInsecureCheck_->setObjectName(QStringLiteral("settingsDefaultAllowInsecureCheck"));

    defaultFingerprintCombo_ = new QComboBox(this);
    defaultFingerprintCombo_->setObjectName(QStringLiteral("settingsDefaultFingerprintCombo"));
    defaultFingerprintCombo_->setEditable(true);
    defaultFingerprintCombo_->addItems(defaultFingerprintOptions());

    defaultUserAgentCombo_ = new QComboBox(this);
    defaultUserAgentCombo_->setObjectName(QStringLiteral("settingsDefaultUserAgentCombo"));
    defaultUserAgentCombo_->setEditable(true);
    defaultUserAgentCombo_->addItems(defaultUserAgentOptions());

    enableStatisticsCheck_ = new QCheckBox(tr("Enable traffic statistics"), this);
    enableStatisticsCheck_->setObjectName(QStringLiteral("settingsEnableStatisticsCheck"));
    statisticsFreshRateSpin_ = new QSpinBox(this);
    statisticsFreshRateSpin_->setObjectName(QStringLiteral("settingsStatisticsFreshRateSpin"));
    statisticsFreshRateSpin_->setRange(1, 100);
    statisticsFreshRateSpin_->setSuffix(tr(" s"));

    trayMenuServersLimitSpin_ = new QSpinBox(this);
    trayMenuServersLimitSpin_->setObjectName(QStringLiteral("settingsTrayMenuServersLimitSpin"));
    trayMenuServersLimitSpin_->setRange(0, 999);
    trayMenuServersLimitSpin_->setSpecialValueText(tr("Auto"));

    languageCombo_ = new QComboBox(this);
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
        defaultAllowInsecureCheck_,
        defaultFingerprintCombo_,
        defaultUserAgentCombo_,
        enableStatisticsCheck_,
        statisticsFreshRateSpin_,
        trayMenuServersLimitSpin_,
        languageCombo_});

    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, showMainOnStartupCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, autoRunCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, allowLanConnectionCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, sniffingEnabledCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, routeOnlyCheck_);
    generalLayout->addRow(tr("Local Port"), localPortSpin_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, enableFragmentCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, defaultAllowInsecureCheck_);
    generalLayout->addRow(tr("Default Fingerprint"), defaultFingerprintCombo_);
    generalLayout->addRow(tr("Default User-Agent"), defaultUserAgentCombo_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, enableStatisticsCheck_);
    generalLayout->addRow(tr("Statistics Refresh Rate"), statisticsFreshRateSpin_);
    generalLayout->addRow(tr("Tray Menu Server Limit"), trayMenuServersLimitSpin_);
    generalLayout->addRow(tr("Language"), languageCombo_);

    connect(enableStatisticsCheck_, &QCheckBox::toggled, this, [this](bool) {
        updateFieldState();
    });
    connect(sniffingEnabledCheck_, &QCheckBox::toggled, this, [this](bool) {
        updateFieldState();
    });
}

void GeneralSettingsPageWidget::updateFieldState()
{
    const bool statisticsEnabled = enableStatisticsCheck_ != nullptr && enableStatisticsCheck_->isChecked();
    if (statisticsFreshRateSpin_ != nullptr) {
        statisticsFreshRateSpin_->setEnabled(statisticsEnabled);
    }

    const bool sniffingEnabled = sniffingEnabledCheck_ != nullptr && sniffingEnabledCheck_->isChecked();
    if (routeOnlyCheck_ != nullptr) {
        routeOnlyCheck_->setEnabled(sniffingEnabled);
    }
}
