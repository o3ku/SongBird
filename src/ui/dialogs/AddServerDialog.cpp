#include "ui/dialogs/AddServerDialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QUuid>
#include <QVBoxLayout>

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

AddServerDialog::AddServerDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    updateFieldState();
}

void AddServerDialog::setServer(const VmessItem& server)
{
    typeCombo_->setCurrentIndex(typeCombo_->findData(static_cast<int>(server.configType)));
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
    // Hysteria2
    obfsPasswordEdit_->setText(server.obfsPassword);
    upMbpsEdit_->setText(server.upMbps);
    downMbpsEdit_->setText(server.downMbps);
    // TUIC
    congestionControlCombo_->setCurrentText(server.congestionControl.trimmed().isEmpty() ? QStringLiteral("cubic") : server.congestionControl);
    udpRelayModeCombo_->setCurrentText(server.udpRelayMode.trimmed().isEmpty() ? QStringLiteral("native") : server.udpRelayMode);
    // WireGuard
    privateKeyEdit_->setText(server.privateKey);
    peerPublicKeyEdit_->setText(server.peerPublicKey);
    localAddressEdit_->setText(server.localAddress);
    wireguardMtuSpin_->setValue(server.wireguardMtu);
    reservedEdit_->setText(server.reserved);
    // Naive
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
    // Hysteria2
    item.obfsPassword = obfsPasswordEdit_->text().trimmed();
    item.upMbps = upMbpsEdit_->text().trimmed();
    item.downMbps = downMbpsEdit_->text().trimmed();
    // TUIC
    item.congestionControl = congestionControlCombo_->currentText().trimmed();
    item.udpRelayMode = udpRelayModeCombo_->currentText().trimmed();
    item.zeroRttHandshake = false;
    // WireGuard
    item.privateKey = privateKeyEdit_->text().trimmed();
    item.peerPublicKey = peerPublicKeyEdit_->text().trimmed();
    item.localAddress = localAddressEdit_->text().trimmed();
    item.wireguardMtu = wireguardMtuSpin_->value();
    item.reserved = reservedEdit_->text().trimmed();
    // Naive: reinterpret credential field (username) and read password from its own group.
    if (item.configType == ConfigType::Naive) {
        item.username = credentialEdit_->text().trimmed();
        item.id = naivePasswordEdit_->text().trimmed();
        item.security.clear();
        item.naiveQuic = naiveQuicCheck_->isChecked();
        item.insecureConcurrency = naiveInsecureConcurrencySpin_->value();
    }
    return item;
}

void AddServerDialog::updateFieldState()
{
    const ConfigType type = static_cast<ConfigType>(typeCombo_->currentData().toInt());
    refillSecurityOptions(type);

    bool showAlterId = false;
    bool showFlow = false;
    QString credentialLabelText = QStringLiteral("Credential");
    QString securityLabelText = QStringLiteral("Security");
    QString defaultSecurity;

    switch (type) {
    case ConfigType::VMess:
        credentialLabelText = QStringLiteral("UUID");
        securityLabelText = QStringLiteral("Encryption");
        defaultSecurity = QStringLiteral("auto");
        showAlterId = true;
        break;
    case ConfigType::VLESS:
        credentialLabelText = QStringLiteral("UUID");
        securityLabelText = QStringLiteral("Encryption");
        defaultSecurity = QStringLiteral("none");
        showFlow = true;
        break;
    case ConfigType::Trojan:
        credentialLabelText = QStringLiteral("Password");
        securityLabelText = QStringLiteral("Security");
        defaultSecurity.clear();
        showFlow = true;
        break;
    case ConfigType::Shadowsocks:
        credentialLabelText = QStringLiteral("Password");
        securityLabelText = QStringLiteral("Method");
        defaultSecurity = QStringLiteral("aes-128-gcm");
        break;
    case ConfigType::Socks:
    case ConfigType::HTTP:
        credentialLabelText = QStringLiteral("Username");
        securityLabelText = QStringLiteral("Password");
        defaultSecurity.clear();
        break;
    case ConfigType::Hysteria2:
        credentialLabelText = QStringLiteral("Password");
        securityLabelText = QStringLiteral("Security");
        defaultSecurity.clear();
        break;
    case ConfigType::TUIC:
        credentialLabelText = QStringLiteral("UUID");
        securityLabelText = QStringLiteral("Password");
        defaultSecurity.clear();
        break;
    case ConfigType::WireGuard:
        credentialLabelText = QStringLiteral("Private Key");
        securityLabelText = QStringLiteral("Security");
        defaultSecurity.clear();
        break;
    case ConfigType::AnyTLS:
        credentialLabelText = QStringLiteral("Password");
        securityLabelText = QStringLiteral("Security");
        defaultSecurity.clear();
        break;
    case ConfigType::Naive:
        credentialLabelText = QStringLiteral("Username");
        securityLabelText = QStringLiteral("Security");
        defaultSecurity.clear();
        break;
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        break;
    }

    credentialLabel_->setText(credentialLabelText);
    credentialGenerateButton_->setVisible(type == ConfigType::VMess || type == ConfigType::VLESS);
    securityLabel_->setText(securityLabelText);

    alterIdLabel_->setVisible(showAlterId);
    alterIdSpin_->setVisible(showAlterId);
    flowLabel_->setVisible(showFlow);
    flowEdit_->setVisible(showFlow);

    const QString transportSecurity = streamSecurityCombo_->currentText().trimmed();
    const bool secureTransport = transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0
        || transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0
        || transportSecurity.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0;
    const bool tlsTransport = transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0
        || transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0;
    const bool realityTransport = transportSecurity.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0;
    const QString network = networkCombo_->currentText().trimmed().toLower();

    QString headerLabelText = QStringLiteral("Header Type");
    QString hostLabelText = QStringLiteral("Host");
    QString pathLabelText = QStringLiteral("Path");
    QString hostPlaceholder = QStringLiteral("host.example.com");
    QString pathPlaceholder = QStringLiteral("/");
    bool showHost = true;
    const bool httpupgradeTransport = network == QStringLiteral("httpupgrade");
    const bool xhttpTransport = network == QStringLiteral("xhttp");

    if (network == QStringLiteral("grpc")) {
        headerLabelText = QStringLiteral("Mode");
        pathLabelText = QStringLiteral("Service Name");
        pathPlaceholder = QStringLiteral("service-name");
        showHost = false;
    } else if (xhttpTransport) {
        headerLabelText = QStringLiteral("Mode");
    } else if (network == QStringLiteral("kcp")) {
        pathLabelText = QStringLiteral("Seed");
        pathPlaceholder = QStringLiteral("mkcp-seed");
        showHost = false;
    } else if (network == QStringLiteral("quic")) {
        hostLabelText = QStringLiteral("QUIC Security");
        pathLabelText = QStringLiteral("Key");
        hostPlaceholder = QStringLiteral("none");
        pathPlaceholder = QStringLiteral("quic-key");
    }

    headerTypeFieldLabel_->setText(headerLabelText);
    hostFieldLabel_->setText(hostLabelText);
    pathFieldLabel_->setText(pathLabelText);
    hostEdit_->setPlaceholderText(hostPlaceholder);
    pathEdit_->setPlaceholderText(pathPlaceholder);
    hostFieldLabel_->setVisible(showHost);
    hostEdit_->setVisible(showHost);

    fingerprintLabel_->setVisible(secureTransport);
    fingerprintEdit_->setVisible(secureTransport);
    certLabel_->setVisible(tlsTransport);
    certEdit_->setVisible(tlsTransport);
    certShaLabel_->setVisible(tlsTransport);
    certShaEdit_->setVisible(tlsTransport);
    echConfigListLabel_->setVisible(tlsTransport);
    echConfigListEdit_->setVisible(tlsTransport);
    echForceQueryLabel_->setVisible(tlsTransport);
    echForceQueryCombo_->setVisible(tlsTransport);
    publicKeyLabel_->setVisible(realityTransport);
    publicKeyEdit_->setVisible(realityTransport);
    shortIdLabel_->setVisible(realityTransport);
    shortIdEdit_->setVisible(realityTransport);
    spiderXLabel_->setVisible(realityTransport);
    spiderXEdit_->setVisible(realityTransport);
    mldsa65VerifyLabel_->setVisible(realityTransport);
    mldsa65VerifyEdit_->setVisible(realityTransport);
    alpnLabel_->setVisible(secureTransport);
    alpnEdit_->setVisible(secureTransport);
    userAgentLabel_->setVisible(httpupgradeTransport);
    userAgentCombo_->setVisible(httpupgradeTransport);
    extraLabel_->setVisible(xhttpTransport);
    extraEdit_->setVisible(xhttpTransport);
    allowInsecureLabel_->setVisible(secureTransport);
    allowInsecureCombo_->setVisible(secureTransport);

    if (securityCombo_->currentText().trimmed().isEmpty() && !defaultSecurity.isEmpty()) {
        securityCombo_->setCurrentText(defaultSecurity);
    }

    // Protocol-specific group visibility
    hysteria2Group_->setVisible(type == ConfigType::Hysteria2);
    tuicGroup_->setVisible(type == ConfigType::TUIC);
    wireguardGroup_->setVisible(type == ConfigType::WireGuard);
    naiveGroup_->setVisible(type == ConfigType::Naive);

    // For Hysteria2/TUIC/WireGuard: force TLS on and hide certain fields
    const bool isQuicProtocol = type == ConfigType::Hysteria2 || type == ConfigType::TUIC;
    const bool isWireguard = type == ConfigType::WireGuard;

    // WireGuard: hide network, stream security, transport fields
    networkCombo_->setVisible(!isQuicProtocol && !isWireguard);
    headerTypeFieldLabel_->setVisible(!isQuicProtocol && !isWireguard);
    headerTypeCombo_->setVisible(!isQuicProtocol && !isWireguard);
    streamSecurityCombo_->setVisible(!isQuicProtocol && !isWireguard);
    hostFieldLabel_->setVisible(!isQuicProtocol && !isWireguard && showHost);
    hostEdit_->setVisible(!isQuicProtocol && !isWireguard && showHost);
    pathFieldLabel_->setVisible(!isQuicProtocol && !isWireguard);
    pathEdit_->setVisible(!isQuicProtocol && !isWireguard);
    sniEdit_->setVisible(!isWireguard);
    muxOverrideLabel_->setVisible(!isQuicProtocol && !isWireguard);
    muxOverrideCombo_->setVisible(!isQuicProtocol && !isWireguard);
    extraLabel_->setVisible(!isQuicProtocol && !isWireguard && xhttpTransport);
    extraEdit_->setVisible(!isQuicProtocol && !isWireguard && xhttpTransport);
    finalmaskLabel_->setVisible(!isQuicProtocol && !isWireguard);
    finalmaskEdit_->setVisible(!isQuicProtocol && !isWireguard);
}

void AddServerDialog::refillSecurityOptions(ConfigType type)
{
    const QString currentText = securityCombo_->currentText().trimmed();
    securityCombo_->clear();

    QStringList options;
    switch (type) {
    case ConfigType::VMess:
        options = QStringList{
            QStringLiteral("auto"),
            QStringLiteral("aes-128-gcm"),
            QStringLiteral("chacha20-poly1305"),
            QStringLiteral("none")
        };
        break;
    case ConfigType::VLESS:
        options = QStringList{
            QStringLiteral("none")
        };
        break;
    case ConfigType::Shadowsocks:
        options = QStringList{
            QStringLiteral("aes-256-gcm"),
            QStringLiteral("aes-128-gcm"),
            QStringLiteral("chacha20-poly1305"),
            QStringLiteral("chacha20-ietf-poly1305"),
            QStringLiteral("xchacha20-poly1305"),
            QStringLiteral("2022-blake3-aes-128-gcm"),
            QStringLiteral("2022-blake3-aes-256-gcm")
        };
        break;
    case ConfigType::Trojan:
    case ConfigType::Socks:
    case ConfigType::HTTP:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::WireGuard:
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        break;
    }

    securityCombo_->addItems(options);
    if (!currentText.isEmpty()) {
        securityCombo_->setCurrentText(currentText);
    }
}

void AddServerDialog::setCoreType(CoreType type)
{
    const int index = coreCombo_->findData(static_cast<int>(type));
    coreCombo_->setCurrentIndex(index >= 0 ? index : 0);
}

void AddServerDialog::setupUi()
{
    setWindowTitle(QStringLiteral("Add Server"));
    resize(460, 560);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem(QStringLiteral("VMess"), static_cast<int>(ConfigType::VMess));
    typeCombo_->addItem(QStringLiteral("VLESS"), static_cast<int>(ConfigType::VLESS));
    typeCombo_->addItem(QStringLiteral("Trojan"), static_cast<int>(ConfigType::Trojan));
    typeCombo_->addItem(QStringLiteral("Shadowsocks"), static_cast<int>(ConfigType::Shadowsocks));
    typeCombo_->addItem(QStringLiteral("Socks"), static_cast<int>(ConfigType::Socks));
    typeCombo_->addItem(QStringLiteral("HTTP"), static_cast<int>(ConfigType::HTTP));
    typeCombo_->addItem(QStringLiteral("Hysteria2"), static_cast<int>(ConfigType::Hysteria2));
    typeCombo_->addItem(QStringLiteral("TUIC"), static_cast<int>(ConfigType::TUIC));
    typeCombo_->addItem(QStringLiteral("WireGuard"), static_cast<int>(ConfigType::WireGuard));
    typeCombo_->addItem(QStringLiteral("AnyTLS"), static_cast<int>(ConfigType::AnyTLS));
    typeCombo_->addItem(QStringLiteral("Naive"), static_cast<int>(ConfigType::Naive));

    coreCombo_ = new QComboBox(this);
    coreCombo_->addItem(QStringLiteral("Auto (Xray)"), static_cast<int>(CoreType::Auto));
    coreCombo_->addItem(QStringLiteral("Xray"), static_cast<int>(CoreType::Xray));
    coreCombo_->addItem(QStringLiteral("V2Ray"), static_cast<int>(CoreType::V2Fly));
    coreCombo_->addItem(QStringLiteral("SagerNet"), static_cast<int>(CoreType::SagerNet));
    coreCombo_->addItem(QStringLiteral("V2Ray v5"), static_cast<int>(CoreType::V2FlyV5));
    coreCombo_->addItem(QStringLiteral("sing-box"), static_cast<int>(CoreType::SingBox));

    remarksEdit_ = new QLineEdit(this);
    addressEdit_ = new QLineEdit(this);
    addressEdit_->setPlaceholderText(QStringLiteral("example.com"));

    portSpin_ = new QSpinBox(this);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(443);

    credentialLabel_ = new QLabel(this);
    credentialRowWidget_ = new QWidget(this);
    credentialEdit_ = new QLineEdit(this);
    credentialGenerateButton_ = new QPushButton(QStringLiteral("Generate UUID"), this);

    auto* credentialLayout = new QHBoxLayout(credentialRowWidget_);
    credentialLayout->setContentsMargins(0, 0, 0, 0);
    credentialLayout->addWidget(credentialEdit_, 1);
    credentialLayout->addWidget(credentialGenerateButton_);

    securityLabel_ = new QLabel(this);
    securityCombo_ = new QComboBox(this);
    securityCombo_->setEditable(true);

    alterIdLabel_ = new QLabel(QStringLiteral("Alter ID"), this);
    alterIdSpin_ = new QSpinBox(this);
    alterIdSpin_->setRange(0, 65535);

    flowLabel_ = new QLabel(QStringLiteral("Flow"), this);
    flowEdit_ = new QLineEdit(this);

    networkCombo_ = new QComboBox(this);
    networkCombo_->setEditable(true);
    networkCombo_->addItems({
        QStringLiteral("tcp"),
        QStringLiteral("kcp"),
        QStringLiteral("quic"),
        QStringLiteral("ws"),
        QStringLiteral("httpupgrade"),
        QStringLiteral("xhttp"),
        QStringLiteral("grpc"),
        QStringLiteral("h2")
    });

    headerTypeFieldLabel_ = new QLabel(QStringLiteral("Header Type"), this);
    headerTypeCombo_ = new QComboBox(this);
    headerTypeCombo_->setEditable(true);
    headerTypeCombo_->addItems({
        QStringLiteral("none"),
        QStringLiteral("http"),
        QStringLiteral("srtp"),
        QStringLiteral("utp"),
        QStringLiteral("wechat-video"),
        QStringLiteral("dtls"),
        QStringLiteral("wireguard"),
        QStringLiteral("gun"),
        QStringLiteral("multi")
    });

    streamSecurityCombo_ = new QComboBox(this);
    streamSecurityCombo_->setEditable(true);
    streamSecurityCombo_->addItems({
        QString(),
        QStringLiteral("tls"),
        QStringLiteral("xtls"),
        QStringLiteral("reality")
    });

    hostFieldLabel_ = new QLabel(QStringLiteral("Host"), this);
    hostEdit_ = new QLineEdit(this);
    hostEdit_->setPlaceholderText(QStringLiteral("host.example.com"));

    pathFieldLabel_ = new QLabel(QStringLiteral("Path"), this);
    pathEdit_ = new QLineEdit(this);
    pathEdit_->setPlaceholderText(QStringLiteral("/"));

    sniEdit_ = new QLineEdit(this);
    sniEdit_->setPlaceholderText(QStringLiteral("server name indication"));

    fingerprintLabel_ = new QLabel(QStringLiteral("Fingerprint"), this);
    fingerprintEdit_ = new QLineEdit(this);
    fingerprintEdit_->setPlaceholderText(QStringLiteral("chrome"));

    certLabel_ = new QLabel(QStringLiteral("TLS Certificate"), this);
    certEdit_ = new QPlainTextEdit(this);
    certEdit_->setPlaceholderText(QStringLiteral("-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----"));
    certEdit_->setTabChangesFocus(true);
    certEdit_->setMaximumHeight(110);

    certShaLabel_ = new QLabel(QStringLiteral("Pinned Cert SHA256"), this);
    certShaEdit_ = new QLineEdit(this);
    certShaEdit_->setPlaceholderText(QStringLiteral("sha256/base64-one,sha256/base64-two"));

    echConfigListLabel_ = new QLabel(QStringLiteral("ECH Config"), this);
    echConfigListEdit_ = new QLineEdit(this);
    echConfigListEdit_->setPlaceholderText(QStringLiteral("ECHCONFIGBASE64"));

    echForceQueryLabel_ = new QLabel(QStringLiteral("ECH Force Query"), this);
    echForceQueryCombo_ = new QComboBox(this);
    echForceQueryCombo_->setEditable(true);
    echForceQueryCombo_->addItems({
        QString(),
        QStringLiteral("none"),
        QStringLiteral("half"),
        QStringLiteral("full")
    });

    publicKeyLabel_ = new QLabel(QStringLiteral("Public Key"), this);
    publicKeyEdit_ = new QLineEdit(this);
    publicKeyEdit_->setPlaceholderText(QStringLiteral("Reality public key"));

    shortIdLabel_ = new QLabel(QStringLiteral("Short ID"), this);
    shortIdEdit_ = new QLineEdit(this);
    shortIdEdit_->setPlaceholderText(QStringLiteral("0123456789abcdef"));

    spiderXLabel_ = new QLabel(QStringLiteral("Spider X"), this);
    spiderXEdit_ = new QLineEdit(this);
    spiderXEdit_->setPlaceholderText(QStringLiteral("/"));

    mldsa65VerifyLabel_ = new QLabel(QStringLiteral("MLDSA65 Verify"), this);
    mldsa65VerifyEdit_ = new QLineEdit(this);
    mldsa65VerifyEdit_->setPlaceholderText(QStringLiteral("mldsa65 verify key"));

    alpnLabel_ = new QLabel(QStringLiteral("ALPN"), this);
    alpnEdit_ = new QLineEdit(this);
    alpnEdit_->setPlaceholderText(QStringLiteral("h2,http/1.1"));

    userAgentLabel_ = new QLabel(QStringLiteral("User-Agent"), this);
    userAgentCombo_ = new QComboBox(this);
    userAgentCombo_->setEditable(true);
    userAgentCombo_->addItems({
        QString(),
        QStringLiteral("chrome"),
        QStringLiteral("firefox"),
        QStringLiteral("edge"),
        QStringLiteral("golang")
    });

    extraLabel_ = new QLabel(QStringLiteral("Extra"), this);
    extraEdit_ = new QPlainTextEdit(this);
    extraEdit_->setPlaceholderText(QStringLiteral("{\"scMaxEachPostBytes\":12345}"));
    extraEdit_->setTabChangesFocus(true);
    extraEdit_->setMaximumHeight(100);

    finalmaskLabel_ = new QLabel(QStringLiteral("Finalmask"), this);
    finalmaskEdit_ = new QPlainTextEdit(this);
    finalmaskEdit_->setPlaceholderText(QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}"));
    finalmaskEdit_->setTabChangesFocus(true);
    finalmaskEdit_->setMaximumHeight(100);

    muxOverrideLabel_ = new QLabel(QStringLiteral("Mux"), this);
    muxOverrideCombo_ = new QComboBox(this);
    muxOverrideCombo_->setObjectName(QStringLiteral("muxOverrideCombo"));
    muxOverrideCombo_->addItem(QStringLiteral("Inherit Global"));
    muxOverrideCombo_->addItem(QStringLiteral("Enabled"));
    muxOverrideCombo_->addItem(QStringLiteral("Disabled"));

    allowInsecureLabel_ = new QLabel(QStringLiteral("Allow Insecure"), this);
    allowInsecureCombo_ = new QComboBox(this);
    allowInsecureCombo_->setEditable(true);
    allowInsecureCombo_->addItems({
        QString(),
        QStringLiteral("false"),
        QStringLiteral("true")
    });

    // Hysteria2 fields
    hysteria2Group_ = new QWidget(this);
    obfsPasswordLabel_ = new QLabel(QStringLiteral("Obfs Password"), this);
    obfsPasswordEdit_ = new QLineEdit(this);
    upMbpsLabel_ = new QLabel(QStringLiteral("Upload (Mbps)"), this);
    upMbpsEdit_ = new QLineEdit(this);
    downMbpsLabel_ = new QLabel(QStringLiteral("Download (Mbps)"), this);
    downMbpsEdit_ = new QLineEdit(this);
    {
        auto* hy2Layout = new QFormLayout(hysteria2Group_);
        hy2Layout->setContentsMargins(0, 0, 0, 0);
        hy2Layout->addRow(obfsPasswordLabel_, obfsPasswordEdit_);
        hy2Layout->addRow(upMbpsLabel_, upMbpsEdit_);
        hy2Layout->addRow(downMbpsLabel_, downMbpsEdit_);
    }

    // TUIC fields
    tuicGroup_ = new QWidget(this);
    congestionControlLabel_ = new QLabel(QStringLiteral("Congestion Control"), this);
    congestionControlCombo_ = new QComboBox(this);
    congestionControlCombo_->setEditable(true);
    congestionControlCombo_->addItems({
        QStringLiteral("cubic"),
        QStringLiteral("bbr"),
        QStringLiteral("new_reno")
    });
    udpRelayModeLabel_ = new QLabel(QStringLiteral("UDP Relay Mode"), this);
    udpRelayModeCombo_ = new QComboBox(this);
    udpRelayModeCombo_->setEditable(true);
    udpRelayModeCombo_->addItems({
        QStringLiteral("native"),
        QStringLiteral("quic")
    });
    {
        auto* tuicLayout = new QFormLayout(tuicGroup_);
        tuicLayout->setContentsMargins(0, 0, 0, 0);
        tuicLayout->addRow(congestionControlLabel_, congestionControlCombo_);
        tuicLayout->addRow(udpRelayModeLabel_, udpRelayModeCombo_);
    }

    // WireGuard fields
    wireguardGroup_ = new QWidget(this);
    privateKeyLabel_ = new QLabel(QStringLiteral("Private Key"), this);
    privateKeyEdit_ = new QLineEdit(this);
    peerPublicKeyLabel_ = new QLabel(QStringLiteral("Peer Public Key"), this);
    peerPublicKeyEdit_ = new QLineEdit(this);
    localAddressLabel_ = new QLabel(QStringLiteral("Local Address"), this);
    localAddressEdit_ = new QLineEdit(this);
    localAddressEdit_->setPlaceholderText(QStringLiteral("10.0.0.2/32, fd00::2/128"));
    wireguardMtuLabel_ = new QLabel(QStringLiteral("MTU"), this);
    wireguardMtuSpin_ = new QSpinBox(this);
    wireguardMtuSpin_->setRange(0, 65535);
    wireguardMtuSpin_->setValue(0);
    wireguardMtuSpin_->setSpecialValueText(QStringLiteral("Default"));
    reservedLabel_ = new QLabel(QStringLiteral("Reserved"), this);
    reservedEdit_ = new QLineEdit(this);
    reservedEdit_->setPlaceholderText(QStringLiteral("0,0,0"));
    {
        auto* wgLayout = new QFormLayout(wireguardGroup_);
        wgLayout->setContentsMargins(0, 0, 0, 0);
        wgLayout->addRow(privateKeyLabel_, privateKeyEdit_);
        wgLayout->addRow(peerPublicKeyLabel_, peerPublicKeyEdit_);
        wgLayout->addRow(localAddressLabel_, localAddressEdit_);
        wgLayout->addRow(wireguardMtuLabel_, wireguardMtuSpin_);
        wgLayout->addRow(reservedLabel_, reservedEdit_);
    }

    // Naive fields
    naiveGroup_ = new QWidget(this);
    naivePasswordLabel_ = new QLabel(QStringLiteral("Password"), this);
    naivePasswordEdit_ = new QLineEdit(this);
    naivePasswordEdit_->setEchoMode(QLineEdit::Password);
    naiveQuicCheck_ = new QCheckBox(QStringLiteral("Use QUIC transport (naive+quic)"), this);
    naiveInsecureConcurrencyLabel_ = new QLabel(QStringLiteral("Insecure Concurrency"), this);
    naiveInsecureConcurrencySpin_ = new QSpinBox(this);
    naiveInsecureConcurrencySpin_->setRange(0, 64);
    naiveInsecureConcurrencySpin_->setValue(0);
    naiveInsecureConcurrencySpin_->setSpecialValueText(QStringLiteral("Off"));
    {
        auto* naiveLayout = new QFormLayout(naiveGroup_);
        naiveLayout->setContentsMargins(0, 0, 0, 0);
        naiveLayout->addRow(naivePasswordLabel_, naivePasswordEdit_);
        naiveLayout->addRow(QString(), naiveQuicCheck_);
        naiveLayout->addRow(naiveInsecureConcurrencyLabel_, naiveInsecureConcurrencySpin_);
    }

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* formLayout = new QFormLayout();
    formLayout->addRow(QStringLiteral("Type"), typeCombo_);
    formLayout->addRow(QStringLiteral("Core"), coreCombo_);
    formLayout->addRow(QStringLiteral("Remarks"), remarksEdit_);
    formLayout->addRow(QStringLiteral("Address"), addressEdit_);
    formLayout->addRow(QStringLiteral("Port"), portSpin_);
    formLayout->addRow(credentialLabel_, credentialRowWidget_);
    formLayout->addRow(securityLabel_, securityCombo_);
    formLayout->addRow(alterIdLabel_, alterIdSpin_);
    formLayout->addRow(flowLabel_, flowEdit_);
    formLayout->addRow(QStringLiteral("Network"), networkCombo_);
    formLayout->addRow(headerTypeFieldLabel_, headerTypeCombo_);
    formLayout->addRow(QStringLiteral("Transport Security"), streamSecurityCombo_);
    formLayout->addRow(hostFieldLabel_, hostEdit_);
    formLayout->addRow(pathFieldLabel_, pathEdit_);
    formLayout->addRow(QStringLiteral("SNI"), sniEdit_);
    formLayout->addRow(fingerprintLabel_, fingerprintEdit_);
    formLayout->addRow(certLabel_, certEdit_);
    formLayout->addRow(certShaLabel_, certShaEdit_);
    formLayout->addRow(echConfigListLabel_, echConfigListEdit_);
    formLayout->addRow(echForceQueryLabel_, echForceQueryCombo_);
    formLayout->addRow(publicKeyLabel_, publicKeyEdit_);
    formLayout->addRow(shortIdLabel_, shortIdEdit_);
    formLayout->addRow(spiderXLabel_, spiderXEdit_);
    formLayout->addRow(mldsa65VerifyLabel_, mldsa65VerifyEdit_);
    formLayout->addRow(alpnLabel_, alpnEdit_);
    formLayout->addRow(userAgentLabel_, userAgentCombo_);
    formLayout->addRow(extraLabel_, extraEdit_);
    formLayout->addRow(finalmaskLabel_, finalmaskEdit_);
    formLayout->addRow(muxOverrideLabel_, muxOverrideCombo_);
    formLayout->addRow(allowInsecureLabel_, allowInsecureCombo_);
    formLayout->addRow(hysteria2Group_);
    formLayout->addRow(tuicGroup_);
    formLayout->addRow(wireguardGroup_);
    formLayout->addRow(naiveGroup_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(formLayout);
    rootLayout->addWidget(buttonBox_);

    connect(typeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        updateFieldState();
    });
    connect(networkCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        updateFieldState();
    });
    connect(streamSecurityCombo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        updateFieldState();
    });
    connect(credentialGenerateButton_, &QPushButton::clicked, this, [this]() {
        credentialEdit_->setText(QUuid::createUuid().toString(QUuid::WithoutBraces));
    });
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    headerTypeCombo_->setCurrentText(QStringLiteral("none"));
    networkCombo_->setCurrentText(QStringLiteral("tcp"));
}
