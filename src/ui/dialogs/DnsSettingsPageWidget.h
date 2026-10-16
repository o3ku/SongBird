#pragma once

#include <QWidget>

#include "domain/models/Config.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTextEdit;
class QWidget;

class DnsSettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit DnsSettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    void applyToConfig(Config& config) const;

signals:
    // FakeIP is a shared setting, but the sing-box-only "Global FakeIP" lives on the
    // Core page, so that page has to know when this one turns FakeIP on or off.
    void fakeIpEnabledChanged(bool enabled);

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
    QCheckBox* fakeIpCheck_ = nullptr;
    QTextEdit* dnsHostsEdit_ = nullptr;
    // Trailing zero-height row that absorbs the form's leftover height, so the pages matches
    // the other tabs instead of spreading that height over the row gaps.
    QWidget* formFiller_ = nullptr;
};
