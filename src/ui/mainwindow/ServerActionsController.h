#pragma once

#include <functional>

#include <QList>
#include <QStringList>

class QAction;
class MainWindow;
class QObject;
class QTableView;
struct ServerTableRow;

class ServerActionsController final {
public:
    struct Context {
        MainWindow* window = nullptr;
        QTableView* serverView = nullptr;
        QAction* addServerAction = nullptr;
        QAction* editServerAction = nullptr;
        QAction* copyUrlAction = nullptr;
        QAction* copyShareLinkAction = nullptr;
        QAction* importClipboardAction = nullptr;
        QAction* removeServerAction = nullptr;
        QAction* moveServerTopAction = nullptr;
        QAction* moveServerUpAction = nullptr;
        QAction* moveServerDownAction = nullptr;
        QAction* moveServerBottomAction = nullptr;
        QAction* testAction = nullptr;
        QAction* setDefaultServerAction = nullptr;
        QAction* setDefaultServerWithTunAction = nullptr;
    };

    struct ActionState {
        bool hasSelection = false;
        bool hasSingleSelection = false;
        bool hasShareExports = false;
        bool canReorder = false;
        bool canSpeedTest = false;
    };

    struct SelectionSnapshot {
        QList<const ServerTableRow*> selectedItems;
        bool hasServers = false;
        bool canStartTask = false;
        QStringList selectedShareLinks;
    };

    ServerActionsController(
        const Context& context,
        std::function<QString()> selectedServerId,
        std::function<QStringList()> selectedServerIds,
        std::function<bool()> isUngroupedSubscriptionTabSelected,
        std::function<bool()> canStartBackgroundTask,
        std::function<void()> updateActionState,
        std::function<void(const QString&)> appendLog,
        std::function<void(const QString&, int, int)> showTransientStatus,
        std::function<QStringList()> selectedShareLinks);

    void setup();
    ActionState buildActionState(const SelectionSnapshot& snapshot) const;
    void syncActionState(const ActionState& state);
    void showContextMenu(const QPoint& position);
    void copySelectedServerUrlsToClipboard(QObject* senderObject);

private:
    Context context_;
    std::function<QString()> selectedServerId_;
    std::function<QStringList()> selectedServerIds_;
    std::function<bool()> isUngroupedSubscriptionTabSelected_;
    std::function<bool()> canStartBackgroundTask_;
    std::function<void()> updateActionState_;
    std::function<void(const QString&)> appendLog_;
    std::function<void(const QString&, int, int)> showTransientStatus_;
    std::function<QStringList()> selectedShareLinks_;
};
