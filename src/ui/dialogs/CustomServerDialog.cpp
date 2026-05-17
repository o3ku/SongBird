#include "ui/dialogs/CustomServerDialog.h"

#include "common/DialogUtils.h"

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
    setWindowTitle(tr("Custom Server"));
    resize(560, 220);

    remarksEdit_ = new QLineEdit(this);

    filePathEdit_ = new QLineEdit(this);
    filePathEdit_->setPlaceholderText(tr("Select a custom config file"));
    browseButton_ = new QPushButton(tr("Browse"), this);

    auto* fileRowWidget = new QWidget(this);
    auto* fileRowLayout = new QHBoxLayout(fileRowWidget);
    fileRowLayout->setContentsMargins(0, 0, 0, 0);
    fileRowLayout->addWidget(filePathEdit_, 1);
    fileRowLayout->addWidget(browseButton_);

    coreCombo_ = new QComboBox(this);
    coreCombo_->addItem(QStringLiteral("Xray"), static_cast<int>(CoreType::Xray));
    coreCombo_->addItem(QStringLiteral("sing-box"), static_cast<int>(CoreType::SingBox));

    preSocksPortSpin_ = new QSpinBox(this);
    preSocksPortSpin_->setRange(0, 65535);
    preSocksPortSpin_->setSpecialValueText(tr("Disabled"));

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    DialogUtils::localizeStandardDialogButtonBox(buttonBox_);

    auto* formLayout = new QFormLayout();
    formLayout->addRow(tr("Remarks"), remarksEdit_);
    formLayout->addRow(tr("Config File"), fileRowWidget);
    formLayout->addRow(tr("Core"), coreCombo_);
    formLayout->addRow(tr("Pre-Socks Port"), preSocksPortSpin_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(formLayout);
    rootLayout->addWidget(buttonBox_);

    connect(browseButton_, &QPushButton::clicked, this, [this]() {
        const QString filePath = DialogUtils::getOpenFileName(
            this,
            tr("Select Custom Config"),
            filePathEdit_->text().trimmed(),
            QStringLiteral("Config Files (*.json);;All Files (*.*)"));
        if (!filePath.trimmed().isEmpty()) {
            filePathEdit_->setText(filePath);
        }
    });
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
