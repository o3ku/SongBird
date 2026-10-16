#include "ui/dialogs/DnsSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSizePolicy>
#include <QTextEdit>

#include "ui/theme/AppTheme.h"

#include "runtime/DnsAddressParser.h"
#include "runtime/DnsHosts.h"

namespace {

// Xray reads these values verbatim, and the sing-box mapping in
// SingBoxDnsConfigSupport::mapDomainStrategy understands the same spellings (the
// legacy Prefer*/Only* names are still accepted for saved configs). Offering
// anything outside this list would silently reach a core unmapped.
//
// The list is the enumeration both cores accept where the value lands: Xray's
// outbound `targetStrategy` / `sockopt.domainStrategy` and sing-box's
// `dns.strategy`. AsIs is the one exception, because it says "let the core
// decide" and so intentionally maps to no sing-box strategy. Xray's routing-only
// IPIfNonMatch / IPOnDemand are deliberately absent: they belong to
// `routing.domainStrategy`, and this value is written verbatim into the outbound
// strategy above, where Xray would reject them.
QStringList domainStrategyOptions()
{
    return {
        QString(),
        QStringLiteral("AsIs"),
        QStringLiteral("UseIPv4"),
        QStringLiteral("UseIPv6"),
        QStringLiteral("UseIPv4v6"),
        QStringLiteral("UseIPv6v4"),
        QStringLiteral("ForceIPv4"),
        QStringLiteral("ForceIPv6"),
        QStringLiteral("ForceIPv4v6"),
        QStringLiteral("ForceIPv6v4")};
}

// Bootstrap DNS is only consulted to name a server that is itself addressed by hostname, so it
// does nothing while both address fields hold IPs -- which is what ships by default. Looking at
// the host part rather than the whole address keeps this correct for the scheme prefixes and
// port suffixes the fields accept, and an empty field is treated as "no opinion" so clearing one
// never hides a bootstrap that the other field still needs.
bool bootstrapPlaceholderNeeded(const QString& remoteDns, const QString& directDns)
{
    const auto isHostnameAddressed = [](const QString& address) {
        const QString trimmed = address.trimmed();
        if (trimmed.isEmpty()) {
            return false;
        }
        const QString host = DnsAddressParser::extractHost(trimmed);
        return !host.isEmpty() && host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) != 0
               && !DnsHosts::isIpAddress(host);
    };
    return isHostnameAddressed(remoteDns) || isHostnameAddressed(directDns);
}

} // namespace

DnsSettingsPageWidget::DnsSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DnsSettingsPageWidget::setConfig(const Config& config)
{
    remoteDnsEdit_->setText(config.dns().remoteDns);
    directDnsEdit_->setText(config.dns().directDns);
    bootstrapDnsEdit_->setText(config.dns().bootstrapDns);
    domainStrategyForFreedomCombo_->setCurrentText(config.dns().domainStrategyForFreedom);
    domainStrategyForProxyCombo_->setCurrentText(config.dns().domainStrategyForProxy);
    useSystemHostsCheck_->setChecked(config.dns().useSystemHosts);
    addCommonHostsCheck_->setChecked(config.dns().addCommonHosts);
    fakeIpCheck_->setChecked(config.dns().fakeIp);
    dnsHostsEdit_->setPlainText(config.dns().dnsHosts);
}

void DnsSettingsPageWidget::applyToConfig(Config& config) const
{
    config.dns().remoteDns = remoteDnsEdit_->text().trimmed();
    config.dns().directDns = directDnsEdit_->text().trimmed();
    config.dns().bootstrapDns = bootstrapDnsEdit_->text().trimmed();
    config.dns().domainStrategyForFreedom = domainStrategyForFreedomCombo_->currentText().trimmed();
    config.dns().domainStrategyForProxy = domainStrategyForProxyCombo_->currentText().trimmed();
    config.dns().useSystemHosts = useSystemHostsCheck_->isChecked();
    config.dns().addCommonHosts = addCommonHostsCheck_->isChecked();
    config.dns().fakeIp = fakeIpCheck_->isChecked();
    config.dns().dnsHosts = dnsHostsEdit_->toPlainText().trimmed();
}

void DnsSettingsPageWidget::setupUi()
{
    auto* dnsLayout = new QFormLayout(this);

    remoteDnsEdit_ = new QLineEdit(this);
    remoteDnsEdit_->setObjectName(QStringLiteral("settingsRemoteDnsEdit"));
    remoteDnsEdit_->setPlaceholderText(QStringLiteral("https://1.1.1.1/dns-query"));
    // The three addresses are not three interchangeable servers: Remote is queried through the
    // proxy, Direct outside it, and Bootstrap exists only to name the two above when their
    // address is a hostname. The tooltips carry that so the page does not have to.
    remoteDnsEdit_->setToolTip(tr("Queried through the proxy. Used for domains that must not be resolved directly."));
    directDnsEdit_ = new QLineEdit(this);
    directDnsEdit_->setObjectName(QStringLiteral("settingsDirectDnsEdit"));
    directDnsEdit_->setPlaceholderText(QStringLiteral("https://223.5.5.5/dns-query"));
    directDnsEdit_->setToolTip(tr("Queried directly, without the proxy. Used for domains that resolve best locally."));

    bootstrapDnsEdit_ = new QLineEdit(this);
    bootstrapDnsEdit_->setObjectName(QStringLiteral("settingsBootstrapDnsEdit"));
    bootstrapDnsEdit_->setPlaceholderText(QStringLiteral("https://223.5.5.5/dns-query"));
    bootstrapDnsEdit_->setToolTip(tr("Resolves the hostnames of the two servers above. Only needed when Remote DNS or Direct DNS is a hostname rather than an IP; leave empty when they are IP addresses."));

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
    fakeIpCheck_ = new QCheckBox(tr("Enable FakeIP"), this);
    fakeIpCheck_->setObjectName(QStringLiteral("dnsFakeIpCheck"));

    dnsHostsEdit_ = new QTextEdit(this);
    dnsHostsEdit_->setObjectName(QStringLiteral("settingsDnsHostsEdit"));
    dnsHostsEdit_->setTabChangesFocus(true);
    dnsHostsEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    dnsHostsEdit_->setPlaceholderText(QStringLiteral("domain ip\nexample.com 1.2.3.4"));
    AppTheme::applyCompactFont({
        remoteDnsEdit_,
        directDnsEdit_,
        bootstrapDnsEdit_,
        domainStrategyForFreedomCombo_,
        domainStrategyForProxyCombo_,
        useSystemHostsCheck_,
        addCommonHostsCheck_,
        fakeIpCheck_,
        dnsHostsEdit_});

    dnsLayout->addRow(tr("Remote DNS"), remoteDnsEdit_);
    dnsLayout->addRow(tr("Direct DNS"), directDnsEdit_);
    dnsLayout->addRow(tr("Bootstrap DNS"), bootstrapDnsEdit_);    dnsLayout->addRow(tr("Domain Strategy (Freedom)"), domainStrategyForFreedomCombo_);
    dnsLayout->addRow(tr("Domain Strategy (Proxy)"), domainStrategyForProxyCombo_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, useSystemHostsCheck_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, addCommonHostsCheck_);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, fakeIpCheck_);
    dnsLayout->addRow(tr("DNS Hosts"), dnsHostsEdit_);
    // The hosts box used to be capped at 120px, and QFormLayout spends a row's surplus height on
    // its resizable rows, so the leftover went into the gaps between rows instead of anywhere
    // useful: this page read 35-37px between rows while General and TUN read 24-26px, and the
    // extra space vanished. Letting the box expand vertically takes that slack itself, which
    // restores the 24-26px rhythm and gives the hosts text somewhere to go at the same time.
    // QFormLayout has no addStretch, so the row after it is a zero-height spacer that keeps the
    // dialog's height from growing further once the box has absorbed the slack.
    formFiller_ = new QWidget(this);
    formFiller_->setObjectName(QStringLiteral("settingsBottomFiller"));
    formFiller_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    formFiller_->setFixedHeight(0);
    dnsLayout->setWidget(dnsLayout->rowCount(), QFormLayout::SpanningRole, formFiller_);

    // The sing-box-only "Global FakeIP" moved to the Core page with the rest of the
    // sing-box settings, so that page needs to follow this checkbox.
    connect(fakeIpCheck_, &QCheckBox::toggled, this, &DnsSettingsPageWidget::fakeIpEnabledChanged);

    // Bootstrap DNS is only reached when the two servers above are addressed by hostname, so
    // its hint says so rather than asking the user to guess: with the default IP addresses it
    // reports "not needed", and it asks for a value only once a hostname actually appears.
    const auto refreshBootstrapHint = [this]() {
        if (bootstrapDnsEdit_ == nullptr) {
            return;
        }
        bootstrapDnsEdit_->setPlaceholderText(
            bootstrapPlaceholderNeeded(remoteDnsEdit_->text(), directDnsEdit_->text())
                ? QStringLiteral("https://223.5.5.5/dns-query")
                : tr("Not needed - the servers above use IP addresses"));
    };
    connect(remoteDnsEdit_, &QLineEdit::textChanged, this, refreshBootstrapHint);
    connect(directDnsEdit_, &QLineEdit::textChanged, this, refreshBootstrapHint);
    refreshBootstrapHint();
}
