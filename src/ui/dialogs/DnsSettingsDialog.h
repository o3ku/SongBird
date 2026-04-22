#pragma once

#include <QDialog>

#include "domain/models/Config.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class DnsSettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit DnsSettingsDialog(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    Config config() const;

private:
    void setupUi();
    void updateFieldState();
    void resetToDefaults();

    Config config_;

    QLineEdit* remoteDnsEdit_ = nullptr;
    QLineEdit* directDnsEdit_ = nullptr;
    QLineEdit* bootstrapDnsEdit_ = nullptr;
    QComboBox* domainStrategyProxyCombo_ = nullptr;
    QComboBox* domainStrategyFreedomCombo_ = nullptr;
    QCheckBox* fakeIpCheck_ = nullptr;
    QCheckBox* globalFakeIpCheck_ = nullptr;
    QCheckBox* serveStaleCheck_ = nullptr;
    QCheckBox* parallelQueryCheck_ = nullptr;
    QCheckBox* useSystemHostsCheck_ = nullptr;
    QCheckBox* addCommonHostsCheck_ = nullptr;
    QCheckBox* blockBindingQueryCheck_ = nullptr;
    QLineEdit* directExpectedIpsEdit_ = nullptr;
    QPlainTextEdit* hostsEdit_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
};
