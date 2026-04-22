#include "ui/dialogs/AboutDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void AboutDialog::setVersion(const QString& version)
{
    if (versionValueLabel_ != nullptr) {
        versionValueLabel_->setText(version.trimmed().isEmpty() ? tr("Unknown") : version.trimmed());
    }
}

void AboutDialog::setConfigPath(const QString& configPath)
{
    if (configPathValueLabel_ != nullptr) {
        configPathValueLabel_->setText(configPath.trimmed().isEmpty() ? tr("Unavailable") : configPath.trimmed());
    }
}

void AboutDialog::setupUi()
{
    setWindowTitle(tr("About"));
    resize(480, 220);

    auto* titleLabel = new QLabel(tr("v2rayq"), this);
    titleLabel->setObjectName(QStringLiteral("aboutTitleLabel"));
    titleLabel->setStyleSheet(QStringLiteral("QLabel { font-size: 22px; font-weight: 700; }"));

    auto* summaryLabel = new QLabel(
        tr("A pure Qt/C++ prototype focused on legacy-compatible v2rayN workflows."),
        this);
    summaryLabel->setObjectName(QStringLiteral("aboutSummaryLabel"));
    summaryLabel->setWordWrap(true);

    versionValueLabel_ = new QLabel(tr("Unknown"), this);
    versionValueLabel_->setObjectName(QStringLiteral("aboutVersionValueLabel"));
    versionValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    configPathValueLabel_ = new QLabel(tr("Unavailable"), this);
    configPathValueLabel_->setObjectName(QStringLiteral("aboutConfigPathValueLabel"));
    configPathValueLabel_->setWordWrap(true);
    configPathValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* formLayout = new QFormLayout();
    formLayout->addRow(tr("Version"), versionValueLabel_);
    formLayout->addRow(tr("Config Path"), configPathValueLabel_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok, this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(summaryLabel);
    rootLayout->addLayout(formLayout);
    rootLayout->addStretch(1);
    rootLayout->addWidget(buttonBox_);

    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
