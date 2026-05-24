#include "ui/dialogs/CoreSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include "runtime/ProtocolCoreCompat.h"
#include "ui/theme/AppTheme.h"

namespace {

QStringList singBoxMuxProtocolOptions()
{
    return {
        QStringLiteral("h2mux"),
        QStringLiteral("smux"),
        QStringLiteral("yamux"),
        QString()};
}

QStringList xrayUserAgentOptions()
{
    return {
        QString(),
        QStringLiteral("chrome"),
        QStringLiteral("firefox"),
        QStringLiteral("edge"),
        QStringLiteral("golang")};
}

QList<CoreType> settingsCoreDisplayOrder(const QList<CoreType>& cores)
{
    QList<CoreType> ordered;
    for (const CoreType core : orderedCoreTypes()) {
        if (cores.contains(core)) {
            ordered.append(core);
        }
    }
    for (const CoreType core : cores) {
        if (!ordered.contains(core)) {
            ordered.append(core);
        }
    }
    return ordered;
}

}

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

void CoreSettingsPageWidget::setupUi()
{
    auto* coreLayout = new QVBoxLayout(this);

    enableCacheFile4SboxCheck_ = new QCheckBox(tr("Enable sing-box cache file"), this);
    enableCacheFile4SboxCheck_->setObjectName(QStringLiteral("settingsEnableCacheFile4SboxCheck"));

    mux4SboxProtocolCombo_ = new QComboBox(this);
    mux4SboxProtocolCombo_->setObjectName(QStringLiteral("settingsMux4SboxProtocolCombo"));
    mux4SboxProtocolCombo_->addItems(singBoxMuxProtocolOptions());

    xrayEnableFragmentCheck_ = new QCheckBox(tr("Enable fragmentation for TLS outbounds"), this);
    xrayEnableFragmentCheck_->setObjectName(QStringLiteral("settingsEnableFragmentCheck"));

    xrayDefaultUserAgentCombo_ = new QComboBox(this);
    xrayDefaultUserAgentCombo_->setObjectName(QStringLiteral("settingsDefaultUserAgentCombo"));
    xrayDefaultUserAgentCombo_->setEditable(true);
    xrayDefaultUserAgentCombo_->addItems(xrayUserAgentOptions());

    tunEnableLegacyProtectCheck_ =
        new QCheckBox(tr("Enable legacy protection (pre-socks relay for Xray)"), this);
    tunEnableLegacyProtectCheck_->setObjectName(QStringLiteral("settingsTunEnableLegacyProtectCheck"));

    AppTheme::applyCompactFont({
        enableCacheFile4SboxCheck_,
        mux4SboxProtocolCombo_,
        xrayEnableFragmentCheck_,
        xrayDefaultUserAgentCombo_,
        tunEnableLegacyProtectCheck_});

    coreProtocolEntries_.clear();
    for (const ConfigType configType : configurableCoreProtocols()) {
        coreProtocolEntries_.append(CoreProtocolEntry{configTypeDisplayName(configType), configType});
    }
    const QList<CoreType> allCores = orderedCoreTypes();
    const QFontMetrics coreLabelMetrics(font());
    int protocolLabelWidth = 0;
    for (const CoreProtocolEntry& entry : coreProtocolEntries_) {
        protocolLabelWidth = qMax(protocolLabelWidth, coreLabelMetrics.horizontalAdvance(entry.name));
    }
    const int coreStatusInfoWidth = coreLabelMetrics.horizontalAdvance(tr("Downloading...")) + 20;

    auto* coreTypeTitle = new QLabel(tr("By Protocol"), this);
    coreTypeTitle->setObjectName(QStringLiteral("coreProtocolSectionTitle"));
    AppTheme::applyCompactFont(coreTypeTitle);
    QFont coreTypeTitleFont = coreTypeTitle->font();
    coreTypeTitleFont.setBold(true);
    coreTypeTitle->setFont(coreTypeTitleFont);
    coreLayout->addWidget(coreTypeTitle);

    auto* coreTypeWidget = new QWidget(this);
    auto* coreTypeGrid = new QGridLayout(coreTypeWidget);
    coreTypeGrid->setContentsMargins(0, 0, 0, 0);
    coreTypeGrid->setHorizontalSpacing(10);
    coreTypeGrid->setVerticalSpacing(6);
    coreTypeGrid->setColumnStretch(1, 1);
    coreTypeGrid->setColumnMinimumWidth(2, 14);
    coreTypeGrid->setColumnStretch(4, 1);

    for (int i = 0; i < coreProtocolEntries_.size(); ++i) {
        const CoreProtocolEntry& entry = coreProtocolEntries_.at(i);
        const QList<CoreType> cores = settingsCoreDisplayOrder(supportedCoreTypes(entry.configType));
        const int row = i / 2;
        const int column = (i % 2) == 0 ? 0 : 3;
        auto* protocolLabel = new QLabel(entry.name, coreTypeWidget);
        protocolLabel->setMinimumWidth(protocolLabelWidth);
        AppTheme::applyCompactFont(protocolLabel);
        if (cores.size() > 1) {
            auto* coreCombo = new QComboBox(coreTypeWidget);
            for (const CoreType core : cores) {
                coreCombo->addItem(coreTypeDisplayName(core), static_cast<int>(core));
            }
            coreCombo->setObjectName(QStringLiteral("coreTypeCombo_%1").arg(static_cast<int>(entry.configType)));
            AppTheme::applyCompactFont(coreCombo);
            coreTypeGrid->addWidget(protocolLabel, row, column);
            coreTypeGrid->addWidget(coreCombo, row, column + 1);
            coreTypeCombos_.append(coreCombo);
        } else if (cores.size() == 1) {
            auto* fixedLabel = new QLabel(coreTypeDisplayName(cores.first()), coreTypeWidget);
            fixedLabel->setObjectName(QStringLiteral("coreTypeCombo_%1").arg(static_cast<int>(entry.configType)));
            AppTheme::applyCompactFont(fixedLabel);
            coreTypeGrid->addWidget(protocolLabel, row, column);
            coreTypeGrid->addWidget(fixedLabel, row, column + 1);
            coreTypeCombos_.append(nullptr);
        }
    }

    auto* coreStatusTitle = new QLabel(tr("By Core"), this);
    coreStatusTitle->setObjectName(QStringLiteral("coreStatusSectionTitle"));
    AppTheme::applyCompactFont(coreStatusTitle);
    QFont coreStatusTitleFont = coreStatusTitle->font();
    coreStatusTitleFont.setBold(true);
    coreStatusTitle->setFont(coreStatusTitleFont);

    coreDetailTabs_ = new QTabWidget(this);
    coreDetailTabs_->setObjectName(QStringLiteral("coreDetailTabs"));
    AppTheme::applyCompactFont(coreDetailTabs_);
    coreDetailTabs_->setDocumentMode(true);
    coreDetailTabs_->tabBar()->setDrawBase(false);
    coreDetailTabs_->tabBar()->setExpanding(false);
    AppTheme::applyCompactFont(coreDetailTabs_->tabBar());

    for (const CoreType core : allCores) {
        const int coreKey = static_cast<int>(core);
        auto& row = coreStatusRows_[coreKey];

        row.page = new QWidget(coreDetailTabs_);
        row.page->setProperty("coreDetailPage", true);
        auto* pageLayout = new QVBoxLayout(row.page);
        pageLayout->setContentsMargins(9, 9, 9, 9);
        pageLayout->setSpacing(10);

        auto* statusRow = new QWidget(row.page);
        auto* statusLayout = new QHBoxLayout(statusRow);
        statusLayout->setContentsMargins(0, 0, 0, 0);
        statusLayout->setSpacing(12);

        auto* coreNameLabel = new QLabel(coreTypeDisplayName(core), row.page);
        coreNameLabel->setObjectName(QStringLiteral("coreNameLabel_%1").arg(coreKey));
        coreNameLabel->setMinimumWidth(protocolLabelWidth);
        row.versionLabel = new QLabel(row.page);
        row.versionLabel->setObjectName(QStringLiteral("coreVersionLabel_%1").arg(coreKey));
        row.statusLabel = new QLabel(row.page);
        row.statusLabel->setObjectName(QStringLiteral("coreStatusLabel_%1").arg(coreKey));
        row.downloadButton = new QPushButton(tr("Install"), row.page);
        row.downloadButton->setObjectName(QStringLiteral("coreDownloadButton_%1").arg(coreKey));
        AppTheme::applyCompactFont({coreNameLabel, row.versionLabel, row.statusLabel, row.downloadButton});

        row.setAllButton = new QPushButton(tr("Set All"), row.page);
        row.setAllButton->setToolTip(tr("Set all protocols to %1").arg(coreTypeDisplayName(core)));
        AppTheme::applyCompactFont(row.setAllButton);
        row.downloadButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        row.setAllButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(row.setAllButton, &QPushButton::clicked, this, [this, core]() {
            for (int i = 0; i < coreTypeCombos_.size() && i < coreProtocolEntries_.size(); ++i) {
                auto* combo = coreTypeCombos_.at(i);
                if (combo == nullptr) {
                    continue;
                }
                if (protocolSupportsCore(coreProtocolEntries_.at(i).configType, core)) {
                    const int idx = combo->findData(static_cast<int>(core));
                    if (idx >= 0) {
                        combo->setCurrentIndex(idx);
                    }
                }
            }
        });

        auto* infoWidget = new QWidget(row.page);
        infoWidget->setFixedWidth(coreStatusInfoWidth);
        auto* infoLayout = new QVBoxLayout(infoWidget);
        infoLayout->setContentsMargins(0, 0, 0, 0);
        infoLayout->setSpacing(2);
        row.versionLabel->setFixedWidth(coreStatusInfoWidth);
        row.statusLabel->setFixedWidth(coreStatusInfoWidth);
        infoLayout->addWidget(row.versionLabel);
        infoLayout->addWidget(row.statusLabel);

        statusLayout->addWidget(coreNameLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
        statusLayout->addWidget(infoWidget, 0, Qt::AlignLeft | Qt::AlignVCenter);
        statusLayout->addWidget(row.downloadButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
        statusLayout->addWidget(row.setAllButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
        statusLayout->addStretch(1);
        pageLayout->addWidget(statusRow);

        auto* optionsWidget = new QWidget(row.page);
        row.optionsLayout = new QVBoxLayout(optionsWidget);
        row.optionsLayout->setContentsMargins(0, 0, 0, 0);
        row.optionsLayout->setSpacing(8);

        switch (core) {
        case CoreType::SingBox: {
            row.optionsLayout->addWidget(enableCacheFile4SboxCheck_);
            auto* muxRow = new QWidget(optionsWidget);
            auto* muxLayout = new QHBoxLayout(muxRow);
            muxLayout->setContentsMargins(0, 0, 0, 0);
            muxLayout->setSpacing(8);
            auto* muxLabel = new QLabel(tr("Mux Protocol"), muxRow);
            muxLayout->addWidget(muxLabel);
            muxLayout->addWidget(mux4SboxProtocolCombo_, 1);
            row.optionsLayout->addWidget(muxRow);
            break;
        }
        case CoreType::Xray:
            row.optionsLayout->addWidget(xrayEnableFragmentCheck_);
            {
                auto* userAgentRow = new QWidget(optionsWidget);
                auto* userAgentLayout = new QHBoxLayout(userAgentRow);
                userAgentLayout->setContentsMargins(0, 0, 0, 0);
                userAgentLayout->setSpacing(8);
                auto* userAgentLabel = new QLabel(tr("Default User-Agent"), userAgentRow);
                userAgentLayout->addWidget(userAgentLabel);
                userAgentLayout->addWidget(xrayDefaultUserAgentCombo_, 1);
                row.optionsLayout->addWidget(userAgentRow);
            }
            row.optionsLayout->addWidget(tunEnableLegacyProtectCheck_);
            break;
        default:
            break;
        }

        pageLayout->addWidget(optionsWidget);
        pageLayout->addStretch(1);
        coreDetailTabs_->addTab(row.page, coreTypeDisplayName(core));

        connect(row.downloadButton, &QPushButton::clicked, this, [this, core]() {
            if (coreDownloadHandler_) {
                coreDownloadHandler_(core);
            }
        });
    }

    coreLayout->addWidget(coreTypeWidget);
    coreLayout->addSpacing(8);
    coreLayout->addWidget(coreStatusTitle);
    coreLayout->addWidget(coreDetailTabs_, 1);
    coreLayout->addStretch();
}

void CoreSettingsPageWidget::reloadCoreTypeTable()
{
    for (int i = 0; i < coreTypeCombos_.size() && i < coreProtocolEntries_.size(); ++i) {
        auto* combo = coreTypeCombos_.at(i);
        if (combo == nullptr) {
            continue;
        }

        const ConfigType configType = coreProtocolEntries_.at(i).configType;
        int coreType = static_cast<int>(defaultCoreTypeForProtocol(configType));

        for (const CoreTypeItem& item : coreTypeItems_) {
            if (item.configType == static_cast<int>(configType)) {
                coreType = item.coreType;
                break;
            }
        }

        const int dataIndex = combo->findData(coreType);
        if (dataIndex >= 0) {
            combo->setCurrentIndex(dataIndex);
        }
    }
    refreshCoreStatusPresentation();
}

void CoreSettingsPageWidget::refreshCoreStatusPresentation()
{
    const bool updateRunning = updatingCoreType_ != CoreType::Unknown;

    for (auto it = coreStatusRows_.begin(); it != coreStatusRows_.end(); ++it) {
        const int key = it.key();
        const bool isActive = static_cast<CoreType>(key) == updatingCoreType_;
        const CoreType coreType = static_cast<CoreType>(key);
        auto& row = it.value();

        if (row.versionLabel != nullptr) {
            if (isActive) {
                row.versionLabel->setText(tr("Downloading..."));
            } else if (!row.versionText.isEmpty()) {
                row.versionLabel->setText(row.versionText);
            } else if (existingCoreTypes_.contains(coreType)) {
                row.versionLabel->setText(tr("Installed"));
            } else {
                row.versionLabel->setText(tr("Not found"));
            }
        }
        if (row.statusLabel != nullptr) {
            row.statusLabel->setText(coreStatusTextForProgress(row.progressText));
            row.statusLabel->setVisible(!isActive && !row.statusLabel->text().isEmpty());
        }
        if (row.downloadButton != nullptr) {
            const bool installed = existingCoreTypes_.contains(coreType);
            row.downloadButton->setText(installed ? tr("Update") : tr("Install"));
            row.downloadButton->setVisible(!isActive);
            row.downloadButton->setEnabled(!updateRunning);
        }
        if (row.setAllButton != nullptr) {
            row.setAllButton->setVisible(!isActive && existingCoreTypes_.contains(coreType));
        }
    }
}

QString CoreSettingsPageWidget::coreStatusTextForProgress(const QString& message) const
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
