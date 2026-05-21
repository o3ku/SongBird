#include "ui/mainwindow/ServerActionsController.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QMessageBox>
#include <QTableView>
#include <QTimer>

#include "common/DialogUtils.h"
#include "services/ServerService.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/models/ServerTableModel.h"

namespace {

void showPopupMenu(QMenu* menu, const QPoint& globalPosition)
{
    if (menu == nullptr) {
        return;
    }

    QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(globalPosition);
}

} // namespace

ServerActionsController::ServerActionsController(
    const Context& context,
    std::function<QString()> selectedServerId,
    std::function<QStringList()> selectedServerIds,
    std::function<bool()> isUngroupedSubscriptionTabSelected,
    std::function<bool()> canStartBackgroundTask,
    std::function<void()> updateActionState,
    std::function<void(const QString&)> appendLog,
    std::function<void(const QString&, int, int)> showTransientStatus,
    std::function<QStringList()> selectedShareLinks,
    std::function<QStringList()> selectedSubscriptionContent)
    : context_(context)
    , selectedServerId_(std::move(selectedServerId))
    , selectedServerIds_(std::move(selectedServerIds))
    , isUngroupedSubscriptionTabSelected_(std::move(isUngroupedSubscriptionTabSelected))
    , canStartBackgroundTask_(std::move(canStartBackgroundTask))
    , updateActionState_(std::move(updateActionState))
    , appendLog_(std::move(appendLog))
    , showTransientStatus_(std::move(showTransientStatus))
    , selectedShareLinks_(std::move(selectedShareLinks))
    , selectedSubscriptionContent_(std::move(selectedSubscriptionContent))
{
}

void ServerActionsController::setup()
{
    QObject::connect(context_.editServerAction, &QAction::triggered, context_.window, [this]() {
        const QString indexId = selectedServerId_();
        if (!indexId.isEmpty()) {
            emit context_.window->editServerRequested(indexId);
        }
    });
    QObject::connect(context_.duplicateServerAction, &QAction::triggered, context_.window, [this]() {
        const QString indexId = selectedServerId_();
        if (!indexId.isEmpty()) {
            emit context_.window->duplicateServerRequested(indexId);
        }
    });
    QObject::connect(context_.exportClientConfigAction, &QAction::triggered, context_.window, [this]() {
        const QString indexId = selectedServerId_();
        if (!indexId.isEmpty()) {
            emit context_.window->exportClientConfigRequested(indexId);
        }
    });
    QObject::connect(context_.exportServerConfigAction, &QAction::triggered, context_.window, [this]() {
        const QString indexId = selectedServerId_();
        if (!indexId.isEmpty()) {
            emit context_.window->exportServerConfigRequested(indexId);
        }
    });
    QObject::connect(context_.copyUrlAction, &QAction::triggered, context_.window, [this]() {
        copySelectedServerUrlsToClipboard(context_.copyUrlAction);
    });
    QObject::connect(context_.copyShareLinkAction, &QAction::triggered, context_.window, [this]() {
        copySelectedServerUrlsToClipboard(context_.copyShareLinkAction);
    });
    QObject::connect(context_.copySubscriptionContentAction, &QAction::triggered, context_.window, [this]() {
        const QStringList shareLinks = selectedSubscriptionContent_();
        if (shareLinks.isEmpty()) {
            showTransientStatus_(QObject::tr("Subscription content unavailable for the selected server."), 4000, 0);
            return;
        }

        if (QApplication::clipboard() != nullptr) {
            QApplication::clipboard()->setText(QString::fromLatin1(shareLinks.join(QChar('\n')).toUtf8().toBase64()));
        }
        const QString message =
            QObject::tr("Copied subscription content for %1 server(s) to the clipboard.").arg(shareLinks.size());
        appendLog_(message);
        showTransientStatus_(message, 3000, 1);
    });
    QObject::connect(context_.openCustomConfigAction, &QAction::triggered, context_.window, [this]() {
        const QString indexId = selectedServerId_();
        if (!indexId.isEmpty()) {
            emit context_.window->openCustomConfigRequested(indexId);
        }
    });
    QObject::connect(context_.removeServerAction, &QAction::triggered, context_.window, [this]() {
        const QStringList indexIds = selectedServerIds_();
        if (indexIds.isEmpty()) {
            return;
        }

        const QString message = indexIds.size() == 1
            ? QObject::tr("Remove the selected server?")
            : QObject::tr("Remove the selected %1 servers?").arg(indexIds.size());
        if (DialogUtils::askYesNoQuestion(
                context_.window,
                QObject::tr("Remove Servers"),
                message,
                QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }

        emit context_.window->removeServersRequested(indexIds);
    });
    QObject::connect(context_.moveServerTopAction, &QAction::triggered, context_.window, [this]() {
        emit context_.window->moveServersRequested(
            selectedServerIds_(),
            static_cast<int>(ServerMoveOperation::Top));
    });
    QObject::connect(context_.moveServerUpAction, &QAction::triggered, context_.window, [this]() {
        emit context_.window->moveServersRequested(
            selectedServerIds_(),
            static_cast<int>(ServerMoveOperation::Up));
    });
    QObject::connect(context_.moveServerDownAction, &QAction::triggered, context_.window, [this]() {
        emit context_.window->moveServersRequested(
            selectedServerIds_(),
            static_cast<int>(ServerMoveOperation::Down));
    });
    QObject::connect(context_.moveServerBottomAction, &QAction::triggered, context_.window, [this]() {
        emit context_.window->moveServersRequested(
            selectedServerIds_(),
            static_cast<int>(ServerMoveOperation::Bottom));
    });
    QObject::connect(context_.testAction, &QAction::triggered, context_.window, [this]() {
        if (canStartBackgroundTask_ && !canStartBackgroundTask_()) {
            return;
        }
        emit context_.window->testServersRequested(selectedServerIds_());
    });
    QObject::connect(context_.setDefaultServerAction, &QAction::triggered, context_.window, [this]() {
        const QString indexId = selectedServerId_();
        if (!indexId.isEmpty()) {
            emit context_.window->setDefaultServerRequested(indexId);
        }
    });
    QObject::connect(context_.setDefaultServerWithTunAction, &QAction::triggered, context_.window, [this]() {
        const QString indexId = selectedServerId_();
        if (!indexId.isEmpty()) {
            emit context_.window->setDefaultServerWithTunRequested(indexId);
        }
    });

    if (context_.serverView != nullptr) {
        context_.serverView->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(context_.serverView, &QWidget::customContextMenuRequested, context_.window, [this](const QPoint& position) {
            showContextMenu(position);
        });
        QObject::connect(context_.serverView, &QTableView::doubleClicked, context_.window, [this](const QModelIndex&) {
            const QString indexId = selectedServerId_();
            if (!indexId.isEmpty()) {
                emit context_.window->setDefaultServerRequested(indexId);
            }
        });

        context_.copyShareLinkAction->setShortcut(QKeySequence::Copy);
        context_.copyShareLinkAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.copyShareLinkAction);

        context_.importClipboardAction->setShortcut(QKeySequence::Paste);
        context_.importClipboardAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.importClipboardAction);

        context_.setDefaultServerAction->setShortcut(QKeySequence(Qt::Key_Return));
        context_.setDefaultServerAction->setShortcuts({
            QKeySequence(Qt::Key_Return),
            QKeySequence(Qt::Key_Enter)});
        context_.setDefaultServerAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.setDefaultServerAction);

        context_.setDefaultServerWithTunAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
        context_.setDefaultServerWithTunAction->setShortcuts({
            QKeySequence(Qt::CTRL | Qt::Key_Return),
            QKeySequence(Qt::CTRL | Qt::Key_Enter)});
        context_.setDefaultServerWithTunAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.setDefaultServerWithTunAction);

        context_.removeServerAction->setShortcut(QKeySequence::Delete);
        context_.removeServerAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.removeServerAction);

        context_.moveServerTopAction->setShortcut(QKeySequence(Qt::Key_T));
        context_.moveServerTopAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.moveServerTopAction);

        context_.moveServerBottomAction->setShortcut(QKeySequence(Qt::Key_B));
        context_.moveServerBottomAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.moveServerBottomAction);

        context_.moveServerUpAction->setShortcut(QKeySequence(Qt::Key_U));
        context_.moveServerUpAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.moveServerUpAction);

        context_.moveServerDownAction->setShortcut(QKeySequence(Qt::Key_D));
        context_.moveServerDownAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        context_.serverView->addAction(context_.moveServerDownAction);

        auto* selectAllServersShortcutAction = new QAction(context_.window);
        selectAllServersShortcutAction->setObjectName(QStringLiteral("selectAllServersShortcutAction"));
        selectAllServersShortcutAction->setShortcut(QKeySequence::SelectAll);
        selectAllServersShortcutAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(selectAllServersShortcutAction, &QAction::triggered, context_.window, [this]() {
            if (context_.serverView != nullptr
                && context_.serverView->model() != nullptr
                && context_.serverView->model()->rowCount() > 0) {
                context_.serverView->selectAll();
            }
        });
        context_.serverView->addAction(selectAllServersShortcutAction);
    }
}

ServerActionsController::ActionState
ServerActionsController::buildActionState(const SelectionSnapshot& snapshot) const
{
    const ServerTableRow* selectedItem = snapshot.selectedItems.isEmpty()
        ? nullptr
        : snapshot.selectedItems.constFirst();
    const bool hasSelection = selectedItem != nullptr;
    const bool hasSingleSelection = snapshot.selectedItems.size() == 1;
    const bool hasCustomSelection = hasSingleSelection && selectedItem->configType == ConfigType::Custom;
    const bool hasServerConfigSelection = hasSingleSelection
        && (selectedItem->configType == ConfigType::VMess || selectedItem->configType == ConfigType::VLESS)
        && selectedItem->streamSecurity.trimmed().compare(QStringLiteral("reality"), Qt::CaseInsensitive) != 0;
    const bool hasShareExports = !snapshot.selectedShareLinks.isEmpty();
    const bool canReorder = hasSelection && snapshot.hasServers;
    const bool canSpeedTest = hasSelection && snapshot.canStartTask;

    return {
        hasSelection,
        hasSingleSelection,
        hasCustomSelection,
        hasServerConfigSelection,
        hasShareExports,
        canReorder,
        canSpeedTest
    };
}

void ServerActionsController::syncActionState(const ActionState& state)
{
    if (context_.editServerAction != nullptr) {
        context_.editServerAction->setEnabled(state.hasSelection);
    }

    if (context_.duplicateServerAction != nullptr) {
        context_.duplicateServerAction->setEnabled(state.hasSelection);
    }

    if (context_.exportClientConfigAction != nullptr) {
        context_.exportClientConfigAction->setEnabled(state.hasSingleSelection);
    }

    if (context_.exportServerConfigAction != nullptr) {
        context_.exportServerConfigAction->setEnabled(state.hasServerConfigSelection);
    }

    if (context_.copyUrlAction != nullptr) {
        context_.copyUrlAction->setEnabled(state.hasShareExports);
    }

    if (context_.copyShareLinkAction != nullptr) {
        context_.copyShareLinkAction->setEnabled(state.hasShareExports);
    }

    if (context_.copySubscriptionContentAction != nullptr) {
        context_.copySubscriptionContentAction->setEnabled(state.hasShareExports);
    }

    if (context_.openCustomConfigAction != nullptr) {
        context_.openCustomConfigAction->setEnabled(state.hasCustomSelection);
    }

    if (context_.removeServerAction != nullptr) {
        context_.removeServerAction->setEnabled(state.hasSelection);
    }

    if (context_.moveServerTopAction != nullptr) {
        context_.moveServerTopAction->setEnabled(state.canReorder);
    }

    if (context_.moveServerUpAction != nullptr) {
        context_.moveServerUpAction->setEnabled(state.canReorder);
    }

    if (context_.moveServerDownAction != nullptr) {
        context_.moveServerDownAction->setEnabled(state.canReorder);
    }

    if (context_.moveServerBottomAction != nullptr) {
        context_.moveServerBottomAction->setEnabled(state.canReorder);
    }

    if (context_.setDefaultServerAction != nullptr) {
        context_.setDefaultServerAction->setEnabled(state.hasSelection);
    }
    if (context_.setDefaultServerWithTunAction != nullptr) {
        context_.setDefaultServerWithTunAction->setEnabled(state.hasSingleSelection);
    }

    if (context_.testAction != nullptr) {
        context_.testAction->setEnabled(state.canSpeedTest);
    }
}

void ServerActionsController::showContextMenu(const QPoint& position)
{
    if (context_.serverView == nullptr || context_.serverView->selectionModel() == nullptr) {
        return;
    }

    const QModelIndex clickedIndex = context_.serverView->indexAt(position);
    const bool ungroupedTabSelected = isUngroupedSubscriptionTabSelected_();
    if (!clickedIndex.isValid()) {
        if (!ungroupedTabSelected || context_.addServerAction == nullptr) {
            return;
        }

        auto* menu = new QMenu(context_.serverView);
        menu->addAction(context_.addServerAction);
        showPopupMenu(menu, context_.serverView->viewport()->mapToGlobal(position));
        return;
    }

    if (!context_.serverView->selectionModel()->isRowSelected(clickedIndex.row(), QModelIndex())) {
        context_.serverView->clearSelection();
        context_.serverView->setCurrentIndex(clickedIndex);
        context_.serverView->selectRow(clickedIndex.row());
    }

    updateActionState_();
    const QStringList selectedIds = selectedServerIds_();
    if (selectedIds.isEmpty()) {
        return;
    }

    const bool singleSelection = selectedIds.size() == 1;
    auto* menu = new QMenu(context_.serverView);

    if (singleSelection) {
        menu->addAction(context_.setDefaultServerAction);
        menu->addAction(context_.setDefaultServerWithTunAction);
        menu->addAction(context_.copyUrlAction);

        if (ungroupedTabSelected) {
            menu->addAction(context_.editServerAction);
            menu->addAction(context_.removeServerAction);
            menu->addSeparator();
            menu->addAction(context_.testAction);
            menu->addSeparator();
            menu->addAction(context_.addServerAction);
        } else {
            menu->addSeparator();
            menu->addAction(context_.testAction);
        }
    } else {
        menu->addAction(context_.copyUrlAction);
        menu->addAction(context_.testAction);
        if (ungroupedTabSelected) {
            menu->addSeparator();
            menu->addAction(context_.removeServerAction);
        }
    }

    showPopupMenu(menu, context_.serverView->viewport()->mapToGlobal(position));
}

void ServerActionsController::copySelectedServerUrlsToClipboard(QObject* senderObject)
{
    const QStringList shareLinks = selectedShareLinks_();
    const bool copyShareLinkRequested = senderObject == context_.copyShareLinkAction;
    if (shareLinks.isEmpty()) {
        showTransientStatus_(
            copyShareLinkRequested
                ? QObject::tr("Share link unavailable for the selected server.")
                : QObject::tr("URL unavailable for the selected server."),
            4000,
            0);
        return;
    }

    if (QApplication::clipboard() != nullptr) {
        QApplication::clipboard()->setText(shareLinks.join(QChar('\n')));
    }

    const QString message = copyShareLinkRequested
        ? QObject::tr("Copied %1 share link(s) to the clipboard.").arg(shareLinks.size())
        : QObject::tr("Copied %1 URL(s) to the clipboard.").arg(shareLinks.size());
    appendLog_(message);
    showTransientStatus_(message, 3000, 1);
}
