#include "ui/dialogs/AddServerDialog.h"

#include "common/DialogUtils.h"
#include "ui/dialogs/AddServerDialogOptions.h"

#include <QCheckBox>
#include <QComboBox>
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

void AddServerDialog::setupUi()
{
    setWindowTitle(tr("Add Server"));
    resize(460, 560);

    typeCombo_ = new QComboBox(this);
    typeCombo_->setObjectName(QStringLiteral("typeCombo"));
    for (const AddServerDialogOptions::ComboOption& option : AddServerDialogOptions::protocolTypeOptions()) {
        typeCombo_->addItem(option.label, option.value);
    }

    coreCombo_ = new QComboBox(this);
    coreCombo_->setObjectName(QStringLiteral("coreCombo"));

    remarksEdit_ = new QLineEdit(this);
    addressEdit_ = new QLineEdit(this);
    addressEdit_->setPlaceholderText(tr("example.com"));

    portSpin_ = new QSpinBox(this);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(443);

    credentialLabel_ = new QLabel(this);
    credentialRowWidget_ = new QWidget(this);
    credentialEdit_ = new QLineEdit(this);
    credentialGenerateButton_ = new QPushButton(tr("Generate UUID"), this);

    auto* credentialLayout = new QHBoxLayout(credentialRowWidget_);
    credentialLayout->setContentsMargins(0, 0, 0, 0);
    credentialLayout->addWidget(credentialEdit_, 1);
    credentialLayout->addWidget(credentialGenerateButton_);

    securityLabel_ = new QLabel(this);
    securityCombo_ = new QComboBox(this);
    securityCombo_->setEditable(true);

    alterIdLabel_ = new QLabel(tr("Alter ID"), this);
    alterIdSpin_ = new QSpinBox(this);
    alterIdSpin_->setRange(0, 65535);

    flowLabel_ = new QLabel(tr("Flow"), this);
    flowEdit_ = new QLineEdit(this);

    networkCombo_ = new QComboBox(this);
    networkCombo_->setEditable(true);
    networkCombo_->addItems(AddServerDialogOptions::networkOptions());

    headerTypeFieldLabel_ = new QLabel(tr("Header Type"), this);
    headerTypeCombo_ = new QComboBox(this);
    headerTypeCombo_->setEditable(true);
    headerTypeCombo_->addItems(AddServerDialogOptions::headerTypeOptions());

    streamSecurityCombo_ = new QComboBox(this);
    streamSecurityCombo_->setEditable(true);
    streamSecurityCombo_->addItems(AddServerDialogOptions::streamSecurityOptions());

    hostFieldLabel_ = new QLabel(tr("Host"), this);
    hostEdit_ = new QLineEdit(this);
    hostEdit_->setPlaceholderText(tr("host.example.com"));

    pathFieldLabel_ = new QLabel(tr("Path"), this);
    pathEdit_ = new QLineEdit(this);
    pathEdit_->setPlaceholderText(tr("/"));

    sniEdit_ = new QLineEdit(this);
    sniEdit_->setPlaceholderText(tr("server name indication"));

    fingerprintLabel_ = new QLabel(tr("Fingerprint"), this);
    fingerprintEdit_ = new QLineEdit(this);
    fingerprintEdit_->setPlaceholderText(QStringLiteral("chrome"));

    certLabel_ = new QLabel(tr("TLS Certificate"), this);
    certEdit_ = new QPlainTextEdit(this);
    certEdit_->setPlaceholderText(QStringLiteral("-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----"));
    certEdit_->setTabChangesFocus(true);
    certEdit_->setMaximumHeight(110);

    certShaLabel_ = new QLabel(tr("Pinned Cert SHA256"), this);
    certShaEdit_ = new QLineEdit(this);
    certShaEdit_->setPlaceholderText(QStringLiteral("sha256/base64-one,sha256/base64-two"));

    echConfigListLabel_ = new QLabel(tr("ECH Config"), this);
    echConfigListEdit_ = new QLineEdit(this);
    echConfigListEdit_->setPlaceholderText(QStringLiteral("ECHCONFIGBASE64"));

    echForceQueryLabel_ = new QLabel(tr("ECH Force Query"), this);
    echForceQueryCombo_ = new QComboBox(this);
    echForceQueryCombo_->setEditable(true);
    echForceQueryCombo_->addItems(AddServerDialogOptions::echForceQueryOptions());

    publicKeyLabel_ = new QLabel(tr("Public Key"), this);
    publicKeyEdit_ = new QLineEdit(this);
    publicKeyEdit_->setPlaceholderText(tr("Reality public key"));

    shortIdLabel_ = new QLabel(tr("Short ID"), this);
    shortIdEdit_ = new QLineEdit(this);
    shortIdEdit_->setPlaceholderText(QStringLiteral("0123456789abcdef"));

    spiderXLabel_ = new QLabel(tr("Spider X"), this);
    spiderXEdit_ = new QLineEdit(this);
    spiderXEdit_->setPlaceholderText(QStringLiteral("/"));

    mldsa65VerifyLabel_ = new QLabel(tr("MLDSA65 Verify"), this);
    mldsa65VerifyEdit_ = new QLineEdit(this);
    mldsa65VerifyEdit_->setPlaceholderText(tr("mldsa65 verify key"));

    alpnLabel_ = new QLabel(tr("ALPN"), this);
    alpnEdit_ = new QLineEdit(this);
    alpnEdit_->setPlaceholderText(QStringLiteral("h2,http/1.1"));

    userAgentLabel_ = new QLabel(tr("User-Agent"), this);
    userAgentCombo_ = new QComboBox(this);
    userAgentCombo_->setEditable(true);
    userAgentCombo_->addItems(AddServerDialogOptions::userAgentOptions());

    extraLabel_ = new QLabel(tr("Extra"), this);
    extraEdit_ = new QPlainTextEdit(this);
    extraEdit_->setPlaceholderText(QStringLiteral("{\"scMaxEachPostBytes\":12345}"));
    extraEdit_->setTabChangesFocus(true);
    extraEdit_->setMaximumHeight(100);

    finalmaskLabel_ = new QLabel(tr("Finalmask"), this);
    finalmaskEdit_ = new QPlainTextEdit(this);
    finalmaskEdit_->setPlaceholderText(QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}"));
    finalmaskEdit_->setTabChangesFocus(true);
    finalmaskEdit_->setMaximumHeight(100);

    muxOverrideLabel_ = new QLabel(tr("Mux"), this);
    muxOverrideCombo_ = new QComboBox(this);
    muxOverrideCombo_->setObjectName(QStringLiteral("muxOverrideCombo"));
    muxOverrideCombo_->addItem(tr("Inherit Global"));
    muxOverrideCombo_->addItem(tr("Enabled"));
    muxOverrideCombo_->addItem(tr("Disabled"));

    allowInsecureLabel_ = new QLabel(tr("Allow Insecure"), this);
    allowInsecureCombo_ = new QComboBox(this);
    allowInsecureCombo_->setEditable(true);
    allowInsecureCombo_->addItems(AddServerDialogOptions::allowInsecureOptions());

    hysteria2Group_ = new QWidget(this);
    obfsPasswordLabel_ = new QLabel(tr("Obfs Password"), this);
    obfsPasswordEdit_ = new QLineEdit(this);
    upMbpsLabel_ = new QLabel(tr("Upload (Mbps)"), this);
    upMbpsEdit_ = new QLineEdit(this);
    downMbpsLabel_ = new QLabel(tr("Download (Mbps)"), this);
    downMbpsEdit_ = new QLineEdit(this);
    auto* hy2Layout = new QFormLayout(hysteria2Group_);
    hy2Layout->setContentsMargins(0, 0, 0, 0);
    hy2Layout->addRow(obfsPasswordLabel_, obfsPasswordEdit_);
    hy2Layout->addRow(upMbpsLabel_, upMbpsEdit_);
    hy2Layout->addRow(downMbpsLabel_, downMbpsEdit_);

    tuicGroup_ = new QWidget(this);
    congestionControlLabel_ = new QLabel(tr("Congestion Control"), this);
    congestionControlCombo_ = new QComboBox(this);
    congestionControlCombo_->setEditable(true);
    congestionControlCombo_->addItems(AddServerDialogOptions::congestionControlOptions());
    udpRelayModeLabel_ = new QLabel(tr("UDP Relay Mode"), this);
    udpRelayModeCombo_ = new QComboBox(this);
    udpRelayModeCombo_->setEditable(true);
    udpRelayModeCombo_->addItems(AddServerDialogOptions::udpRelayModeOptions());
    auto* tuicLayout = new QFormLayout(tuicGroup_);
    tuicLayout->setContentsMargins(0, 0, 0, 0);
    tuicLayout->addRow(congestionControlLabel_, congestionControlCombo_);
    tuicLayout->addRow(udpRelayModeLabel_, udpRelayModeCombo_);

    wireguardGroup_ = new QWidget(this);
    privateKeyLabel_ = new QLabel(tr("Private Key"), this);
    privateKeyEdit_ = new QLineEdit(this);
    peerPublicKeyLabel_ = new QLabel(tr("Peer Public Key"), this);
    peerPublicKeyEdit_ = new QLineEdit(this);
    localAddressLabel_ = new QLabel(tr("Local Address"), this);
    localAddressEdit_ = new QLineEdit(this);
    localAddressEdit_->setPlaceholderText(QStringLiteral("10.0.0.2/32, fd00::2/128"));
    wireguardMtuLabel_ = new QLabel(tr("MTU"), this);
    wireguardMtuSpin_ = new QSpinBox(this);
    wireguardMtuSpin_->setRange(0, 65535);
    wireguardMtuSpin_->setValue(0);
    wireguardMtuSpin_->setSpecialValueText(tr("Default"));
    reservedLabel_ = new QLabel(tr("Reserved"), this);
    reservedEdit_ = new QLineEdit(this);
    reservedEdit_->setPlaceholderText(QStringLiteral("0,0,0"));
    auto* wgLayout = new QFormLayout(wireguardGroup_);
    wgLayout->setContentsMargins(0, 0, 0, 0);
    wgLayout->addRow(privateKeyLabel_, privateKeyEdit_);
    wgLayout->addRow(peerPublicKeyLabel_, peerPublicKeyEdit_);
    wgLayout->addRow(localAddressLabel_, localAddressEdit_);
    wgLayout->addRow(wireguardMtuLabel_, wireguardMtuSpin_);
    wgLayout->addRow(reservedLabel_, reservedEdit_);

    naiveGroup_ = new QWidget(this);
    naivePasswordLabel_ = new QLabel(tr("Password"), this);
    naivePasswordEdit_ = new QLineEdit(this);
    naivePasswordEdit_->setEchoMode(QLineEdit::Password);
    naiveQuicCheck_ = new QCheckBox(tr("Use QUIC transport (naive+quic)"), this);
    naiveInsecureConcurrencyLabel_ = new QLabel(tr("Insecure Concurrency"), this);
    naiveInsecureConcurrencySpin_ = new QSpinBox(this);
    naiveInsecureConcurrencySpin_->setRange(0, 64);
    naiveInsecureConcurrencySpin_->setValue(0);
    naiveInsecureConcurrencySpin_->setSpecialValueText(tr("Off"));
    auto* naiveLayout = new QFormLayout(naiveGroup_);
    naiveLayout->setContentsMargins(0, 0, 0, 0);
    naiveLayout->addRow(naivePasswordLabel_, naivePasswordEdit_);
    naiveLayout->addRow(QString(), naiveQuicCheck_);
    naiveLayout->addRow(naiveInsecureConcurrencyLabel_, naiveInsecureConcurrencySpin_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    DialogUtils::localizeStandardDialogButtonBox(buttonBox_);

    auto* formLayout = new QFormLayout();
    formLayout->addRow(tr("Type"), typeCombo_);
    formLayout->addRow(tr("Core"), coreCombo_);
    formLayout->addRow(tr("Remarks"), remarksEdit_);
    formLayout->addRow(tr("Address"), addressEdit_);
    formLayout->addRow(tr("Port"), portSpin_);
    formLayout->addRow(credentialLabel_, credentialRowWidget_);
    formLayout->addRow(securityLabel_, securityCombo_);
    formLayout->addRow(alterIdLabel_, alterIdSpin_);
    formLayout->addRow(flowLabel_, flowEdit_);
    formLayout->addRow(tr("Network"), networkCombo_);
    formLayout->addRow(headerTypeFieldLabel_, headerTypeCombo_);
    formLayout->addRow(tr("Transport Security"), streamSecurityCombo_);
    formLayout->addRow(hostFieldLabel_, hostEdit_);
    formLayout->addRow(pathFieldLabel_, pathEdit_);
    formLayout->addRow(tr("SNI"), sniEdit_);
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
        applyDefaultCoreTypeForCurrentProtocol();
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
    applyDefaultCoreTypeForCurrentProtocol();
}
