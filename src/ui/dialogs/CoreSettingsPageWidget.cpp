#include "ui/dialogs/CoreSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

#include "runtime/ProtocolCoreCompat.h"

CoreSettingsPageWidget::CoreSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void CoreSettingsPageWidget::setConfig(const Config& config)
{
    config_ = config;
    enableCacheFile4SboxCheck_->setChecked(config.dns().enableCacheFile4Sbox);
    const int muxProtocolIndex = mux4SboxProtocolCombo_->findText(config.mux4SboxProtocol);
    mux4SboxProtocolCombo_->setCurrentIndex(muxProtocolIndex >= 0 ? muxProtocolIndex : 0);
    xrayEnableFragmentCheck_->setChecked(config.dns().enableFragment);
    xrayDefaultUserAgentCombo_->setCurrentText(config.dns().defaultUserAgent);
    tunEnableLegacyProtectCheck_->setChecked(config.tun().tunModeItem.enableLegacyProtect);
    coreTypeItems_ = config.policy().coreTypeItems;
    reloadCoreTypeTable();
}

void CoreSettingsPageWidget::setExistingCoreTypes(const QList<CoreType>& coreTypes)
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

QList<CoreTypeItem> CoreSettingsPageWidget::collectCoreTypeItems() const
{
    QList<CoreTypeItem> items;

    for (int i = 0; i < coreTypeCombos_.size() && i < coreProtocolEntries_.size(); ++i) {
        const ConfigType configType = coreProtocolEntries_.at(i).configType;
        auto* combo = coreTypeCombos_.at(i);

        CoreTypeItem item;
        item.configType = static_cast<int>(configType);
        if (combo != nullptr) {
            bool ok = false;
            const int coreTypeValue = combo->currentData().toInt(&ok);
            item.coreType = ok
                ? coreTypeValue
                : static_cast<int>(defaultCoreTypeForProtocol(configType));
        } else {
            const QList<CoreType> cores = supportedCoreTypes(configType);
            item.coreType = cores.isEmpty()
                ? static_cast<int>(defaultCoreTypeForProtocol(configType))
                : static_cast<int>(cores.first());
        }
        items.append(item);
    }

    return items;
}

bool CoreSettingsPageWidget::enableCacheFile4Sbox() const
{
    return enableCacheFile4SboxCheck_ != nullptr && enableCacheFile4SboxCheck_->isChecked();
}

QString CoreSettingsPageWidget::mux4SboxProtocol() const
{
    return mux4SboxProtocolCombo_ == nullptr ? QString() : mux4SboxProtocolCombo_->currentText();
}

bool CoreSettingsPageWidget::xrayFragmentEnabled() const
{
    return xrayEnableFragmentCheck_ != nullptr && xrayEnableFragmentCheck_->isChecked();
}

QString CoreSettingsPageWidget::xrayDefaultUserAgent() const
{
    return xrayDefaultUserAgentCombo_ == nullptr ? QString() : xrayDefaultUserAgentCombo_->currentText().trimmed();
}

bool CoreSettingsPageWidget::legacyProtectEnabled() const
{
    return tunEnableLegacyProtectCheck_ != nullptr && tunEnableLegacyProtectCheck_->isChecked();
}

void CoreSettingsPageWidget::setCoreVersion(CoreType coreType, const QString& version)
{
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        coreStatusRows_[key].versionText = version.trimmed();
        refreshCoreStatusPresentation();
    }
}

void CoreSettingsPageWidget::beginCoreUpdate(CoreType coreType)
{
    updatingCoreType_ = coreType;
    setCoreUpdateProgress(coreType, tr("Checking..."));
}

void CoreSettingsPageWidget::setCoreUpdateProgress(CoreType coreType, const QString& message)
{
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        coreStatusRows_[key].progressText = message.trimmed();
        refreshCoreStatusPresentation();
    }
}

void CoreSettingsPageWidget::finishCoreUpdate(CoreType coreType, bool success, const QString& message)
{
    Q_UNUSED(message)
    updatingCoreType_ = CoreType::Unknown;
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        coreStatusRows_[key].progressText = success ? QString() : tr("Failed");
        refreshCoreStatusPresentation();
    }
}

void CoreSettingsPageWidget::setTunOptionsEnabled(bool enabled)
{
    tunEnableLegacyProtectCheck_->setEnabled(enabled);
}

void CoreSettingsPageWidget::setCoreDownloadHandler(const std::function<void(CoreType)>& handler)
{
    coreDownloadHandler_ = handler;
}

void CoreSettingsPageWidget::reloadCoreTypeTable()
{
    for (int i = 0; i < coreTypeCombos_.size() && i < coreProtocolEntries_.size(); ++i) {
        auto* combo = coreTypeCombos_.at(i);
        if (combo == nullptr) {
            continue;
        }

        const ConfigType configType = coreProtocolEntries_.at(i).configType;
        const int coreType = CoreSettingsPageSupport::configuredCoreTypeValue(
            coreTypeItems_,
            configType,
            static_cast<int>(defaultCoreTypeForProtocol(configType)));

        const int dataIndex = combo->findData(coreType);
        if (dataIndex >= 0) {
            combo->setCurrentIndex(dataIndex);
        }
    }
    refreshCoreStatusPresentation();
}

void CoreSettingsPageWidget::refreshCoreStatusPresentation()
{
    for (auto it = coreStatusRows_.begin(); it != coreStatusRows_.end(); ++it) {
        const int key = it.key();
        const CoreType coreType = static_cast<CoreType>(key);
        auto& row = it.value();
        const CoreSettingsPageSupport::CoreStatusPresentation presentation =
            CoreSettingsPageSupport::coreStatusPresentation(CoreSettingsPageSupport::CoreStatusInput{
                coreType,
                updatingCoreType_,
                existingCoreTypes_,
                row.versionText,
                row.progressText});

        if (row.versionLabel != nullptr) {
            row.versionLabel->setText(presentation.versionText);
        }
        if (row.statusLabel != nullptr) {
            row.statusLabel->setText(presentation.statusText);
            row.statusLabel->setVisible(presentation.statusVisible);
        }
        if (row.downloadButton != nullptr) {
            row.downloadButton->setText(presentation.downloadButtonText);
            row.downloadButton->setVisible(presentation.downloadButtonVisible);
            row.downloadButton->setEnabled(presentation.downloadButtonEnabled);
        }
        if (row.setAllButton != nullptr) {
            row.setAllButton->setVisible(presentation.setAllButtonVisible);
        }
    }
}
