#pragma once

#include <QDialog>

#include "domain/models/VmessItem.h"

class QComboBox;
class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QWidget;

class AddServerDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AddServerDialog(QWidget* parent = nullptr);

    void setServer(const VmessItem& server);
    VmessItem server() const;

private:
    void updateFieldState();
    void refillSecurityOptions(ConfigType type);
    void setCoreType(CoreType type);
    void applyDefaultCoreTypeForCurrentProtocol();

    void setupUi();

    QComboBox* typeCombo_ = nullptr;
    QComboBox* coreCombo_ = nullptr;
    QLineEdit* remarksEdit_ = nullptr;
    QLineEdit* addressEdit_ = nullptr;
    QSpinBox* portSpin_ = nullptr;
    QLabel* credentialLabel_ = nullptr;
    QWidget* credentialRowWidget_ = nullptr;
    QLineEdit* credentialEdit_ = nullptr;
    QPushButton* credentialGenerateButton_ = nullptr;
    QLabel* securityLabel_ = nullptr;
    QComboBox* securityCombo_ = nullptr;
    QLabel* alterIdLabel_ = nullptr;
    QSpinBox* alterIdSpin_ = nullptr;
    QLabel* flowLabel_ = nullptr;
    QLineEdit* flowEdit_ = nullptr;
    QComboBox* networkCombo_ = nullptr;
    QLabel* headerTypeFieldLabel_ = nullptr;
    QComboBox* headerTypeCombo_ = nullptr;
    QComboBox* streamSecurityCombo_ = nullptr;
    QLabel* hostFieldLabel_ = nullptr;
    QLineEdit* hostEdit_ = nullptr;
    QLabel* pathFieldLabel_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QLineEdit* sniEdit_ = nullptr;
    QLabel* fingerprintLabel_ = nullptr;
    QLineEdit* fingerprintEdit_ = nullptr;
    QLabel* certLabel_ = nullptr;
    QPlainTextEdit* certEdit_ = nullptr;
    QLabel* certShaLabel_ = nullptr;
    QLineEdit* certShaEdit_ = nullptr;
    QLabel* echConfigListLabel_ = nullptr;
    QLineEdit* echConfigListEdit_ = nullptr;
    QLabel* echForceQueryLabel_ = nullptr;
    QComboBox* echForceQueryCombo_ = nullptr;
    QLabel* publicKeyLabel_ = nullptr;
    QLineEdit* publicKeyEdit_ = nullptr;
    QLabel* shortIdLabel_ = nullptr;
    QLineEdit* shortIdEdit_ = nullptr;
    QLabel* spiderXLabel_ = nullptr;
    QLineEdit* spiderXEdit_ = nullptr;
    QLabel* mldsa65VerifyLabel_ = nullptr;
    QLineEdit* mldsa65VerifyEdit_ = nullptr;
    QLabel* alpnLabel_ = nullptr;
    QLineEdit* alpnEdit_ = nullptr;
    QLabel* userAgentLabel_ = nullptr;
    QComboBox* userAgentCombo_ = nullptr;
    QLabel* extraLabel_ = nullptr;
    QPlainTextEdit* extraEdit_ = nullptr;
    QLabel* finalmaskLabel_ = nullptr;
    QPlainTextEdit* finalmaskEdit_ = nullptr;
    QLabel* muxOverrideLabel_ = nullptr;
    QComboBox* muxOverrideCombo_ = nullptr;
    QLabel* allowInsecureLabel_ = nullptr;
    QComboBox* allowInsecureCombo_ = nullptr;
    // Hysteria2 fields
    QWidget* hysteria2Group_ = nullptr;
    QLabel* obfsPasswordLabel_ = nullptr;
    QLineEdit* obfsPasswordEdit_ = nullptr;
    QLabel* upMbpsLabel_ = nullptr;
    QLineEdit* upMbpsEdit_ = nullptr;
    QLabel* downMbpsLabel_ = nullptr;
    QLineEdit* downMbpsEdit_ = nullptr;
    // TUIC fields
    QWidget* tuicGroup_ = nullptr;
    QLabel* congestionControlLabel_ = nullptr;
    QComboBox* congestionControlCombo_ = nullptr;
    QLabel* udpRelayModeLabel_ = nullptr;
    QComboBox* udpRelayModeCombo_ = nullptr;
    // WireGuard fields
    QWidget* wireguardGroup_ = nullptr;
    QLabel* privateKeyLabel_ = nullptr;
    QLineEdit* privateKeyEdit_ = nullptr;
    QLabel* peerPublicKeyLabel_ = nullptr;
    QLineEdit* peerPublicKeyEdit_ = nullptr;
    QLabel* localAddressLabel_ = nullptr;
    QLineEdit* localAddressEdit_ = nullptr;
    QLabel* wireguardMtuLabel_ = nullptr;
    QSpinBox* wireguardMtuSpin_ = nullptr;
    QLabel* reservedLabel_ = nullptr;
    QLineEdit* reservedEdit_ = nullptr;
    // Naive fields
    QWidget* naiveGroup_ = nullptr;
    QLabel* naivePasswordLabel_ = nullptr;
    QLineEdit* naivePasswordEdit_ = nullptr;
    QCheckBox* naiveQuicCheck_ = nullptr;
    QLabel* naiveInsecureConcurrencyLabel_ = nullptr;
    QSpinBox* naiveInsecureConcurrencySpin_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};
