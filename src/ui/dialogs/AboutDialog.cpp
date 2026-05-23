#include "ui/dialogs/AboutDialog.h"

#include "common/DialogUtils.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QSvgWidget>
#include <QVBoxLayout>

#include "ui/theme/AppTheme.h"

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
            const QString linkColor = AppTheme::linkColor();
            repoUrlValueLabel_->setText(
                QStringLiteral("<a style=\"color:%1;\" href=\"%2\">%2</a>").arg(linkColor, repoUrl.trimmed()));
        }
    }
}

void AboutDialog::setReleaseDate(const QString& releaseDate)
{
    releaseDate_ = releaseDate.trimmed();
    refreshSubtitle();
}

void AboutDialog::setVersion(const QString& version)
{
    version_ = version.trimmed();
    refreshSubtitle();
}

void AboutDialog::setConfigPath(const QString& configPath)
{
    if (configPathValueLabel_ != nullptr) {
        configPathValueLabel_->setText(configPath.trimmed().isEmpty() ? tr("Unavailable") : configPath.trimmed());
    }
}

void AboutDialog::setProxyActive(bool active)
{
    if (logoLabel_ != nullptr) {
        logoLabel_->load(active
            ? QStringLiteral(":/app/logo-fire.svg")
            : QStringLiteral(":/app/logo.svg"));
    }
}

void AboutDialog::refreshSubtitle()
{
    if (subtitleLabel_ == nullptr) {
        return;
    }

    const QString version = version_.isEmpty() ? tr("Unknown version") : tr("Version %1").arg(version_);
    const QString release = releaseDate_.isEmpty() ? QString() : tr("Released %1").arg(releaseDate_);
    subtitleLabel_->setText(release.isEmpty()
        ? version
        : QStringLiteral("%1  %2  %3").arg(version, QChar(0x00B7), release));
}

void AboutDialog::setupUi()
{
    setWindowTitle(tr("About SongBird"));
    resize(520, 280);

    logoLabel_ = new QSvgWidget(QStringLiteral(":/app/logo.svg"), this);
    logoLabel_->setObjectName(QStringLiteral("aboutLogo"));
    logoLabel_->setFixedSize(72, 72);

    titleLabel_ = new QLabel(tr("SongBird"), this);
    titleLabel_->setObjectName(QStringLiteral("aboutTitleLabel"));
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 6.0);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);

    subtitleLabel_ = new QLabel(this);
    subtitleLabel_->setObjectName(QStringLiteral("aboutSubtitleLabel"));
    QPalette subtitlePalette = subtitleLabel_->palette();
    subtitlePalette.setColor(QPalette::WindowText, QColor(AppTheme::mutedTextColor()));
    subtitleLabel_->setPalette(subtitlePalette);
    subtitleLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    refreshSubtitle();

    auto* heroText = new QVBoxLayout();
    heroText->setContentsMargins(0, 0, 0, 0);
    heroText->setSpacing(4);
    heroText->addWidget(titleLabel_);
    heroText->addWidget(subtitleLabel_);

    auto* summaryLabel = new QLabel(
        tr("A Qt/C++ rewrite and improvement of v2rayN."),
        this);
    summaryLabel->setObjectName(QStringLiteral("aboutSummaryLabel"));
    summaryLabel->setWordWrap(true);
    heroText->addWidget(summaryLabel);
    heroText->addStretch(1);

    auto* heroRow = new QHBoxLayout();
    heroRow->setContentsMargins(0, 0, 0, 0);
    heroRow->setSpacing(16);
    heroRow->addWidget(logoLabel_, 0, Qt::AlignTop);
    heroRow->addLayout(heroText, 1);

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    repoUrlValueLabel_ = new QLabel(tr("Unavailable"), this);
    repoUrlValueLabel_->setObjectName(QStringLiteral("aboutRepoUrlValueLabel"));
    repoUrlValueLabel_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    repoUrlValueLabel_->setOpenExternalLinks(true);

    configPathValueLabel_ = new QLabel(tr("Unavailable"), this);
    configPathValueLabel_->setObjectName(QStringLiteral("aboutConfigPathValueLabel"));
    configPathValueLabel_->setWordWrap(true);
    configPathValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setHorizontalSpacing(16);
    formLayout->setVerticalSpacing(6);

    const QColor labelColor(AppTheme::mutedTextColor());
    const auto buildFieldLabel = [&](const QString& text) {
        auto* label = new QLabel(text, this);
        QFont font = label->font();
        font.setPointSizeF(qMax(7.0, font.pointSizeF() - 1.0));
        label->setFont(font);
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, labelColor);
        label->setPalette(pal);
        return label;
    };

    formLayout->addRow(buildFieldLabel(tr("Github")), repoUrlValueLabel_);
    formLayout->addRow(buildFieldLabel(tr("Config")), configPathValueLabel_);

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    DialogUtils::localizeStandardDialogButtonBox(buttonBox_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 18, 20, 16);
    rootLayout->setSpacing(12);
    rootLayout->addLayout(heroRow);
    rootLayout->addWidget(separator);
    rootLayout->addLayout(formLayout);
    rootLayout->addStretch(1);
    rootLayout->addWidget(buttonBox_);

    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
