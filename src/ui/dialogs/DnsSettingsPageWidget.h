#pragma once

#include <QWidget>

#include "domain/models/Config.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTextEdit;

class DnsSettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit DnsSettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    void applyToConfig(Config& config) const;

private:
    void setupUi();
    void updateFieldState();

    QLineEdit* remoteDnsEdit_ = nullptr;
    QLineEdit* directDnsEdit_ = nullptr;
    QLineEdit* bootstrapDnsEdit_ = nullptr;
    QComboBox* domainStrategyForFreedomCombo_ = nullptr;
    QComboBox* domainStrategyForProxyCombo_ = nullptr;
    QCheckBox* useSystemHostsCheck_ = nullptr;
    QCheckBox* addCommonHostsCheck_ = nullptr;
    QCheckBox* blockBindingQueryCheck_ = nullptr;
    QCheckBox* fakeIpCheck_ = nullptr;
    QCheckBox* globalFakeIpCheck_ = nullptr;
    QCheckBox* serveStaleCheck_ = nullptr;
    QCheckBox* parallelQueryCheck_ = nullptr;
    QLineEdit* directExpectedIpsEdit_ = nullptr;
    QTextEdit* dnsHostsEdit_ = nullptr;
};
