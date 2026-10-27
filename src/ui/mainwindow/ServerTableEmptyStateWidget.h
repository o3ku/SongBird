#pragma once

#include <QWidget>

#include <functional>

#include "ui/mainwindow/ServerEmptyStatePolicy.h"

class QLabel;
class QPushButton;

// The panel shown in the server table's place when there is nothing to list.
//
// For a first run it offers the three ways a user can get servers into the app:
// importing a share link or subscription URL, adding one by hand, and pasting
// from the clipboard. For every other reason the table is empty it offers the one
// action that fits that reason instead -- see ServerEmptyStatePolicy, which owns
// the decision. It is a plain widget with signals rather than a dialog so the
// workspace can stack it over the table and the window can wire it to the same
// actions the toolbar uses.
class ServerTableEmptyStateWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ServerTableEmptyStateWidget(QWidget* parent = nullptr);

    // Chooses the wording and the visible actions. `withheldSubscriptionId` names the
    // subscription whose tab the withheld nodes live under, so the message can point at
    // it; empty falls back to the generic phrasing.
    void setState(
        ServerEmptyStatePolicy::State state,
        const QString& withheldSubscriptionId = {});
    ServerEmptyStatePolicy::State state() const;
    bool filterActive() const;

    // The subscriptions on record and the tab this table is showing (a subscription
    // id, or empty for "Ungrouped"). Only used to name the subscription in the
    // wording and to aim the refresh, but it has to be pushed in because nodes that
    // are withheld are precisely the ones the widget cannot see.
    void setSubscriptions(const QList<SubItem>& subscriptions, const QString& listedSubscriptionId);

    // Asks the owner whether the window is allowed to start a background task
    // right now. A subscription refresh is one, and it is disabled while the proxy
    // is starting, so the button has to follow that rule rather than stay clickable
    // and do nothing. Without a predicate the button is always enabled.
    void setCanStartBackgroundTask(std::function<bool()> predicate);
    // Re-asks the predicate and updates the refresh button. Exposed because the answer
    // changes on paths that touch nothing this widget watches -- a proxy activation, the
    // startup checklist -- and the last config sync's answer is stale by then.
    void refreshActionEnablement();

    QPushButton* importButton() const;
    QPushButton* addButton() const;
    QPushButton* clipboardButton() const;
    QPushButton* refreshButton() const;

signals:
    void importRequested();
    void addRequested();
    void importFromClipboardRequested();
    // Refresh the selected subscription; emitted with an empty id when the target is
    // the "Ungrouped" tab, which is the window's cue to refresh all of them.
    void refreshSubscriptionRequested(const QString& subscriptionId);

private:
    void setupUi();
    void refreshTexts();
    void syncActionVisibility();
    void refreshActionState();
    bool canStartBackgroundTask() const;
    static bool sameSubscriptionIdentity(const QList<SubItem>& left, const QList<SubItem>& right);
    QString subscriptionDisplayName(const QString& subscriptionId) const;

    QLabel* titleLabel_ = nullptr;
    QLabel* hintLabel_ = nullptr;
    QPushButton* importButton_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* clipboardButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    std::function<bool()> canStartBackgroundTask_;
    ServerEmptyStatePolicy::State state_ = ServerEmptyStatePolicy::State::FirstRun;
    QString withheldSubscriptionId_;
    // The tab this table is showing: a subscription id, or empty for "Ungrouped". Only
    // used to aim the refresh at the same subscription the panel is talking about.
    QString listedSubscriptionId_;
    QList<SubItem> subscriptions_;
};
