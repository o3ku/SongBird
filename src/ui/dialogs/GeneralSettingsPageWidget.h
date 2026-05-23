#pragma once

#include <QWidget>

#include "domain/models/Config.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

class GeneralSettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit GeneralSettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    void applyToConfig(Config& config) const;

private:
    void setupUi();
    void updateFieldState();

    QCheckBox* showMainOnStartupCheck_ = nullptr;
    QCheckBox* autoRunCheck_ = nullptr;
    QCheckBox* allowLanConnectionCheck_ = nullptr;
    QCheckBox* sniffingEnabledCheck_ = nullptr;
    QCheckBox* routeOnlyCheck_ = nullptr;
    QSpinBox* localPortSpin_ = nullptr;
    QCheckBox* defaultAllowInsecureCheck_ = nullptr;
    QComboBox* defaultFingerprintCombo_ = nullptr;
    QComboBox* languageCombo_ = nullptr;
    QComboBox* themeCombo_ = nullptr;
};
