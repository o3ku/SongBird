#include "ui/dialogs/GeneralSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>

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
    localPortSpin_->setValue(config.localPort > 0 ? config.localPort : 10808);
    defaultAllowInsecureCheck_->setChecked(config.dns().defaultAllowInsecure);
    defaultFingerprintCombo_->setCurrentText(config.dns().defaultFingerprint);

    const QString normalizedLanguage = config.ui().languageCode.trimmed();
    const int languageIndex = languageCombo_->findData(normalizedLanguage);
    languageCombo_->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);

    const QString themeName = AppTheme::normalizeThemeName(config.ui().themeName);
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
    config.ui().languageCode = languageCombo_->currentData().toString().trimmed();
    config.ui().themeName = AppTheme::normalizeThemeName(themeCombo_->currentData().toString());
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

    defaultAllowInsecureCheck_ = new QCheckBox(tr("Allow insecure TLS by default"), this);
    defaultAllowInsecureCheck_->setObjectName(QStringLiteral("settingsDefaultAllowInsecureCheck"));

    defaultFingerprintCombo_ = new QComboBox(this);
    defaultFingerprintCombo_->setObjectName(QStringLiteral("settingsDefaultFingerprintCombo"));
    defaultFingerprintCombo_->setEditable(true);
    defaultFingerprintCombo_->addItems(defaultFingerprintOptions());

    languageCombo_ = new QComboBox(this);
    languageCombo_->setObjectName(QStringLiteral("settingsLanguageCombo"));
    languageCombo_->addItem(tr("System Default"), QString());
    languageCombo_->addItem(tr("English"), QStringLiteral("en"));
    languageCombo_->addItem(tr("Chinese (Simplified)"), QStringLiteral("zh_CN"));

    themeCombo_ = new QComboBox(this);
    themeCombo_->setObjectName(QStringLiteral("settingsThemeCombo"));
    themeCombo_->addItem(AppTheme::lightThemeName(), AppTheme::lightThemeName());
    themeCombo_->addItem(AppTheme::darkThemeName(), AppTheme::darkThemeName());

    AppTheme::applyCompactFont({
        showMainOnStartupCheck_,
        autoRunCheck_,
        allowLanConnectionCheck_,
        sniffingEnabledCheck_,
        routeOnlyCheck_,
        localPortSpin_,
        defaultAllowInsecureCheck_,
        defaultFingerprintCombo_,
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
    generalLayout->addRow(tr("Default Fingerprint"), defaultFingerprintCombo_);

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
