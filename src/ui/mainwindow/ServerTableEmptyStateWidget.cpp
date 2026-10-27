#include "ui/mainwindow/ServerTableEmptyStateWidget.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

#include "ui/theme/AppTheme.h"

ServerTableEmptyStateWidget::ServerTableEmptyStateWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ServerTableEmptyStateWidget::setFilterActive(bool filterActive)
{
    if (filterActive_ == filterActive) {
        return;
    }

    filterActive_ = filterActive;
    refreshTexts();
}

bool ServerTableEmptyStateWidget::filterActive() const
{
    return filterActive_;
}

void ServerTableEmptyStateWidget::setImportFailure(const QString& reason)
{
    const QString trimmed = reason.trimmed();
    if (importFailure_ == trimmed) {
        return;
    }

    importFailure_ = trimmed;
    refreshTexts();
}

QString ServerTableEmptyStateWidget::importFailure() const
{
    return importFailure_;
}

QPushButton* ServerTableEmptyStateWidget::importButton() const
{
    return importButton_;
}

QPushButton* ServerTableEmptyStateWidget::addButton() const
{
    return addButton_;
}

QPushButton* ServerTableEmptyStateWidget::clipboardButton() const
{
    return clipboardButton_;
}

void ServerTableEmptyStateWidget::setupUi()
{
    setObjectName(QStringLiteral("serverTableEmptyState"));
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("sectionBody", true);

    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName(QStringLiteral("serverEmptyStateTitle"));
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    titleLabel_->setAlignment(Qt::AlignCenter);

    hintLabel_ = new QLabel(this);
    hintLabel_->setObjectName(QStringLiteral("serverEmptyStateHint"));
    hintLabel_->setWordWrap(true);
    hintLabel_->setAlignment(Qt::AlignCenter);
    QPalette hintPalette = hintLabel_->palette();
    hintPalette.setColor(QPalette::WindowText, QColor(AppTheme::mutedTextColor()));
    hintLabel_->setPalette(hintPalette);

    importButton_ = new QPushButton(this);
    importButton_->setObjectName(QStringLiteral("serverEmptyStateImportButton"));
    addButton_ = new QPushButton(this);
    addButton_->setObjectName(QStringLiteral("serverEmptyStateAddButton"));
    clipboardButton_ = new QPushButton(this);
    clipboardButton_->setObjectName(QStringLiteral("serverEmptyStateClipboardButton"));
    AppTheme::applyCompactFont({importButton_, addButton_, clipboardButton_});

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    buttonRow->addStretch(1);
    buttonRow->addWidget(importButton_);
    buttonRow->addWidget(addButton_);
    buttonRow->addWidget(clipboardButton_);
    buttonRow->addStretch(1);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(10);
    rootLayout->addStretch(1);
    rootLayout->addWidget(titleLabel_);
    rootLayout->addWidget(hintLabel_);
    rootLayout->addLayout(buttonRow);
    rootLayout->addStretch(1);

    connect(importButton_, &QPushButton::clicked, this, &ServerTableEmptyStateWidget::importRequested);
    connect(addButton_, &QPushButton::clicked, this, &ServerTableEmptyStateWidget::addRequested);
    connect(clipboardButton_, &QPushButton::clicked, this, &ServerTableEmptyStateWidget::importFromClipboardRequested);

    refreshTexts();
}

void ServerTableEmptyStateWidget::refreshTexts()
{
    if (filterActive_) {
        // A filter that matched nothing is not the same situation as an empty
        // list: the servers exist, they are just hidden. Saying "no servers"
        // here would be wrong and would hide the actual fix (clear the filter).
        titleLabel_->setText(tr("No servers match the filter"));
        hintLabel_->setText(tr("Clear the filter box above to see all servers again."));
    } else if (!importFailure_.isEmpty()) {
        // The last import failed, so this is not a first run. The reason is the
        // whole point: it is what tells the user whether to retry, change the
        // subscription's user agent, or give up on that protocol. Only the first
        // line goes here -- the parser report follows it and would push the panel
        // past the height of the table it is standing in for.
        //
        // The whole reason is still put on the tooltip: the panel has one line for
        // it, and the report behind that line is what tells a user which nodes were
        // dropped and why.
        const QString firstLine = importFailure_.section(QChar('\n'), 0, 0);
        titleLabel_->setText(tr("Import failed"));
        hintLabel_->setText(
            tr("%1\nFix the reason above, then import again. The log panel below has the full report.")
                .arg(firstLine));
        titleLabel_->setToolTip(importFailure_);
        hintLabel_->setToolTip(importFailure_);
    } else {
        titleLabel_->setText(tr("No servers yet"));
        hintLabel_->setText(tr("Import a subscription or share link to get started."));
    }

    if (filterActive_ || importFailure_.isEmpty()) {
        // Both labels are rewritten wholesale above, so a tooltip left over from
        // an earlier failure would describe text that is no longer there.
        titleLabel_->setToolTip(QString());
        hintLabel_->setToolTip(QString());
    }

    importButton_->setText(tr("Import from File or URL"));
    addButton_->setText(tr("Add Server"));
    clipboardButton_->setText(tr("From Clipboard"));
}
