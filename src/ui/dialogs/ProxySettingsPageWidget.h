#pragma once

#include <QWidget>

#include "domain/models/Config.h"

class QComboBox;
class QLineEdit;

class ProxySettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ProxySettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    void applyToConfig(Config& config) const;

private:
    void setupUi();

    QLineEdit* systemProxyExceptionsEdit_ = nullptr;
    QComboBox* systemProxyAdvancedProtocolCombo_ = nullptr;
    QLineEdit* pacUrlEdit_ = nullptr;
};
