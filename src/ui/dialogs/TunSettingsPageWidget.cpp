#include "ui/dialogs/TunSettingsPageWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QStringList>
#include <QSpinBox>

#include "ui/theme/AppTheme.h"

namespace {

QStringList tunRoutingPolicyOptions()
{
    return {
        QStringLiteral("rule"),
        QStringLiteral("direct"),
        QStringLiteral("unreachable"),
        QStringLiteral("drop"),
        QStringLiteral("reply")};
}

} // namespace

TunSettingsPageWidget::TunSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    updateFieldState();
}

void TunSettingsPageWidget::setConfig(const Config& config)
{
    const TunModeItem& tun = config.tun().tunModeItem;
    tunEnableCheck_->setChecked(tun.enableTun);
    tunAutoRouteCheck_->setChecked(tun.autoRoute);
    tunStrictRouteCheck_->setChecked(tun.strictRoute);
    tunMtuSpin_->setValue(tun.mtu > 0 ? tun.mtu : 9000);
    tunStackCombo_->setCurrentText(tun.stack.isEmpty() ? QStringLiteral("system") : tun.stack);
    tunEnableIPv6AddressCheck_->setChecked(tun.enableIPv6Address);
    tunIcmpRoutingCombo_->setCurrentText(tun.icmpRouting);
    tunUdpRoutingCombo_->setCurrentText(tun.udpRouting);
    updateFieldState();
}

void TunSettingsPageWidget::applyToConfig(Config& config) const
{
    TunModeItem& tun = config.tun().tunModeItem;
    tun.enableTun = tunEnableCheck_->isChecked();
    tun.autoRoute = tunAutoRouteCheck_->isChecked();
    tun.strictRoute = tunStrictRouteCheck_->isChecked();
    tun.mtu = tunMtuSpin_->value();
    tun.stack = tunStackCombo_->currentText().trimmed();
    tun.enableIPv6Address = tunEnableIPv6AddressCheck_->isChecked();
    tun.icmpRouting = tunIcmpRoutingCombo_->currentText().trimmed();
    tun.udpRouting = tunUdpRoutingCombo_->currentText().trimmed();
}

bool TunSettingsPageWidget::tunEnabled() const
{
    return tunEnableCheck_ != nullptr && tunEnableCheck_->isChecked();
}

void TunSettingsPageWidget::setupUi()
{
    auto* tunLayout = new QFormLayout(this);

    tunEnableCheck_ = new QCheckBox(tr("Enable TUN mode (requires admin privileges)"), this);
    tunEnableCheck_->setObjectName(QStringLiteral("settingsTunEnableCheck"));
    tunAutoRouteCheck_ = new QCheckBox(tr("Auto route (route all traffic through TUN)"), this);
    tunAutoRouteCheck_->setObjectName(QStringLiteral("settingsTunAutoRouteCheck"));
    tunStrictRouteCheck_ = new QCheckBox(tr("Strict route (apply strict routing rules)"), this);
    tunStrictRouteCheck_->setObjectName(QStringLiteral("settingsTunStrictRouteCheck"));

    tunMtuSpin_ = new QSpinBox(this);
    tunMtuSpin_->setObjectName(QStringLiteral("settingsTunMtuSpin"));
    tunMtuSpin_->setRange(0, 99999);
    tunMtuSpin_->setSpecialValueText(tr("Auto (9000)"));

    tunStackCombo_ = new QComboBox(this);
    tunStackCombo_->setObjectName(QStringLiteral("settingsTunStackCombo"));
    tunStackCombo_->addItems({
        QStringLiteral("system"),
        QStringLiteral("gvisor"),
        QStringLiteral("mixed")
    });

    tunEnableIPv6AddressCheck_ = new QCheckBox(tr("Enable IPv6 address"), this);
    tunEnableIPv6AddressCheck_->setObjectName(QStringLiteral("settingsTunEnableIPv6AddressCheck"));
    tunIcmpRoutingCombo_ = new QComboBox(this);
    tunIcmpRoutingCombo_->setObjectName(QStringLiteral("settingsTunIcmpRoutingCombo"));
    tunIcmpRoutingCombo_->setEditable(true);
    tunIcmpRoutingCombo_->addItems(tunRoutingPolicyOptions());
    tunIcmpRoutingCombo_->lineEdit()->setPlaceholderText(tr("Optional ICMP routing preference"));
    tunUdpRoutingCombo_ = new QComboBox(this);
    tunUdpRoutingCombo_->setObjectName(QStringLiteral("settingsTunUdpRoutingCombo"));
    tunUdpRoutingCombo_->setEditable(true);
    tunUdpRoutingCombo_->addItems(tunRoutingPolicyOptions());
    tunUdpRoutingCombo_->lineEdit()->setPlaceholderText(tr("UDP routing preference"));

    AppTheme::applyCompactFont({
        tunEnableCheck_,
        tunAutoRouteCheck_,
        tunStrictRouteCheck_,
        tunMtuSpin_,
        tunStackCombo_,
        tunEnableIPv6AddressCheck_,
        tunIcmpRoutingCombo_,
        tunUdpRoutingCombo_});

    tunLayout->setWidget(tunLayout->rowCount(), QFormLayout::SpanningRole, tunEnableCheck_);
    tunLayout->setWidget(tunLayout->rowCount(), QFormLayout::SpanningRole, tunAutoRouteCheck_);
    tunLayout->setWidget(tunLayout->rowCount(), QFormLayout::SpanningRole, tunStrictRouteCheck_);
    tunLayout->setWidget(tunLayout->rowCount(), QFormLayout::SpanningRole, tunEnableIPv6AddressCheck_);
    tunLayout->addRow(tr("MTU"), tunMtuSpin_);
    tunLayout->addRow(tr("Stack"), tunStackCombo_);
    tunLayout->addRow(tr("ICMP Routing"), tunIcmpRoutingCombo_);
    tunLayout->addRow(tr("UDP Routing"), tunUdpRoutingCombo_);

    connect(tunEnableCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        updateFieldState();
        emit tunEnabledChanged(enabled);
    });
}

void TunSettingsPageWidget::updateFieldState()
{
    const bool tunEnabled = tunEnableCheck_ != nullptr && tunEnableCheck_->isChecked();
    if (tunAutoRouteCheck_ != nullptr) {
        tunAutoRouteCheck_->setEnabled(tunEnabled);
    }
    if (tunStrictRouteCheck_ != nullptr) {
        tunStrictRouteCheck_->setEnabled(tunEnabled);
    }
    if (tunMtuSpin_ != nullptr) {
        tunMtuSpin_->setEnabled(tunEnabled);
    }
    if (tunStackCombo_ != nullptr) {
        tunStackCombo_->setEnabled(tunEnabled);
    }
    if (tunEnableIPv6AddressCheck_ != nullptr) {
        tunEnableIPv6AddressCheck_->setEnabled(tunEnabled);
    }
    if (tunIcmpRoutingCombo_ != nullptr) {
        tunIcmpRoutingCombo_->setEnabled(tunEnabled);
    }
    if (tunUdpRoutingCombo_ != nullptr) {
        tunUdpRoutingCombo_->setEnabled(tunEnabled);
    }
}
