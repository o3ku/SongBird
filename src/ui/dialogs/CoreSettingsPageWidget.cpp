#include "ui/dialogs/CoreSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>

#include "runtime/ProtocolCoreCompat.h"

CoreSettingsPageWidget::CoreSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void CoreSettingsPageWidget::setConfig(const Config& config)
{
    enableCacheFile4SboxCheck_->setChecked(config.dns().enableCacheFile4Sbox);
    const int muxProtocolIndex = mux4SboxProtocolCombo_->findText(config.mux4SboxProtocol);
    mux4SboxProtocolCombo_->setCurrentIndex(muxProtocolIndex >= 0 ? muxProtocolIndex : 0);
    xrayEnableFragmentCheck_->setChecked(config.dns().enableFragment);
    xrayDefaultUserAgentCombo_->setCurrentText(config.dns().defaultUserAgent);
    tunEnableLegacyProtectCheck_->setChecked(config.tun().tunModeItem.enableLegacyProtect);
    proxyServerDnsEdit_->setText(config.dns().proxyServerDns);
    blockBindingQueryCheck_->setChecked(config.dns().blockBindingQuery);
    globalFakeIpCheck_->setChecked(config.dns().globalFakeIp);
    serveStaleCheck_->setChecked(config.dns().serveStale);
    parallelQueryCheck_->setChecked(config.dns().parallelQuery);
    directExpectedIpsEdit_->setText(config.dns().directExpectedIps);
    setFakeIpEnabled(config.dns().fakeIp);
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

QString CoreSettingsPageWidget::proxyServerDns() const
{
    return proxyServerDnsEdit_ == nullptr ? QString() : proxyServerDnsEdit_->text().trimmed();
}

bool CoreSettingsPageWidget::blockBindingQuery() const
{
    return blockBindingQueryCheck_ != nullptr && blockBindingQueryCheck_->isChecked();
}

bool CoreSettingsPageWidget::globalFakeIp() const
{
    return globalFakeIpCheck_ != nullptr && globalFakeIpCheck_->isChecked();
}

bool CoreSettingsPageWidget::serveStale() const
{
    return serveStaleCheck_ != nullptr && serveStaleCheck_->isChecked();
}

bool CoreSettingsPageWidget::parallelQuery() const
{
    return parallelQueryCheck_ != nullptr && parallelQueryCheck_->isChecked();
}

QString CoreSettingsPageWidget::directExpectedIps() const
{
    return directExpectedIpsEdit_ == nullptr ? QString() : directExpectedIpsEdit_->text().trimmed();
}

void CoreSettingsPageWidget::setFakeIpEnabled(bool enabled)
{
    if (globalFakeIpCheck_ != nullptr) {
        // FakeIP itself stayed on the DNS page because two cores read it, but only
        // sing-box has a "global" variant, so the row follows that checkbox.
        globalFakeIpCheck_->setEnabled(enabled);
    }
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
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        // A fresh run always starts the bar over from the first byte.
        auto& row = coreStatusRows_[key];
        row.downloadActive = true;
        row.downloadReceivedBytes = 0;
        row.downloadTotalBytes = 0;
        row.lastAttemptFailed = false;
        row.failureReason.clear();
        row.cancelRequested = false;
    }
    setCoreUpdateProgress(coreType, tr("Checking..."));
}

void CoreSettingsPageWidget::markCoreUpdateCancelRequested(CoreType coreType)
{
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        coreStatusRows_[key].cancelRequested = true;
        refreshCoreStatusPresentation();
    }
}

void CoreSettingsPageWidget::setCoreUpdateProgress(CoreType coreType, const QString& message)
{
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        coreStatusRows_[key].progressText = message.trimmed();
        refreshCoreStatusPresentation();
    }
}

void CoreSettingsPageWidget::setCoreUpdateDownloadProgress(CoreType coreType, qint64 bytesReceived, qint64 bytesTotal)
{
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        auto& row = coreStatusRows_[key];
        row.downloadActive = true;
        row.downloadReceivedBytes = bytesReceived < 0 ? 0 : bytesReceived;
        row.downloadTotalBytes = bytesTotal < 0 ? 0 : bytesTotal;
        refreshCoreStatusPresentation();
    }
}

void CoreSettingsPageWidget::finishCoreUpdate(CoreType coreType, bool success, const QString& message)
{
    updatingCoreType_ = CoreType::Unknown;
    const int key = static_cast<int>(coreType);
    if (coreStatusRows_.contains(key)) {
        auto& row = coreStatusRows_[key];
        row.downloadActive = false;
        row.downloadReceivedBytes = 0;
        row.downloadTotalBytes = 0;
        row.cancelRequested = false;
        // The failure reason outlives the attempt: it becomes the retry button's
        // tooltip, which is where the user looks for it.
        row.lastAttemptFailed = !success;
        row.failureReason = success ? QString() : message.trimmed();
        row.progressText = success ? QString() : tr("Failed");
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

void CoreSettingsPageWidget::setCoreUpdateCancelHandler(const std::function<void(CoreType)>& handler)
{
    coreUpdateCancelHandler_ = handler;
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
                row.progressText,
                row.downloadReceivedBytes,
                row.downloadTotalBytes,
                row.downloadActive,
                row.lastAttemptFailed,
                row.failureReason,
                row.cancelRequested});

        if (row.versionLabel != nullptr) {
            row.versionLabel->setText(presentation.versionText);
            // While the bar is shown it stands in for the version line so the row does not grow
            // mid-download.
            row.versionLabel->setVisible(!presentation.downloadProgressVisible);
        }
        if (row.downloadProgress != nullptr) {
            row.downloadProgress->setVisible(presentation.downloadProgressVisible);
            row.downloadProgress->setToolTip(presentation.downloadProgressToolTip);
            if (presentation.downloadProgressVisible) {
                if (presentation.downloadIndeterminate) {
                    // Range 0/0 makes QProgressBar draw the busy indicator; the percent text is
                    // suppressed by Qt in that state.
                    row.downloadProgress->setRange(0, 0);
                } else {
                    row.downloadProgress->setRange(0, 100);
                    row.downloadProgress->setValue(presentation.downloadPercent);
                }
            } else {
                row.downloadProgress->setRange(0, 100);
                row.downloadProgress->setValue(0);
            }
        }
        if (row.statusLabel != nullptr) {
            row.statusLabel->setText(presentation.statusText);
            row.statusLabel->setVisible(presentation.statusVisible);
        }
        if (row.downloadButton != nullptr) {
            row.downloadButton->setText(presentation.downloadButtonText);
            row.downloadButton->setVisible(presentation.downloadButtonVisible);
            row.downloadButton->setEnabled(presentation.downloadButtonEnabled);
            row.downloadButton->setToolTip(presentation.downloadButtonToolTip);
        }
        if (row.cancelButton != nullptr) {
            row.cancelButton->setVisible(presentation.cancelButtonVisible);
            row.cancelButton->setEnabled(presentation.cancelButtonEnabled);
        }
        if (row.setAllButton != nullptr) {
            row.setAllButton->setVisible(presentation.setAllButtonVisible);
        }
    }
}
