#include "ui/mainwindow/ServerTableEmptyStateWidget.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

#include "ui/mainwindow/SubscriptionViewSupport.h"
#include "ui/theme/AppTheme.h"

ServerTableEmptyStateWidget::ServerTableEmptyStateWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ServerTableEmptyStateWidget::setState(
    ServerEmptyStatePolicy::State state,
    const QString& withheldSubscriptionId,
    bool textFilterActive)
{
    if (state_ == state && withheldSubscriptionId_ == withheldSubscriptionId
        && textFilterActive_ == textFilterActive) {
        return;
    }

    state_ = state;
    withheldSubscriptionId_ = withheldSubscriptionId;
    textFilterActive_ = textFilterActive;
    refreshTexts();
}

ServerEmptyStatePolicy::State ServerTableEmptyStateWidget::state() const
{
    return state_;
}

bool ServerTableEmptyStateWidget::filterActive() const
{
    return state_ == ServerEmptyStatePolicy::State::NodesElsewhere;
}

void ServerTableEmptyStateWidget::setCanStartBackgroundTask(std::function<bool()> predicate)
{
    canStartBackgroundTask_ = std::move(predicate);
    refreshActionEnablement();
}

void ServerTableEmptyStateWidget::refreshActionEnablement()
{
    refreshButton_->setEnabled(canStartBackgroundTask());
}

void ServerTableEmptyStateWidget::setSubscriptions(
    const QList<SubItem>& subscriptions,
    const QString& listedSubscriptionId)
{
    const QString trimmedListedId = listedSubscriptionId.trimmed();
    // The owner pushes this on every refresh, including one per keystroke in the filter
    // box. Rebuilding every label through tr() for an unchanged context is work nobody
    // sees, and it also re-arms nothing: the node counts reach the panel through the
    // proxy's own row signals, not through this call.
    if (listedSubscriptionId_ == trimmedListedId && sameSubscriptionIdentity(subscriptions_, subscriptions)) {
        return;
    }

    subscriptions_ = subscriptions;
    listedSubscriptionId_ = trimmedListedId;
    refreshTexts();
}

bool ServerTableEmptyStateWidget::sameSubscriptionIdentity(
    const QList<SubItem>& left,
    const QList<SubItem>& right)
{
    // Only what the wording depends on: which subscriptions exist, what they are called,
    // and whether they are enabled. The download bookkeeping (etag, failStreak) changes
    // without changing a single character on screen.
    if (left.size() != right.size()) {
        return false;
    }

    for (int index = 0; index < left.size(); ++index) {
        const SubItem& a = left.at(index);
        const SubItem& b = right.at(index);
        if (a.id != b.id || a.remarks != b.remarks || a.enabled != b.enabled) {
            return false;
        }
    }

    return true;
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

QPushButton* ServerTableEmptyStateWidget::refreshButton() const
{
    return refreshButton_;
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
    refreshButton_ = new QPushButton(this);
    refreshButton_->setObjectName(QStringLiteral("serverEmptyStateRefreshButton"));
    AppTheme::applyCompactFont({importButton_, addButton_, clipboardButton_, refreshButton_});

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    buttonRow->addStretch(1);
    buttonRow->addWidget(importButton_);
    buttonRow->addWidget(addButton_);
    buttonRow->addWidget(clipboardButton_);
    buttonRow->addWidget(refreshButton_);
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
    connect(refreshButton_, &QPushButton::clicked, this, [this]() {
        emit refreshSubscriptionRequested(listedSubscriptionId_);
    });

    refreshTexts();
}

bool ServerTableEmptyStateWidget::canStartBackgroundTask() const
{
    return !canStartBackgroundTask_ || canStartBackgroundTask_();
}

void ServerTableEmptyStateWidget::refreshActionState()
{
    syncActionVisibility();
    refreshActionEnablement();
}

QString ServerTableEmptyStateWidget::subscriptionDisplayName(const QString& subscriptionId) const
{
    for (const SubItem& item : subscriptions_) {
        if (item.id.trimmed() == subscriptionId) {
            return SubscriptionViewSupport::subscriptionDisplayName(item);
        }
    }

    return {};
}

void ServerTableEmptyStateWidget::refreshTexts()
{
    switch (state_) {
    case ServerEmptyStatePolicy::State::FirstRun:
        titleLabel_->setText(tr("No servers yet"));
        hintLabel_->setText(tr("Import a subscription or share link to get started."));
        importButton_->setText(tr("Import from File or URL"));
        addButton_->setText(tr("Add Server"));
        clipboardButton_->setText(tr("From Clipboard"));
        break;

    case ServerEmptyStatePolicy::State::SubscriptionWithoutNodes: {
        // The subscription is on file but produced nothing. Retrying the download is
        // the fix, so the panel leads with it instead of repeating the first-run guide.
        titleLabel_->setText(tr("This subscription has no servers yet"));
        hintLabel_->setText(
            tr("Its address is saved but no node has been downloaded from it. "
               "Refresh it, or add a server by hand."));
        refreshButton_->setText(tr("Refresh Subscription"));
        addButton_->setText(tr("Add Server"));
        clipboardButton_->setText(tr("From Clipboard"));
        break;
    }

    case ServerEmptyStatePolicy::State::WithheldSubscription: {
        // The nodes exist but this tab does not list them. The name of the
        // subscription is what makes the sentence actionable, so fall back to a
        // generic phrasing rather than an empty pair of quotes.
        const QString name = subscriptionDisplayName(withheldSubscriptionId_);
        titleLabel_->setText(tr("No servers in this view"));
        hintLabel_->setText(name.isEmpty()
                ? tr("These servers are grouped under a subscription. "
                     "Select its tab above to list them.")
                : tr("These servers are grouped under \"%1\". "
                     "Select its tab above to list them.").arg(name));
        break;
    }

    case ServerEmptyStatePolicy::State::NodesElsewhere:
        // Two situations share this state: the user's own filter emptied the table, or
        // the tab on screen is not where these nodes live. Only the first has a fix the
        // panel can name, and the filter box is where that is visible.
        if (textFilterActive_) {
            titleLabel_->setText(tr("No servers match the filter"));
            hintLabel_->setText(tr("Clear the filter box above to see all servers again."));
        } else {
            titleLabel_->setText(tr("No servers in this view"));
            hintLabel_->setText(tr("These servers belong to another view. "
                                   "Select its tab above to list them."));
        }
        break;
    }

    // An import that downloaded fine and produced no node lands the user on the same
    // empty table a fresh install shows, so the reason has to outrank the state that
    // stands for "nothing to list": otherwise the panel tells someone who just
    // imported to go import something. Only the first line goes here -- the parser
    // report follows it and would push the panel past the height of the table it
    // stands in for -- and the whole reason goes on the tooltip.
    if (!importFailure_.isEmpty() && state_ != ServerEmptyStatePolicy::State::NodesElsewhere) {
        const QString firstLine = importFailure_.section(QChar('\n'), 0, 0);
        titleLabel_->setText(tr("Import failed"));
        hintLabel_->setText(
            tr("%1\nFix the reason above, then import again. The log panel below has the full report.")
                .arg(firstLine));
        titleLabel_->setToolTip(importFailure_);
        hintLabel_->setToolTip(importFailure_);
    } else {
        // Both labels are rewritten wholesale above, so a tooltip left over from an
        // earlier failure would describe text that is no longer there.
        titleLabel_->setToolTip(QString());
        hintLabel_->setToolTip(QString());
    }

    refreshActionState();
}

void ServerTableEmptyStateWidget::syncActionVisibility()
{
    // Exactly one set of actions is visible, and it is the set that answers the
    // question this state is asking. A first-run page shows all three ways in; a
    // broken subscription shows the refresh plus a manual way out; a filter shows
    // nothing at all, because the fix is in the box above.
    const bool firstRun = state_ == ServerEmptyStatePolicy::State::FirstRun;
    const bool subscriptionWithoutNodes =
        state_ == ServerEmptyStatePolicy::State::SubscriptionWithoutNodes;

    // WithheldSubscription and NodesElsewhere fall through to "nothing visible":
    // the message alone says where the servers went, and offering an import there
    // would only invite a duplicate the user does not need.
    importButton_->setVisible(firstRun);
    addButton_->setVisible(firstRun || subscriptionWithoutNodes);
    clipboardButton_->setVisible(firstRun || subscriptionWithoutNodes);
    refreshButton_->setVisible(subscriptionWithoutNodes);
}
