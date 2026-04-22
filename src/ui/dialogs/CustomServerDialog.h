#pragma once

#include <QDialog>

#include "domain/models/VmessItem.h"

class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

class CustomServerDialog final : public QDialog {
    Q_OBJECT

public:
    explicit CustomServerDialog(QWidget* parent = nullptr);

    void setServer(const VmessItem& server, const QString& resolvedPath);
    VmessItem server() const;

private:
    void setCoreType(CoreType type);
    void setupUi();

    QLineEdit* remarksEdit_ = nullptr;
    QLineEdit* filePathEdit_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    QComboBox* coreCombo_ = nullptr;
    QSpinBox* preSocksPortSpin_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};
