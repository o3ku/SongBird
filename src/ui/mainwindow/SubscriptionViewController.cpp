#include "ui/mainwindow/SubscriptionViewController.h"

#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QTabBar>

#include "ui/models/ServerFilterProxyModel.h"

SubscriptionViewController::SubscriptionViewController(
    const Context& context,
    MenuCallbacks callbacks)
    : context_(context)
    , callbacks_(std::move(callbacks))
{
}

void SubscriptionViewController::setSubscriptions(const QList<SubItem>& subscriptions)
{
    subscriptions_ = subscriptions;
}

void SubscriptionViewController::rebuildTabs(const QString& previousTabKey)
{
    if (context_.subscriptionTabBar == nullptr || context_.serverFilterModel == nullptr) {
        return;
    }

    QSet<QString> knownSubscriptionIds;
    for (const SubItem& item : subscriptions_) {
        if (!item.enabled) {
            continue;
        }

        const QString subscriptionId = item.id.trimmed();
        if (!subscriptionId.isEmpty()) {
            knownSubscriptionIds.insert(subscriptionId);
        }
    }
    context_.serverFilterModel->setKnownSubscriptionIds(knownSubscriptionIds);

    {
        const QSignalBlocker blocker(context_.subscriptionTabBar);
        for (int index = context_.subscriptionTabBar->count() - 1; index >= 0; --index) {
            context_.subscriptionTabBar->removeTab(index);
        }

        QSet<QString> addedSubscriptionIds;
        for (const SubItem& item : subscriptions_) {
            if (!item.enabled) {
                continue;
            }

            const QString subscriptionId = item.id.trimmed();
            if (subscriptionId.isEmpty() || addedSubscriptionIds.contains(subscriptionId)) {
                continue;
            }

            addedSubscriptionIds.insert(subscriptionId);
            const int index = context_.subscriptionTabBar->insertTab(0, describeSubscription(item));
            context_.subscriptionTabBar->setTabData(index, QStringLiteral("sub:%1").arg(subscriptionId));
        }

        const int ungroupedIndex = context_.subscriptionTabBar->addTab(QObject::tr("Ungrouped"));
        context_.subscriptionTabBar->setTabData(ungroupedIndex, QStringLiteral("ungrouped"));

        int restoreIndex = -1;
        for (int index = 0; index < context_.subscriptionTabBar->count(); ++index) {
            if (context_.subscriptionTabBar->tabData(index).toString() == previousTabKey) {
                restoreIndex = index;
                break;
            }
        }

        if (context_.subscriptionTabBar->count() > 0) {
            context_.subscriptionTabBar->setCurrentIndex(restoreIndex >= 0 ? restoreIndex : 0);
        }
    }
}

void SubscriptionViewController::applyFilter(const QString& filterText)
{
    if (context_.serverFilterModel == nullptr) {
        return;
    }

    const QString pattern = filterText.trimmed();
    context_.serverFilterModel->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(pattern), QRegularExpression::CaseInsensitiveOption));
}

void SubscriptionViewController::applyCurrentTabFilter()
{
    if (context_.serverFilterModel == nullptr) {
        return;
    }

    const QString tabKey = currentTabKey();
    if (tabKey == QStringLiteral("ungrouped")) {
        context_.serverFilterModel->setSubscriptionFilterMode(ServerFilterProxyModel::SubscriptionFilterMode::Ungrouped);
    } else {
        context_.serverFilterModel->setSubscriptionFilterMode(
            ServerFilterProxyModel::SubscriptionFilterMode::Subscription,
            tabKey.mid(QStringLiteral("sub:").size()));
    }
}

bool SubscriptionViewController::selectSubscriptionTab(const QString& selectionId)
{
    if (context_.subscriptionTabBar == nullptr) {
        return false;
    }

    const QString targetTabKey = resolveTabKeyFromSelectionId(selectionId);
    for (int index = 0; index < context_.subscriptionTabBar->count(); ++index) {
        if (context_.subscriptionTabBar->tabData(index).toString() == targetTabKey) {
            context_.subscriptionTabBar->setCurrentIndex(index);
            return true;
        }
    }

    return false;
}

QString SubscriptionViewController::currentTabKey() const
{
    if (context_.subscriptionTabBar == nullptr || context_.subscriptionTabBar->currentIndex() < 0) {
        return QStringLiteral("ungrouped");
    }

    return context_.subscriptionTabBar->tabData(context_.subscriptionTabBar->currentIndex()).toString();
}

QString SubscriptionViewController::currentSubscriptionUrl() const
{
    const QString subscriptionId = resolveSubscriptionIdFromTabKey(currentTabKey());
    if (subscriptionId.isEmpty()) {
        return {};
    }

    for (const SubItem& item : subscriptions_) {
        if (item.id.trimmed() == subscriptionId) {
            return item.url.trimmed();
        }
    }

    return {};
}

QString SubscriptionViewController::persistedSelectionId() const
{
    return resolvePersistedSelectionId(currentTabKey());
}

bool SubscriptionViewController::isUngroupedTabSelected() const
{
    return currentTabKey() == QStringLiteral("ungrouped");
}

void SubscriptionViewController::showTabContextMenu(const QPoint& position)
{
    if (context_.subscriptionTabBar == nullptr) {
        return;
    }

    const int tabIndex = context_.subscriptionTabBar->tabAt(position);
    if (tabIndex < 0) {
        return;
    }

    if (context_.subscriptionTabBar->currentIndex() != tabIndex) {
        context_.subscriptionTabBar->setCurrentIndex(tabIndex);
    }

    const QString tabKey = context_.subscriptionTabBar->tabData(tabIndex).toString();
    const QString subscriptionId = resolveSubscriptionIdFromTabKey(tabKey);
    QMenu menu(context_.subscriptionTabBar);

    if (!subscriptionId.isEmpty()) {
        auto* copySubscriptionUrlAction = menu.addAction(QObject::tr("Copy Subscription Url"));
        copySubscriptionUrlAction->setEnabled(!currentSubscriptionUrl().isEmpty());
        QObject::connect(copySubscriptionUrlAction, &QAction::triggered, &menu, [this]() {
            if (callbacks_.copyCurrentSubscriptionUrl) {
                callbacks_.copyCurrentSubscriptionUrl();
            }
        });

        menu.addSeparator();

        auto* updateAction = menu.addAction(QObject::tr("Update Subscription"));
        updateAction->setEnabled(callbacks_.canStartBackgroundTask && callbacks_.canStartBackgroundTask());
        QObject::connect(updateAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.updateCurrentSubscription) {
                callbacks_.updateCurrentSubscription(subscriptionId);
            }
        });

        auto* updateViaProxyAction = menu.addAction(QObject::tr("Update Subscription via Proxy"));
        updateViaProxyAction->setEnabled(callbacks_.canStartBackgroundTask && callbacks_.canStartBackgroundTask());
        QObject::connect(updateViaProxyAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.updateCurrentSubscriptionViaProxy) {
                callbacks_.updateCurrentSubscriptionViaProxy(subscriptionId);
            }
        });

        auto* hideAction = menu.addAction(QObject::tr("Hide"));
        QObject::connect(hideAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.hideSubscription) {
                callbacks_.hideSubscription(subscriptionId);
            }
        });

        auto* deleteAction = menu.addAction(QObject::tr("Delete"));
        QObject::connect(deleteAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.confirmAndDeleteSubscription) {
                callbacks_.confirmAndDeleteSubscription(subscriptionId);
            }
        });

        menu.addSeparator();
    }

    auto* manageAction = menu.addAction(QObject::tr("Subscription Management"));
    QObject::connect(manageAction, &QAction::triggered, &menu, [this]() {
        if (callbacks_.openSubscriptionManagement) {
            callbacks_.openSubscriptionManagement();
        }
    });

    menu.exec(context_.subscriptionTabBar->mapToGlobal(position));
}

QString SubscriptionViewController::resolveTabKeyFromSelectionId(const QString& selectionId)
{
    const QString normalized = selectionId.trimmed();
    if (normalized == QStringLiteral("__unsubscribed__")
        || normalized.compare(QStringLiteral("ungrouped"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("ungrouped");
    }

    return normalized.isEmpty()
        ? QStringLiteral("ungrouped")
        : QStringLiteral("sub:%1").arg(normalized);
}

QString SubscriptionViewController::resolvePersistedSelectionId(const QString& tabKey)
{
    if (tabKey == QStringLiteral("ungrouped")) {
        return QStringLiteral("__unsubscribed__");
    }

    if (tabKey.startsWith(QStringLiteral("sub:"))) {
        return tabKey.mid(QStringLiteral("sub:").size());
    }

    return {};
}

QString SubscriptionViewController::resolveSubscriptionIdFromTabKey(const QString& tabKey)
{
    return tabKey.startsWith(QStringLiteral("sub:"))
        ? tabKey.mid(QStringLiteral("sub:").size())
        : QString();
}

QString SubscriptionViewController::describeSubscription(const SubItem& item)
{
    const QString remarks = item.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    const QString url = item.url.trimmed();
    if (!url.isEmpty()) {
        return url;
    }

    return item.id.trimmed().isEmpty()
        ? QObject::tr("Subscription")
        : item.id.trimmed();
}
