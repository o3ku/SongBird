#pragma once

#include <QWidget>

#include "domain/models/Config.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

class TunSettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TunSettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    void applyToConfig(Config& config) const;
    bool tunEnabled() const;

signals:
    void tunEnabledChanged(bool enabled);

private:
    void setupUi();
    void updateFieldState();

    QCheckBox* tunEnableCheck_ = nullptr;
    QCheckBox* tunAutoRouteCheck_ = nullptr;
    QCheckBox* tunStrictRouteCheck_ = nullptr;
    QSpinBox* tunMtuSpin_ = nullptr;
    QComboBox* tunStackCombo_ = nullptr;
    QCheckBox* tunEnableIPv6AddressCheck_ = nullptr;
    QComboBox* tunIcmpRoutingCombo_ = nullptr;
    QComboBox* tunUdpRoutingCombo_ = nullptr;
};
