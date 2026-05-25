#include "ui/dialogs/AddServerDialog.h"

#include "ui/dialogs/AddServerDialogOptions.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include "runtime/ProtocolCoreCompat.h"

AddServerDialog::AddServerDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    updateFieldState();
}

void AddServerDialog::updateFieldState()
{
    const ConfigType type = static_cast<ConfigType>(typeCombo_->currentData().toInt());
    refillSecurityOptions(type);
    refillCoreOptions(type, static_cast<CoreType>(coreCombo_->currentData().toInt()));

    const AddServerDialogOptions::ProtocolFieldState protocolState =
        AddServerDialogOptions::protocolFieldState(type);

    credentialLabel_->setText(protocolState.credentialLabelText);
    credentialGenerateButton_->setVisible(type == ConfigType::VMess || type == ConfigType::VLESS);
    securityLabel_->setText(protocolState.securityLabelText);

    alterIdLabel_->setVisible(protocolState.showAlterId);
    alterIdSpin_->setVisible(protocolState.showAlterId);
    flowLabel_->setVisible(protocolState.showFlow);
    flowEdit_->setVisible(protocolState.showFlow);

    const AddServerDialogOptions::TransportFieldState transportState =
        AddServerDialogOptions::transportFieldState(networkCombo_->currentText(), streamSecurityCombo_->currentText());

    headerTypeFieldLabel_->setText(transportState.headerLabelText);
    hostFieldLabel_->setText(transportState.hostLabelText);
    pathFieldLabel_->setText(transportState.pathLabelText);
    hostEdit_->setPlaceholderText(transportState.hostPlaceholder);
    pathEdit_->setPlaceholderText(transportState.pathPlaceholder);
    hostFieldLabel_->setVisible(transportState.showHost);
    hostEdit_->setVisible(transportState.showHost);

    fingerprintLabel_->setVisible(transportState.secureTransport);
    fingerprintEdit_->setVisible(transportState.secureTransport);
    certLabel_->setVisible(transportState.tlsTransport);
    certEdit_->setVisible(transportState.tlsTransport);
    certShaLabel_->setVisible(transportState.tlsTransport);
    certShaEdit_->setVisible(transportState.tlsTransport);
    echConfigListLabel_->setVisible(transportState.tlsTransport);
    echConfigListEdit_->setVisible(transportState.tlsTransport);
    echForceQueryLabel_->setVisible(transportState.tlsTransport);
    echForceQueryCombo_->setVisible(transportState.tlsTransport);
    publicKeyLabel_->setVisible(transportState.realityTransport);
    publicKeyEdit_->setVisible(transportState.realityTransport);
    shortIdLabel_->setVisible(transportState.realityTransport);
    shortIdEdit_->setVisible(transportState.realityTransport);
    spiderXLabel_->setVisible(transportState.realityTransport);
    spiderXEdit_->setVisible(transportState.realityTransport);
    mldsa65VerifyLabel_->setVisible(transportState.realityTransport);
    mldsa65VerifyEdit_->setVisible(transportState.realityTransport);
    alpnLabel_->setVisible(transportState.secureTransport);
    alpnEdit_->setVisible(transportState.secureTransport);
    userAgentLabel_->setVisible(transportState.httpupgradeTransport);
    userAgentCombo_->setVisible(transportState.httpupgradeTransport);
    extraLabel_->setVisible(transportState.xhttpTransport);
    extraEdit_->setVisible(transportState.xhttpTransport);
    allowInsecureLabel_->setVisible(transportState.secureTransport);
    allowInsecureCombo_->setVisible(transportState.secureTransport);

    if (securityCombo_->currentText().trimmed().isEmpty() && !protocolState.defaultSecurity.isEmpty()) {
        securityCombo_->setCurrentText(protocolState.defaultSecurity);
    }

    hysteria2Group_->setVisible(type == ConfigType::Hysteria2);
    tuicGroup_->setVisible(type == ConfigType::TUIC);
    wireguardGroup_->setVisible(type == ConfigType::WireGuard);
    naiveGroup_->setVisible(type == ConfigType::Naive);

    const bool isQuicProtocol = type == ConfigType::Hysteria2 || type == ConfigType::TUIC;
    const bool isWireguard = type == ConfigType::WireGuard;

    networkCombo_->setVisible(!isQuicProtocol && !isWireguard);
    headerTypeFieldLabel_->setVisible(!isQuicProtocol && !isWireguard);
    headerTypeCombo_->setVisible(!isQuicProtocol && !isWireguard);
    streamSecurityCombo_->setVisible(!isQuicProtocol && !isWireguard);
    hostFieldLabel_->setVisible(!isQuicProtocol && !isWireguard && transportState.showHost);
    hostEdit_->setVisible(!isQuicProtocol && !isWireguard && transportState.showHost);
    pathFieldLabel_->setVisible(!isQuicProtocol && !isWireguard);
    pathEdit_->setVisible(!isQuicProtocol && !isWireguard);
    sniEdit_->setVisible(!isWireguard);
    muxOverrideLabel_->setVisible(!isQuicProtocol && !isWireguard);
    muxOverrideCombo_->setVisible(!isQuicProtocol && !isWireguard);
    extraLabel_->setVisible(!isQuicProtocol && !isWireguard && transportState.xhttpTransport);
    extraEdit_->setVisible(!isQuicProtocol && !isWireguard && transportState.xhttpTransport);
    finalmaskLabel_->setVisible(!isQuicProtocol && !isWireguard);
    finalmaskEdit_->setVisible(!isQuicProtocol && !isWireguard);
}

void AddServerDialog::applyDefaultCoreTypeForCurrentProtocol()
{
    if (coreCombo_ == nullptr || typeCombo_ == nullptr) {
        return;
    }

    const ConfigType type = static_cast<ConfigType>(typeCombo_->currentData().toInt());
    refillCoreOptions(type, defaultCoreTypeForProtocol(type));
}

void AddServerDialog::refillCoreOptions(ConfigType type, CoreType preferredCore)
{
    if (coreCombo_ == nullptr) {
        return;
    }

    const CoreType resolvedPreferred = resolveRuntimeCoreType(preferredCore);
    QList<CoreType> cores = prioritizedCoreTypesForProtocol(type);
    if (cores.isEmpty()) {
        cores = orderedCoreTypes();
    }

    coreCombo_->blockSignals(true);
    coreCombo_->clear();
    const CoreType defaultCore = defaultCoreTypeForProtocol(type);
    for (const CoreType core : cores) {
        QString label = coreTypeDisplayName(core);
        if (core == defaultCore) {
            label = tr("%1 (Default)").arg(label);
        }
        coreCombo_->addItem(label, static_cast<int>(core));
    }

    int selectedIndex = coreCombo_->findData(static_cast<int>(resolvedPreferred));
    if (selectedIndex < 0) {
        selectedIndex = coreCombo_->findData(static_cast<int>(defaultCore));
    }
    coreCombo_->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    coreCombo_->blockSignals(false);
}

void AddServerDialog::refillSecurityOptions(ConfigType type)
{
    const QString currentText = securityCombo_->currentText().trimmed();
    securityCombo_->clear();
    securityCombo_->addItems(AddServerDialogOptions::securityOptions(type));
    if (!currentText.isEmpty()) {
        securityCombo_->setCurrentText(currentText);
    }
}

void AddServerDialog::setCoreType(CoreType type)
{
    const int index = coreCombo_->findData(static_cast<int>(type));
    coreCombo_->setCurrentIndex(index >= 0 ? index : 0);
}
