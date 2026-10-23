#include "ui/dialogs/GeneralSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>

#include "common/LocalPortOption.h"
#include "ui/theme/AppTheme.h"

namespace {

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
    showMainOnStartupCheck_->setChecked(config.ui().showMainOnStartup);
    autoRunCheck_->setChecked(config.ui().autoRunEnabled);
    allowLanConnectionCheck_->setChecked(config.allowLanConnection);
    sniffingEnabledCheck_->setChecked(config.sniffingEnabled);
    routeOnlyCheck_->setChecked(config.routeOnly);
    localPortSpin_->setValue(effectiveLocalPort(config.localPort));
    defaultAllowInsecureCheck_->setChecked(config.dns().defaultAllowInsecure);
    defaultFingerprintCombo_->setCurrentText(config.dns().defaultFingerprint);
    speedPingTestUrlEdit_->setText(config.defaults().speedPingTestUrl);
    skipOutboundCheck_->setChecked(config.skipOutboundCheck);

    const QString normalizedLanguage = config.ui().languageCode.trimmed();
    const int languageIndex = languageCombo_->findData(normalizedLanguage);
    languageCombo_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);

    const QString themeName = AppTheme::isSystemThemeName(config.ui().themeName)
        ? AppTheme::systemThemeName()
        : AppTheme::normalizeThemeName(config.ui().themeName);
    const int themeIndex = themeCombo_->findData(themeName);
    themeCombo_->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);

    updateFieldState();
}

void GeneralSettingsPageWidget::applyToConfig(Config& config) const
{
    config.ui().showMainOnStartup = showMainOnStartupCheck_->isChecked();
    config.ui().autoRunEnabled = autoRunCheck_->isChecked();
    config.allowLanConnection = allowLanConnectionCheck_->isChecked();
    config.sniffingEnabled = sniffingEnabledCheck_->isChecked();
    config.routeOnly = routeOnlyCheck_->isChecked();
    config.localPort = localPortSpin_->value();
    config.dns().defaultAllowInsecure = defaultAllowInsecureCheck_->isChecked();
    config.dns().defaultFingerprint = defaultFingerprintCombo_->currentText().trimmed();
    config.defaults().speedPingTestUrl = speedPingTestUrlEdit_->text().trimmed();
    config.skipOutboundCheck = skipOutboundCheck_->isChecked();
    config.ui().languageCode = languageCombo_->currentData().toString().trimmed();
    const QString selectedTheme = themeCombo_->currentData().toString();
    config.ui().themeName = AppTheme::isSystemThemeName(selectedTheme)
        ? AppTheme::systemThemeName()
        : AppTheme::normalizeThemeName(selectedTheme);
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

    skipOutboundCheck_ = new QCheckBox(tr("Skip the outbound check on startup"), this);
    skipOutboundCheck_->setObjectName(QStringLiteral("settingsSkipOutboundCheck"));

    localPortSpin_ = new QSpinBox(this);
    localPortSpin_->setObjectName(QStringLiteral("settingsLocalPortSpin"));
    localPortSpin_->setRange(1, 65535);

    defaultAllowInsecureCheck_ = new QCheckBox(tr("Allow insecure TLS by default"), this);
    defaultAllowInsecureCheck_->setObjectName(QStringLiteral("settingsDefaultAllowInsecureCheck"));
    defaultAllowInsecureCheck_->setToolTip(tr(
        "Used only when the node does not specify certificate verification. Newer Xray versions may require a trusted certificate or SHA-256 pin."));

    defaultFingerprintCombo_ = new QComboBox(this);
    defaultFingerprintCombo_->setObjectName(QStringLiteral("settingsDefaultFingerprintCombo"));
    defaultFingerprintCombo_->setEditable(true);
    defaultFingerprintCombo_->addItems(defaultFingerprintOptions());

    speedPingTestUrlEdit_ = new QLineEdit(this);
    speedPingTestUrlEdit_->setObjectName(QStringLiteral("settingsSpeedPingTestUrlEdit"));

    languageCombo_ = new QComboBox(this);
    languageCombo_->setObjectName(QStringLiteral("settingsLanguageCombo"));
    languageCombo_->addItem(tr("System Default"), QString());
    languageCombo_->addItem(tr("English"), QStringLiteral("en"));
    languageCombo_->addItem(tr("Chinese (Simplified)"), QStringLiteral("zh_CN"));

    themeCombo_ = new QComboBox(this);
    themeCombo_->setObjectName(QStringLiteral("settingsThemeCombo"));
    themeCombo_->addItem(tr("Follow system"), AppTheme::systemThemeName());
    themeCombo_->addItem(tr("Light"), AppTheme::lightThemeName());
    themeCombo_->addItem(tr("Dark"), AppTheme::darkThemeName());

    AppTheme::applyCompactFont({
        showMainOnStartupCheck_,
        autoRunCheck_,
        allowLanConnectionCheck_,
        sniffingEnabledCheck_,
        routeOnlyCheck_,
        skipOutboundCheck_,
        localPortSpin_,
        defaultAllowInsecureCheck_,
        defaultFingerprintCombo_,
        speedPingTestUrlEdit_,
        languageCombo_,
        themeCombo_});

    generalLayout->addRow(tr("Local Port"), localPortSpin_);
    generalLayout->addRow(tr("Language"), languageCombo_);
    generalLayout->addRow(tr("Theme"), themeCombo_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, showMainOnStartupCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, autoRunCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, allowLanConnectionCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, sniffingEnabledCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, routeOnlyCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, defaultAllowInsecureCheck_);
    generalLayout->setWidget(generalLayout->rowCount(), QFormLayout::SpanningRole, skipOutboundCheck_);
    generalLayout->addRow(tr("Default Fingerprint"), defaultFingerprintCombo_);
    generalLayout->addRow(tr("Speed test URL"), speedPingTestUrlEdit_);

    connect(sniffingEnabledCheck_, &QCheckBox::toggled, this, [this](bool) {
        updateFieldState();
    });
}

void GeneralSettingsPageWidget::updateFieldState()
{
    const bool sniffingEnabled = sniffingEnabledCheck_ != nullptr && sniffingEnabledCheck_->isChecked();
    if (routeOnlyCheck_ != nullptr) {
        routeOnlyCheck_->setEnabled(sniffingEnabled);
    }
}
