#include "ui/dialogs/AddServerDialog.h"

#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>

namespace {

QString joinCsv(const QStringList& values)
{
    QStringList result;
    result.reserve(values.size());
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }
    return result.join(QStringLiteral(","));
}

QStringList splitCsv(const QString& value)
{
    QStringList result;
    const QStringList parts = value.split(QChar(','), Qt::SkipEmptyParts);
    result.reserve(parts.size());
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }
    return result;
}

int muxOverrideIndex(const std::optional<bool>& value)
{
    if (!value.has_value()) {
        return 0;
    }
    return value.value() ? 1 : 2;
}

std::optional<bool> muxOverrideValue(int index)
{
    switch (index) {
    case 1:
        return true;
    case 2:
        return false;
    case 0:
    default:
        return std::nullopt;
    }
}

} // namespace

void AddServerDialog::setServer(const VmessItem& server)
{
    typeCombo_->setCurrentIndex(typeCombo_->findData(static_cast<int>(server.configType)));
    refillCoreOptions(server.configType, server.coreType);
    setCoreType(server.coreType);
    remarksEdit_->setText(server.remarks);
    addressEdit_->setText(server.address);
    portSpin_->setValue(server.port > 0 ? server.port : 443);
    if (server.configType == ConfigType::Naive) {
        credentialEdit_->setText(server.username);
        securityCombo_->setCurrentText(QString());
    } else {
        credentialEdit_->setText(server.id);
        securityCombo_->setCurrentText(server.security);
    }
    alterIdSpin_->setValue(server.alterId);
    flowEdit_->setText(server.flow);
    networkCombo_->setCurrentText(server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network);
    headerTypeCombo_->setCurrentText(server.headerType.trimmed().isEmpty() ? QStringLiteral("none") : server.headerType);
    streamSecurityCombo_->setCurrentText(server.streamSecurity);
    hostEdit_->setText(server.requestHost);
    pathEdit_->setText(server.path);
    sniEdit_->setText(server.sni);
    fingerprintEdit_->setText(server.fingerprint);
    certEdit_->setPlainText(server.cert);
    certShaEdit_->setText(server.certSha);
    echConfigListEdit_->setText(server.echConfigList);
    echForceQueryCombo_->setCurrentText(server.echForceQuery);
    publicKeyEdit_->setText(server.publicKey);
    shortIdEdit_->setText(server.shortId);
    spiderXEdit_->setText(server.spiderX);
    mldsa65VerifyEdit_->setText(server.mldsa65Verify);
    alpnEdit_->setText(joinCsv(server.alpn));
    userAgentCombo_->setCurrentText(server.userAgent);
    extraEdit_->setPlainText(server.extra);
    finalmaskEdit_->setPlainText(server.finalmask);
    muxOverrideCombo_->setCurrentIndex(muxOverrideIndex(server.muxEnabled));
    allowInsecureCombo_->setCurrentText(server.allowInsecure);
    obfsPasswordEdit_->setText(server.obfsPassword);
    upMbpsEdit_->setText(server.upMbps);
    downMbpsEdit_->setText(server.downMbps);
    congestionControlCombo_->setCurrentText(server.congestionControl.trimmed().isEmpty()
        ? QStringLiteral("cubic")
        : server.congestionControl);
    udpRelayModeCombo_->setCurrentText(server.udpRelayMode.trimmed().isEmpty()
        ? QStringLiteral("native")
        : server.udpRelayMode);
    privateKeyEdit_->setText(server.privateKey);
    peerPublicKeyEdit_->setText(server.peerPublicKey);
    localAddressEdit_->setText(server.localAddress);
    wireguardMtuSpin_->setValue(server.wireguardMtu);
    reservedEdit_->setText(server.reserved);
    naivePasswordEdit_->setText(server.configType == ConfigType::Naive ? server.id : QString());
    naiveQuicCheck_->setChecked(server.naiveQuic);
    naiveInsecureConcurrencySpin_->setValue(server.insecureConcurrency);
    updateFieldState();
}

VmessItem AddServerDialog::server() const
{
    VmessItem item;
    item.configType = static_cast<ConfigType>(typeCombo_->currentData().toInt());
    item.coreType = static_cast<CoreType>(coreCombo_->currentData().toInt());
    item.remarks = remarksEdit_->text().trimmed();
    item.address = addressEdit_->text().trimmed();
    item.port = portSpin_->value();
    item.id = credentialEdit_->text().trimmed();
    item.security = securityCombo_->currentText().trimmed();
    item.alterId = alterIdSpin_->value();
    item.flow = flowEdit_->text().trimmed();
    item.network = networkCombo_->currentText().trimmed();
    item.headerType = headerTypeCombo_->currentText().trimmed();
    item.streamSecurity = streamSecurityCombo_->currentText().trimmed();
    item.requestHost = hostEdit_->text().trimmed();
    item.path = pathEdit_->text().trimmed();
    item.sni = sniEdit_->text().trimmed();
    item.fingerprint = fingerprintEdit_->text().trimmed();
    item.cert = certEdit_->toPlainText().trimmed();
    item.certSha = certShaEdit_->text().trimmed();
    item.echConfigList = echConfigListEdit_->text().trimmed();
    item.echForceQuery = echForceQueryCombo_->currentText().trimmed();
    item.publicKey = publicKeyEdit_->text().trimmed();
    item.shortId = shortIdEdit_->text().trimmed();
    item.spiderX = spiderXEdit_->text().trimmed();
    item.mldsa65Verify = mldsa65VerifyEdit_->text().trimmed();
    item.alpn = splitCsv(alpnEdit_->text());
    item.userAgent = userAgentCombo_->currentText().trimmed();
    item.extra = extraEdit_->toPlainText().trimmed();
    item.finalmask = finalmaskEdit_->toPlainText().trimmed();
    item.muxEnabled = muxOverrideValue(muxOverrideCombo_->currentIndex());
    item.allowInsecure = allowInsecureCombo_->currentText();
    item.obfsPassword = obfsPasswordEdit_->text().trimmed();
    item.upMbps = upMbpsEdit_->text().trimmed();
    item.downMbps = downMbpsEdit_->text().trimmed();
    item.congestionControl = congestionControlCombo_->currentText().trimmed();
    item.udpRelayMode = udpRelayModeCombo_->currentText().trimmed();
    item.zeroRttHandshake = false;
    item.privateKey = privateKeyEdit_->text().trimmed();
    item.peerPublicKey = peerPublicKeyEdit_->text().trimmed();
    item.localAddress = localAddressEdit_->text().trimmed();
    item.wireguardMtu = wireguardMtuSpin_->value();
    item.reserved = reservedEdit_->text().trimmed();

    if (item.configType == ConfigType::Naive) {
        item.username = credentialEdit_->text().trimmed();
        item.id = naivePasswordEdit_->text().trimmed();
        item.security.clear();
        item.naiveQuic = naiveQuicCheck_->isChecked();
        item.insecureConcurrency = naiveInsecureConcurrencySpin_->value();
    }
    return item;
}
