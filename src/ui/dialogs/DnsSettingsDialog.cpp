#include "ui/dialogs/DnsSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QStringList domainStrategyOptions()
{
    return {
        QStringLiteral(""),
        QStringLiteral("AsIs"),
        QStringLiteral("UseIP"),
        QStringLiteral("UseIPv4"),
        QStringLiteral("UseIPv6"),
        QStringLiteral("PreferIPv4"),
        QStringLiteral("PreferIPv6")};
}

void selectCurrentText(QComboBox* combo, const QString& text)
{
    if (combo == nullptr) {
        return;
    }
    const int index = combo->findText(text, Qt::MatchFixedString);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else {
        combo->setCurrentIndex(0);
    }
}

} // namespace

DnsSettingsDialog::DnsSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    updateFieldState();
}

void DnsSettingsDialog::setConfig(const Config& config)
{
    config_ = config;

    remoteDnsEdit_->setText(config_.dns().remoteDns);
    directDnsEdit_->setText(config_.dns().directDns);
    bootstrapDnsEdit_->setText(config_.dns().bootstrapDns);
    selectCurrentText(domainStrategyProxyCombo_, config_.dns().domainStrategyForProxy);
    selectCurrentText(domainStrategyFreedomCombo_, config_.dns().domainStrategyForFreedom);
    fakeIpCheck_->setChecked(config_.dns().fakeIp);
    globalFakeIpCheck_->setChecked(config_.dns().globalFakeIp);
    serveStaleCheck_->setChecked(config_.dns().serveStale);
    parallelQueryCheck_->setChecked(config_.dns().parallelQuery);
    useSystemHostsCheck_->setChecked(config_.dns().useSystemHosts);
    addCommonHostsCheck_->setChecked(config_.dns().addCommonHosts);
    blockBindingQueryCheck_->setChecked(config_.dns().blockBindingQuery);
    directExpectedIpsEdit_->setText(config_.dns().directExpectedIps);
    hostsEdit_->setPlainText(config_.dns().dnsHosts);

    updateFieldState();
}

Config DnsSettingsDialog::config() const
{
    Config result = config_;
    result.dns().remoteDns = remoteDnsEdit_->text().trimmed();
    result.dns().directDns = directDnsEdit_->text().trimmed();
    result.dns().bootstrapDns = bootstrapDnsEdit_->text().trimmed();
    result.dns().domainStrategyForProxy = domainStrategyProxyCombo_->currentText().trimmed();
    result.dns().domainStrategyForFreedom = domainStrategyFreedomCombo_->currentText().trimmed();
    result.dns().fakeIp = fakeIpCheck_->isChecked();
    result.dns().globalFakeIp = globalFakeIpCheck_->isChecked();
    result.dns().serveStale = serveStaleCheck_->isChecked();
    result.dns().parallelQuery = parallelQueryCheck_->isChecked();
    result.dns().useSystemHosts = useSystemHostsCheck_->isChecked();
    result.dns().addCommonHosts = addCommonHostsCheck_->isChecked();
    result.dns().blockBindingQuery = blockBindingQueryCheck_->isChecked();
    result.dns().directExpectedIps = directExpectedIpsEdit_->text().trimmed();
    result.dns().dnsHosts = hostsEdit_->toPlainText();
    return result;
}

void DnsSettingsDialog::setupUi()
{
    setWindowTitle(tr("DNS Settings"));
    resize(620, 540);

    auto* layout = new QVBoxLayout(this);

    auto* serversGroup = new QGroupBox(tr("DNS Servers"), this);
    auto* serversForm = new QFormLayout(serversGroup);

    remoteDnsEdit_ = new QLineEdit(serversGroup);
    remoteDnsEdit_->setObjectName(QStringLiteral("dnsRemoteEdit"));
    remoteDnsEdit_->setPlaceholderText(tr("Used for proxied domains, e.g. https://cloudflare-dns.com/dns-query"));
    serversForm->addRow(tr("Remote DNS"), remoteDnsEdit_);

    directDnsEdit_ = new QLineEdit(serversGroup);
    directDnsEdit_->setObjectName(QStringLiteral("dnsDirectEdit"));
    directDnsEdit_->setPlaceholderText(tr("Used for direct connections, e.g. https://dns.alidns.com/dns-query"));
    serversForm->addRow(tr("Direct DNS"), directDnsEdit_);

    bootstrapDnsEdit_ = new QLineEdit(serversGroup);
    bootstrapDnsEdit_->setObjectName(QStringLiteral("dnsBootstrapEdit"));
    bootstrapDnsEdit_->setPlaceholderText(tr("Used to resolve DoH/DoT server names, e.g. 223.5.5.5"));
    serversForm->addRow(tr("Bootstrap DNS"), bootstrapDnsEdit_);

    auto* strategyGroup = new QGroupBox(tr("Domain Strategy"), this);
    auto* strategyForm = new QFormLayout(strategyGroup);

    domainStrategyProxyCombo_ = new QComboBox(strategyGroup);
    domainStrategyProxyCombo_->setObjectName(QStringLiteral("dnsStrategyProxyCombo"));
    domainStrategyProxyCombo_->setEditable(true);
    domainStrategyProxyCombo_->addItems(domainStrategyOptions());
    strategyForm->addRow(tr("Strategy for Proxy"), domainStrategyProxyCombo_);

    domainStrategyFreedomCombo_ = new QComboBox(strategyGroup);
    domainStrategyFreedomCombo_->setObjectName(QStringLiteral("dnsStrategyFreedomCombo"));
    domainStrategyFreedomCombo_->setEditable(true);
    domainStrategyFreedomCombo_->addItems(domainStrategyOptions());
    strategyForm->addRow(tr("Strategy for Freedom"), domainStrategyFreedomCombo_);

    auto* behaviourGroup = new QGroupBox(tr("Behaviour"), this);
    auto* behaviourLayout = new QVBoxLayout(behaviourGroup);

    fakeIpCheck_ = new QCheckBox(tr("Enable FakeIP"), behaviourGroup);
    fakeIpCheck_->setObjectName(QStringLiteral("dnsFakeIpCheck"));
    globalFakeIpCheck_ = new QCheckBox(tr("Global FakeIP (apply to all domains)"), behaviourGroup);
    globalFakeIpCheck_->setObjectName(QStringLiteral("dnsGlobalFakeIpCheck"));
    serveStaleCheck_ = new QCheckBox(tr("Serve stale answers when upstream fails"), behaviourGroup);
    serveStaleCheck_->setObjectName(QStringLiteral("dnsServeStaleCheck"));
    parallelQueryCheck_ = new QCheckBox(tr("Query upstream servers in parallel"), behaviourGroup);
    parallelQueryCheck_->setObjectName(QStringLiteral("dnsParallelCheck"));
    useSystemHostsCheck_ = new QCheckBox(tr("Read entries from the system hosts file"), behaviourGroup);
    useSystemHostsCheck_->setObjectName(QStringLiteral("dnsUseSystemHostsCheck"));
    addCommonHostsCheck_ = new QCheckBox(tr("Include built-in common hosts"), behaviourGroup);
    addCommonHostsCheck_->setObjectName(QStringLiteral("dnsAddCommonHostsCheck"));
    blockBindingQueryCheck_ = new QCheckBox(tr("Block binding queries on direct outbound"), behaviourGroup);
    blockBindingQueryCheck_->setObjectName(QStringLiteral("dnsBlockBindingCheck"));

    behaviourLayout->addWidget(fakeIpCheck_);
    behaviourLayout->addWidget(globalFakeIpCheck_);
    behaviourLayout->addWidget(serveStaleCheck_);
    behaviourLayout->addWidget(parallelQueryCheck_);
    behaviourLayout->addWidget(useSystemHostsCheck_);
    behaviourLayout->addWidget(addCommonHostsCheck_);
    behaviourLayout->addWidget(blockBindingQueryCheck_);

    connect(fakeIpCheck_, &QCheckBox::toggled, this, &DnsSettingsDialog::updateFieldState);

    auto* extrasGroup = new QGroupBox(tr("Extras"), this);
    auto* extrasForm = new QFormLayout(extrasGroup);

    directExpectedIpsEdit_ = new QLineEdit(extrasGroup);
    directExpectedIpsEdit_->setObjectName(QStringLiteral("dnsDirectExpectedIpsEdit"));
    directExpectedIpsEdit_->setPlaceholderText(tr("Comma-separated CIDRs that direct DNS answers must match"));
    extrasForm->addRow(tr("Direct Expected IPs"), directExpectedIpsEdit_);

    hostsEdit_ = new QPlainTextEdit(extrasGroup);
    hostsEdit_->setObjectName(QStringLiteral("dnsHostsEdit"));
    hostsEdit_->setPlaceholderText(tr("One host per line: example.com 1.2.3.4"));
    hostsEdit_->setMinimumHeight(90);
    extrasForm->addRow(tr("Hosts"), hostsEdit_);

    auto* buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    resetButton_ = buttonBox->addButton(tr("Reset"), QDialogButtonBox::ResetRole);
    saveButton_ = buttonBox->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    cancelButton_ = buttonBox->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    resetButton_->setObjectName(QStringLiteral("dnsResetButton"));
    saveButton_->setObjectName(QStringLiteral("dnsSaveButton"));
    cancelButton_->setObjectName(QStringLiteral("dnsCancelButton"));

    connect(resetButton_, &QPushButton::clicked, this, &DnsSettingsDialog::resetToDefaults);
    connect(saveButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);

    layout->addWidget(serversGroup);
    layout->addWidget(strategyGroup);
    layout->addWidget(behaviourGroup);
    layout->addWidget(extrasGroup);
    layout->addWidget(buttonBox);
}

void DnsSettingsDialog::updateFieldState()
{
    const bool fakeIp = fakeIpCheck_ != nullptr && fakeIpCheck_->isChecked();
    if (globalFakeIpCheck_ != nullptr) {
        globalFakeIpCheck_->setEnabled(fakeIp);
    }
}

void DnsSettingsDialog::resetToDefaults()
{
    Config defaults;
    setConfig(defaults);
}
