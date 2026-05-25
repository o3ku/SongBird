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

void applyBoldSectionTitle(QLabel* label)
{
    AppTheme::applyCompactFont(label);
    QFont titleFont = label->font();
    titleFont.setBold(true);
    label->setFont(titleFont);
}

} // namespace

void CoreSettingsPageWidget::setupUi()
{
    auto* coreLayout = new QVBoxLayout(this);

    enableCacheFile4SboxCheck_ = new QCheckBox(tr("Enable sing-box cache file"), this);
    enableCacheFile4SboxCheck_->setObjectName(QStringLiteral("settingsEnableCacheFile4SboxCheck"));

    mux4SboxProtocolCombo_ = new QComboBox(this);
    mux4SboxProtocolCombo_->setObjectName(QStringLiteral("settingsMux4SboxProtocolCombo"));
    mux4SboxProtocolCombo_->addItems(CoreSettingsPageSupport::singBoxMuxProtocolOptions());

    xrayEnableFragmentCheck_ = new QCheckBox(tr("Enable fragmentation for TLS outbounds"), this);
    xrayEnableFragmentCheck_->setObjectName(QStringLiteral("settingsEnableFragmentCheck"));

    xrayDefaultUserAgentCombo_ = new QComboBox(this);
    xrayDefaultUserAgentCombo_->setObjectName(QStringLiteral("settingsDefaultUserAgentCombo"));
    xrayDefaultUserAgentCombo_->setEditable(true);
    xrayDefaultUserAgentCombo_->addItems(CoreSettingsPageSupport::xrayUserAgentOptions());

    tunEnableLegacyProtectCheck_ =
        new QCheckBox(tr("Enable legacy protection (pre-socks relay for Xray)"), this);
    tunEnableLegacyProtectCheck_->setObjectName(QStringLiteral("settingsTunEnableLegacyProtectCheck"));

    AppTheme::applyCompactFont({
        enableCacheFile4SboxCheck_,
        mux4SboxProtocolCombo_,
        xrayEnableFragmentCheck_,
        xrayDefaultUserAgentCombo_,
        tunEnableLegacyProtectCheck_});

    coreProtocolEntries_ = CoreSettingsPageSupport::coreProtocolEntries();
    const QList<CoreType> allCores = orderedCoreTypes();
    const QFontMetrics coreLabelMetrics(font());
    int protocolLabelWidth = 0;
    for (const CoreSettingsPageSupport::CoreProtocolEntry& entry : coreProtocolEntries_) {
        protocolLabelWidth = qMax(protocolLabelWidth, coreLabelMetrics.horizontalAdvance(entry.name));
    }
    const int coreStatusInfoWidth = coreLabelMetrics.horizontalAdvance(tr("Downloading...")) + 20;

    auto* coreTypeTitle = new QLabel(tr("By Protocol"), this);
    coreTypeTitle->setObjectName(QStringLiteral("coreProtocolSectionTitle"));
    applyBoldSectionTitle(coreTypeTitle);
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
        const CoreSettingsPageSupport::CoreProtocolEntry& entry = coreProtocolEntries_.at(i);
        const QList<CoreType> cores =
            CoreSettingsPageSupport::settingsCoreDisplayOrder(supportedCoreTypes(entry.configType));
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
    applyBoldSectionTitle(coreStatusTitle);

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
            row.optionsLayout->addWidget(tunEnableLegacyProtectCheck_);
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
