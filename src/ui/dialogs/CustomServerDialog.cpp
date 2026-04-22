#include "ui/dialogs/CustomServerDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

CustomServerDialog::CustomServerDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void CustomServerDialog::setServer(const VmessItem& server, const QString& resolvedPath)
{
    remarksEdit_->setText(server.remarks);
    filePathEdit_->setText(resolvedPath.trimmed().isEmpty() ? server.address : resolvedPath);
    setCoreType(server.coreType);
    preSocksPortSpin_->setValue(server.preSocksPort);
}

VmessItem CustomServerDialog::server() const
{
    VmessItem item;
    item.configType = ConfigType::Custom;
    item.coreType = static_cast<CoreType>(coreCombo_->currentData().toInt());
    item.remarks = remarksEdit_->text().trimmed();
    item.address = filePathEdit_->text().trimmed();
    item.preSocksPort = preSocksPortSpin_->value();
    return item;
}

void CustomServerDialog::setCoreType(CoreType type)
{
    const int index = coreCombo_->findData(static_cast<int>(type));
    coreCombo_->setCurrentIndex(index >= 0 ? index : 0);
}

void CustomServerDialog::setupUi()
{
    setWindowTitle(QStringLiteral("Custom Server"));
    resize(560, 220);

    remarksEdit_ = new QLineEdit(this);

    filePathEdit_ = new QLineEdit(this);
    filePathEdit_->setPlaceholderText(QStringLiteral("Select a custom config file"));
    browseButton_ = new QPushButton(QStringLiteral("Browse"), this);

    auto* fileRowWidget = new QWidget(this);
    auto* fileRowLayout = new QHBoxLayout(fileRowWidget);
    fileRowLayout->setContentsMargins(0, 0, 0, 0);
    fileRowLayout->addWidget(filePathEdit_, 1);
    fileRowLayout->addWidget(browseButton_);

    coreCombo_ = new QComboBox(this);
    coreCombo_->addItem(QStringLiteral("Auto (Xray)"), static_cast<int>(CoreType::Auto));
    coreCombo_->addItem(QStringLiteral("Xray"), static_cast<int>(CoreType::Xray));
    coreCombo_->addItem(QStringLiteral("V2Ray"), static_cast<int>(CoreType::V2Fly));
    coreCombo_->addItem(QStringLiteral("SagerNet"), static_cast<int>(CoreType::SagerNet));
    coreCombo_->addItem(QStringLiteral("V2Ray v5"), static_cast<int>(CoreType::V2FlyV5));
    coreCombo_->addItem(QStringLiteral("Clash"), static_cast<int>(CoreType::Clash));
    coreCombo_->addItem(QStringLiteral("Clash.Meta"), static_cast<int>(CoreType::ClashMeta));
    coreCombo_->addItem(QStringLiteral("Hysteria"), static_cast<int>(CoreType::Hysteria));
    coreCombo_->addItem(QStringLiteral("NaiveProxy"), static_cast<int>(CoreType::NaiveProxy));
    coreCombo_->addItem(QStringLiteral("sing-box"), static_cast<int>(CoreType::SingBox));

    preSocksPortSpin_ = new QSpinBox(this);
    preSocksPortSpin_->setRange(0, 65535);
    preSocksPortSpin_->setSpecialValueText(QStringLiteral("Disabled"));

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* formLayout = new QFormLayout();
    formLayout->addRow(QStringLiteral("Remarks"), remarksEdit_);
    formLayout->addRow(QStringLiteral("Config File"), fileRowWidget);
    formLayout->addRow(QStringLiteral("Core"), coreCombo_);
    formLayout->addRow(QStringLiteral("Pre-Socks Port"), preSocksPortSpin_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(formLayout);
    rootLayout->addWidget(buttonBox_);

    connect(browseButton_, &QPushButton::clicked, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Select Custom Config"),
            filePathEdit_->text().trimmed(),
            QStringLiteral("Config Files (*.json *.yaml *.yml);;All Files (*.*)"));
        if (!filePath.trimmed().isEmpty()) {
            filePathEdit_->setText(filePath);
        }
    });
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
