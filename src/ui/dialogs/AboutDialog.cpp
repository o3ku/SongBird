#include "ui/dialogs/AboutDialog.h"

#include "common/DialogUtils.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QSvgWidget>
#include <QVBoxLayout>

namespace {

QString readableLinkColor(const QWidget* widget)
{
    const QColor windowColor = widget == nullptr
        ? QColor(Qt::white)
        : widget->palette().color(QPalette::Window);
    return windowColor.lightness() < 128
        ? QStringLiteral("#7ed8ff")
        : QStringLiteral("#0969da");
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void AboutDialog::setRepoUrl(const QString& repoUrl)
{
    if (repoUrlValueLabel_ != nullptr) {
        if (repoUrl.trimmed().isEmpty()) {
            repoUrlValueLabel_->setText(tr("Unavailable"));
        } else {
            const QString linkColor = readableLinkColor(repoUrlValueLabel_);
            repoUrlValueLabel_->setText(
                QStringLiteral("<a style=\"color:%1;\" href=\"%2\">%2</a>").arg(linkColor, repoUrl.trimmed()));
        }
    }
}

void AboutDialog::setReleaseDate(const QString& releaseDate)
{
    if (releaseDateValueLabel_ != nullptr) {
        releaseDateValueLabel_->setText(releaseDate.trimmed().isEmpty() ? tr("Unknown") : releaseDate.trimmed());
    }
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
    setWindowTitle(tr("About SongBird"));
    resize(480, 220);

    logoLabel_ = new QSvgWidget(QStringLiteral(":/app/logo.svg"), this);
    logoLabel_->setObjectName(QStringLiteral("aboutLogo"));
    logoLabel_->setFixedSize(24, 24);

    auto* titleLabel = new QLabel(tr("SongBird"), this);
    titleLabel->setObjectName(QStringLiteral("aboutTitleLabel"));

    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    titleRow->addWidget(logoLabel_);
    titleRow->addWidget(titleLabel);
    titleRow->addStretch(1);

    auto* summaryLabel = new QLabel(
        tr("SongBird is a Qt/C++ rewrite and improvement of v2rayN."),
        this);
    summaryLabel->setObjectName(QStringLiteral("aboutSummaryLabel"));
    summaryLabel->setWordWrap(true);

    repoUrlValueLabel_ = new QLabel(tr("Unavailable"), this);
    repoUrlValueLabel_->setObjectName(QStringLiteral("aboutRepoUrlValueLabel"));
    repoUrlValueLabel_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    repoUrlValueLabel_->setOpenExternalLinks(true);

    versionValueLabel_ = new QLabel(tr("Unknown"), this);
    versionValueLabel_->setObjectName(QStringLiteral("aboutVersionValueLabel"));
    versionValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    releaseDateValueLabel_ = new QLabel(tr("Unknown"), this);
    releaseDateValueLabel_->setObjectName(QStringLiteral("aboutReleaseDateValueLabel"));
    releaseDateValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    configPathValueLabel_ = new QLabel(tr("Unavailable"), this);
    configPathValueLabel_->setObjectName(QStringLiteral("aboutConfigPathValueLabel"));
    configPathValueLabel_->setWordWrap(true);
    configPathValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* formLayout = new QFormLayout();
    formLayout->addRow(tr("Github"), repoUrlValueLabel_);
    formLayout->addRow(tr("Version"), versionValueLabel_);
    formLayout->addRow(tr("Release Date"), releaseDateValueLabel_);
    formLayout->addRow(tr("Config Path"), configPathValueLabel_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    DialogUtils::localizeStandardDialogButtonBox(buttonBox_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(titleRow);
    rootLayout->addWidget(summaryLabel);
    rootLayout->addLayout(formLayout);
    rootLayout->addStretch(1);
    rootLayout->addWidget(buttonBox_);

    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
