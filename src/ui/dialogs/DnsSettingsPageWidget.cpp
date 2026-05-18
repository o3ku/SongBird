#include "ui/dialogs/DnsSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>

#include "ui/theme/AppTheme.h"

namespace {

QStringList domainStrategyOptions()
{
    return {
        QString(),
        QStringLiteral("AsIs"),
        QStringLiteral("IPIfNonMatch"),
        QStringLiteral("IPOnDemand"),
        QStringLiteral("PreferIPv4"),
        QStringLiteral("PreferIPv6"),
        QStringLiteral("PreferIPv4v6"),
        QStringLiteral("PreferIPv6v4"),
        QStringLiteral("OnlyIPv4"),
        QStringLiteral("OnlyIPv6")};
}

} // namespace

DnsSettingsPageWidget::DnsSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    updateFieldState();
}

void DnsSettingsPageWidget::setConfig(const Config& config)
{
    remoteDnsEdit_->setText(config.remoteDns);
    directDnsEdit_->setText(config.directDns);
    bootstrapDnsEdit_->setText(config.bootstrapDns);
    domainStrategyForFreedomCombo_->setCurrentText(config.domainStrategyForFreedom);
    domainStrategyForProxyCombo_->setCurrentText(config.domainStrategyForProxy);
    useSystemHostsCheck_->setChecked(config.useSystemHosts);
    addCommonHostsCheck_->setChecked(config.addCommonHosts);
    blockBindingQueryCheck_->setChecked(config.blockBindingQuery);
    fakeIpCheck_->setChecked(config.fakeIp);
    globalFakeIpCheck_->setChecked(config.globalFakeIp);
    serveStaleCheck_->setChecked(config.serveStale);
    parallelQueryCheck_->setChecked(config.parallelQuery);
    directExpectedIpsEdit_->setText(config.directExpectedIps);
    dnsHostsEdit_->setPlainText(config.dnsHosts);
    updateFieldState();
}

void DnsSettingsPageWidget::applyToConfig(Config& config) const
{
    config.remoteDns = remoteDnsEdit_->text().trimmed();
    config.directDns = directDnsEdit_->text().trimmed();
    config.bootstrapDns = bootstrapDnsEdit_->text().trimmed();
    config.domainStrategyForFreedom = domainStrategyForFreedomCombo_->currentText().trimmed();
    config.domainStrategyForProxy = domainStrategyForProxyCombo_->currentText().trimmed();
    config.useSystemHosts = useSystemHostsCheck_->isChecked();
    config.addCommonHosts = addCommonHostsCheck_->isChecked();
    config.blockBindingQuery = blockBindingQueryCheck_->isChecked();
    config.fakeIp = fakeIpCheck_->isChecked();
    config.globalFakeIp = globalFakeIpCheck_->isChecked();
    config.serveStale = serveStaleCheck_->isChecked();
    config.parallelQuery = parallelQueryCheck_->isChecked();
    config.directExpectedIps = directExpectedIpsEdit_->text().trimmed();
    config.dnsHosts = dnsHostsEdit_->toPlainText().trimmed();
}

void DnsSettingsPageWidget::setupUi()
{
    auto* dnsLayout = new QFormLayout(this);

    remoteDnsEdit_ = new QLineEdit(this);
    remoteDnsEdit_->setObjectName(QStringLiteral("settingsRemoteDnsEdit"));
    remoteDnsEdit_->setPlaceholderText(QStringLiteral("https://cloudflare-dns.com/dns-query"));

    directDnsEdit_ = new QLineEdit(this);
    directDnsEdit_->setObjectName(QStringLiteral("settingsDirectDnsEdit"));
    directDnsEdit_->setPlaceholderText(QStringLiteral("https://dns.alidns.com/dns-query"));

    bootstrapDnsEdit_ = new QLineEdit(this);
    bootstrapDnsEdit_->setObjectName(QStringLiteral("settingsBootstrapDnsEdit"));
    bootstrapDnsEdit_->setPlaceholderText(QStringLiteral("223.5.5.5"));

    domainStrategyForFreedomCombo_ = new QComboBox(this);
    domainStrategyForFreedomCombo_->setObjectName(QStringLiteral("settingsDomainStrategyForFreedomCombo"));
    domainStrategyForFreedomCombo_->setEditable(true);
    domainStrategyForFreedomCombo_->addItems(domainStrategyOptions());

    domainStrategyForProxyCombo_ = new QComboBox(this);
    domainStrategyForProxyCombo_->setObjectName(QStringLiteral("settingsDomainStrategyForProxyCombo"));
    domainStrategyForProxyCombo_->setEditable(true);
    domainStrategyForProxyCombo_->addItems(domainStrategyOptions());

    useSystemHostsCheck_ = new QCheckBox(tr("Use system hosts file"), this);
    useSystemHostsCheck_->setObjectName(QStringLiteral("settingsUseSystemHostsCheck"));
    addCommonHostsCheck_ = new QCheckBox(tr("Add common hosts (google, github, etc.)"), this);
    addCommonHostsCheck_->setObjectName(QStringLiteral("settingsAddCommonHostsCheck"));
    blockBindingQueryCheck_ = new QCheckBox(tr("Block binding query"), this);
    blockBindingQueryCheck_->setObjectName(QStringLiteral("settingsBlockBindingQueryCheck"));
    fakeIpCheck_ = new QCheckBox(tr("Enable FakeIP"), this);
    fakeIpCheck_->setObjectName(QStringLiteral("dnsFakeIpCheck"));
    globalFakeIpCheck_ = new QCheckBox(tr("Global FakeIP"), this);
    globalFakeIpCheck_->setObjectName(QStringLiteral("dnsGlobalFakeIpCheck"));
    serveStaleCheck_ = new QCheckBox(tr("Serve stale DNS cache"), this);
    serveStaleCheck_->setObjectName(QStringLiteral("settingsServeStaleCheck"));
    parallelQueryCheck_ = new QCheckBox(tr("Parallel DNS query"), this);
    parallelQueryCheck_->setObjectName(QStringLiteral("settingsParallelQueryCheck"));

    directExpectedIpsEdit_ = new QLineEdit(this);
    directExpectedIpsEdit_->setObjectName(QStringLiteral("settingsDirectExpectedIpsEdit"));
    directExpectedIpsEdit_->setPlaceholderText(QStringLiteral("Comma-separated IPs"));

    dnsHostsEdit_ = new QTextEdit(this);
    dnsHostsEdit_->setObjectName(QStringLiteral("settingsDnsHostsEdit"));
    dnsHostsEdit_->setTabChangesFocus(true);
    dnsHostsEdit_->setMaximumHeight(120);
    dnsHostsEdit_->setPlaceholderText(QStringLiteral("domain ip\nexample.com 1.2.3.4"));

    AppTheme::applyCompactFont({
        remoteDnsEdit_,
        directDnsEdit_,
        bootstrapDnsEdit_,
        domainStrategyForFreedomCombo_,
        domainStrategyForProxyCombo_,
        useSystemHostsCheck_,
        addCommonHostsCheck_,
        blockBindingQueryCheck_,
        fakeIpCheck_,
        globalFakeIpCheck_,
        serveStaleCheck_,
        parallelQueryCheck_,
        directExpectedIpsEdit_,
        dnsHostsEdit_});

    dnsLayout->addRow(tr("Remote DNS"), remoteDnsEdit_);
    dnsLayout->addRow(tr("Direct DNS"), directDnsEdit_);
    dnsLayout->addRow(tr("Bootstrap DNS"), bootstrapDnsEdit_);
    dnsLayout->addRow(tr("Domain Strategy (Freedom)"), domainStrategyForFreedomCombo_);
    dnsLayout->addRow(tr("Domain Strategy (Proxy)"), domainStrategyForProxyCombo_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, useSystemHostsCheck_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, addCommonHostsCheck_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, blockBindingQueryCheck_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, fakeIpCheck_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, globalFakeIpCheck_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, serveStaleCheck_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, parallelQueryCheck_);
    dnsLayout->addRow(tr("Direct Expected IPs"), directExpectedIpsEdit_);
    dnsLayout->addRow(tr("DNS Hosts"), dnsHostsEdit_);

    connect(fakeIpCheck_, &QCheckBox::toggled, this, [this](bool) {
        updateFieldState();
    });
}

void DnsSettingsPageWidget::updateFieldState()
{
    const bool fakeIpEnabled = fakeIpCheck_ != nullptr && fakeIpCheck_->isChecked();
    if (globalFakeIpCheck_ != nullptr) {
        globalFakeIpCheck_->setEnabled(fakeIpEnabled);
    }
}
