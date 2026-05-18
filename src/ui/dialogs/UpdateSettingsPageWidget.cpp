#include "ui/dialogs/UpdateSettingsPageWidget.h"

#include <QCheckBox>
#include <QFormLayout>

#include "ui/theme/AppTheme.h"

UpdateSettingsPageWidget::UpdateSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void UpdateSettingsPageWidget::setConfig(const Config& config)
{
    checkPreReleaseUpdateCheck_->setChecked(config.checkPreReleaseUpdate);
    ignoreGeoUpdateCoreCheck_->setChecked(config.ignoreGeoUpdateCore);
}

void UpdateSettingsPageWidget::applyToConfig(Config& config) const
{
    config.checkPreReleaseUpdate = checkPreReleaseUpdateCheck_->isChecked();
    config.ignoreGeoUpdateCore = ignoreGeoUpdateCoreCheck_->isChecked();
}

void UpdateSettingsPageWidget::setupUi()
{
    auto* updateLayout = new QFormLayout(this);

    checkPreReleaseUpdateCheck_ = new QCheckBox(tr("Include pre-release builds"), this);
    checkPreReleaseUpdateCheck_->setObjectName(QStringLiteral("settingsCheckPreReleaseUpdateCheck"));
    ignoreGeoUpdateCoreCheck_ =
        new QCheckBox(tr("Do not overwrite geo files during core updates"), this);
    ignoreGeoUpdateCoreCheck_->setObjectName(QStringLiteral("settingsIgnoreGeoUpdateCoreCheck"));

    AppTheme::applyCompactFont({
        checkPreReleaseUpdateCheck_,
        ignoreGeoUpdateCoreCheck_});

    updateLayout->setWidget(
        updateLayout->rowCount(),
        QFormLayout::SpanningRole,
        checkPreReleaseUpdateCheck_);
    updateLayout->setWidget(
        updateLayout->rowCount(),
        QFormLayout::SpanningRole,
        ignoreGeoUpdateCoreCheck_);
}
