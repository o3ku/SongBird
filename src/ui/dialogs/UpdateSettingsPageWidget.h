#pragma once

#include <QWidget>

#include "domain/models/Config.h"

class QCheckBox;

class UpdateSettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit UpdateSettingsPageWidget(QWidget* parent = nullptr);

    void setConfig(const Config& config);
    void applyToConfig(Config& config) const;

private:
    void setupUi();

    QCheckBox* checkPreReleaseUpdateCheck_ = nullptr;
    QCheckBox* ignoreGeoUpdateCoreCheck_ = nullptr;
};
