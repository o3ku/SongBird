#include "ui/dialogs/CoreSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
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

int CoreSettingsPageWidget::coreOptionLabelWidthFor(const QStringList& labels) const
{
    const QFontMetrics metrics(font());
    int widest = 0;
    for (const QString& label : labels) {
        widest = qMax(widest, metrics.horizontalAdvance(label));
    }
    return widest;
}

void CoreSettingsPageWidget::alignCoreOptionLabel(QLabel* label) const
{
    if (label == nullptr || coreOptionLabelWidth_ <= 0) {
        return;
    }
    label->setMinimumWidth(coreOptionLabelWidth_);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void CoreSettingsPageWidget::setupUi()
{
    auto* coreLayout = new QVBoxLayout(this);

    // Every "label + input" row on this page shares one label column, the way QFormLayout aligns
    // the General, DNS and TUN pages. Each row here is its own QHBoxLayout, so nothing otherwise
    // ties them together: the width used to come from each label's own text, and the four rows
    // started their editors at three different offsets. Measuring before any label is pinned
    // keeps a translated string honest -- it simply picks a new width and every row follows.
    coreOptionLabelWidth_ = coreOptionLabelWidthFor({
        tr("Mux Protocol"),
        tr("Proxy Server DNS"),
        tr("Default User-Agent"),
        tr("Direct Expected IPs")});

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

    // Moved here from the DNS page: these are read by exactly one core each, so they
    // were shown as if they applied to every core while silently doing nothing for
    // the others. The sing-box ones are below, the Xray ones after them.
    proxyServerDnsEdit_ = new QLineEdit(this);
    proxyServerDnsEdit_->setObjectName(QStringLiteral("settingsProxyServerDnsEdit"));
    proxyServerDnsEdit_->setPlaceholderText(QStringLiteral("https://1.1.1.1/dns-query"));
    proxyServerDnsEdit_->setToolTip(tr("Resolves the proxy server's own hostname. Empty uses the direct DNS."));
    blockBindingQueryCheck_ = new QCheckBox(tr("Block binding query"), this);
    blockBindingQueryCheck_->setObjectName(QStringLiteral("settingsBlockBindingQueryCheck"));
    globalFakeIpCheck_ = new QCheckBox(tr("Global FakeIP"), this);
    globalFakeIpCheck_->setObjectName(QStringLiteral("dnsGlobalFakeIpCheck"));
    globalFakeIpCheck_->setToolTip(tr("Resolve every domain through FakeIP, not only proxied ones. Requires FakeIP on the DNS page."));

    serveStaleCheck_ = new QCheckBox(tr("Serve stale DNS cache"), this);
    serveStaleCheck_->setObjectName(QStringLiteral("settingsServeStaleCheck"));
    parallelQueryCheck_ = new QCheckBox(tr("Parallel DNS query"), this);
    parallelQueryCheck_->setObjectName(QStringLiteral("settingsParallelQueryCheck"));
    directExpectedIpsEdit_ = new QLineEdit(this);
    directExpectedIpsEdit_->setObjectName(QStringLiteral("settingsDirectExpectedIpsEdit"));
    directExpectedIpsEdit_->setPlaceholderText(tr("Comma-separated IPs"));

    AppTheme::applyCompactFont({
        enableCacheFile4SboxCheck_,
        mux4SboxProtocolCombo_,
        xrayEnableFragmentCheck_,
        xrayDefaultUserAgentCombo_,
        tunEnableLegacyProtectCheck_,
        proxyServerDnsEdit_,
        blockBindingQueryCheck_,
        globalFakeIpCheck_,
        serveStaleCheck_,
        parallelQueryCheck_,
        directExpectedIpsEdit_});

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

    // The per-protocol table is shown directly: it used to sit behind an
    // "Advanced" toggle, but collapsing it hid the section title and the mapping
    // rows behind a click for no gain.
    coreProtocolSection_ = new QWidget(this);
    auto* coreProtocolSectionLayout = new QVBoxLayout(coreProtocolSection_);
    coreProtocolSectionLayout->setContentsMargins(0, 0, 0, 0);
    coreProtocolSectionLayout->setSpacing(4);
    coreProtocolSectionLayout->addWidget(coreTypeTitle);

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
        if (!cores.isEmpty()) {
            auto* coreCombo = new QComboBox(coreTypeWidget);
            for (const CoreType core : cores) {
                coreCombo->addItem(coreTypeDisplayName(core), static_cast<int>(core));
            }
            coreCombo->setObjectName(QStringLiteral("coreTypeCombo_%1").arg(static_cast<int>(entry.configType)));
            AppTheme::applyCompactFont(coreCombo);
            coreTypeGrid->addWidget(protocolLabel, row, column);
            coreTypeGrid->addWidget(coreCombo, row, column + 1);
            coreTypeCombos_.append(coreCombo);
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
        row.downloadProgress = new QProgressBar(row.page);
        row.downloadProgress->setObjectName(QStringLiteral("coreDownloadProgress_%1").arg(coreKey));
        row.downloadProgress->setRange(0, 100);
        row.downloadProgress->setTextVisible(true);
        row.downloadProgress->setFixedWidth(coreStatusInfoWidth);
        row.downloadProgress->setVisible(false);
        row.downloadButton = new QPushButton(tr("Install"), row.page);
        row.downloadButton->setObjectName(QStringLiteral("coreDownloadButton_%1").arg(coreKey));
        row.cancelButton = new QPushButton(tr("Cancel"), row.page);
        row.cancelButton->setObjectName(QStringLiteral("coreCancelButton_%1").arg(coreKey));
        row.cancelButton->setToolTip(tr("Stop this update. The partial download is discarded."));
        row.cancelButton->setVisible(false);
        AppTheme::applyCompactFont({coreNameLabel, row.versionLabel, row.statusLabel, row.downloadProgress, row.downloadButton, row.cancelButton});
        connect(row.cancelButton, &QPushButton::clicked, this, [this, core]() {
            if (coreUpdateCancelHandler_) {
                coreUpdateCancelHandler_(core);
            }
        });

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
        infoLayout->addWidget(row.downloadProgress);

        statusLayout->addWidget(coreNameLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
        statusLayout->addWidget(infoWidget, 0, Qt::AlignLeft | Qt::AlignVCenter);
        statusLayout->addWidget(row.downloadButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
        statusLayout->addWidget(row.cancelButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
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
            alignCoreOptionLabel(muxLabel);
            muxLayout->addWidget(muxLabel);
            muxLayout->addWidget(mux4SboxProtocolCombo_, 1);
            row.optionsLayout->addWidget(muxRow);

            // sing-box-only DNS options, moved off the shared DNS page.
            auto* proxyServerDnsRow = new QWidget(optionsWidget);
            auto* proxyServerDnsLayout = new QHBoxLayout(proxyServerDnsRow);
            proxyServerDnsLayout->setContentsMargins(0, 0, 0, 0);
            proxyServerDnsLayout->setSpacing(8);
            auto* proxyServerDnsLabel = new QLabel(tr("Proxy Server DNS"), proxyServerDnsRow);
            alignCoreOptionLabel(proxyServerDnsLabel);
            proxyServerDnsLayout->addWidget(proxyServerDnsLabel);
            proxyServerDnsLayout->addWidget(proxyServerDnsEdit_, 1);
            row.optionsLayout->addWidget(proxyServerDnsRow);
            row.optionsLayout->addWidget(blockBindingQueryCheck_);
            row.optionsLayout->addWidget(globalFakeIpCheck_);
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
                alignCoreOptionLabel(userAgentLabel);
                userAgentLayout->addWidget(userAgentLabel);
                userAgentLayout->addWidget(xrayDefaultUserAgentCombo_, 1);
                row.optionsLayout->addWidget(userAgentRow);
            }

            // Xray-only DNS options, moved off the shared DNS page.
            row.optionsLayout->addWidget(serveStaleCheck_);
            row.optionsLayout->addWidget(parallelQueryCheck_);
            {
                auto* expectedIpsRow = new QWidget(optionsWidget);
                auto* expectedIpsLayout = new QHBoxLayout(expectedIpsRow);
                expectedIpsLayout->setContentsMargins(0, 0, 0, 0);
                expectedIpsLayout->setSpacing(8);
                auto* expectedIpsLabel = new QLabel(tr("Direct Expected IPs"), expectedIpsRow);
                alignCoreOptionLabel(expectedIpsLabel);
                expectedIpsLayout->addWidget(expectedIpsLabel);
                expectedIpsLayout->addWidget(directExpectedIpsEdit_, 1);
                row.optionsLayout->addWidget(expectedIpsRow);
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

    coreProtocolSectionLayout->addWidget(coreTypeWidget);
    coreLayout->addWidget(coreStatusTitle);
    coreLayout->addWidget(coreDetailTabs_, 1);
    coreLayout->addSpacing(8);
    coreLayout->addWidget(coreProtocolSection_);
    coreLayout->addStretch();
}
