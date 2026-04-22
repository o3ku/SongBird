#pragma once

#include <QDialog>

class QLabel;
class QDialogButtonBox;

class AboutDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

    void setVersion(const QString& version);
    void setConfigPath(const QString& configPath);

private:
    void setupUi();

    QLabel* versionValueLabel_ = nullptr;
    QLabel* configPathValueLabel_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};
