#pragma once

#include <QHash>
#include <QMap>
#include <QString>

class QAction;
class QObject;

struct MainWindowToolbarActions {
    QAction* addServerAction = nullptr;
    QAction* editServerAction = nullptr;
    QAction* copyUrlAction = nullptr;
    QAction* copyShareLinkAction = nullptr;
    QAction* importClipboardAction = nullptr;
    QAction* subAction = nullptr;
    QAction* routingSettingsAction = nullptr;
    QAction* updateSubscriptionsAction = nullptr;
    QMap<int, QAction*> updateCoreActions;
    QAction* updateGeoResourcesAction = nullptr;
    QAction* removeServerAction = nullptr;
    QAction* moveServerTopAction = nullptr;
    QAction* moveServerUpAction = nullptr;
    QAction* moveServerDownAction = nullptr;
    QAction* moveServerBottomAction = nullptr;
    QAction* testAction = nullptr;
    QAction* setDefaultServerAction = nullptr;
    QAction* setDefaultServerWithTunAction = nullptr;
    QAction* settingsAction = nullptr;
    QAction* aboutAction = nullptr;
    QAction* checkAppUpdateAction = nullptr;
    QAction* uwpLoopbackAction = nullptr;
    QAction* proxyToggleAction = nullptr;
    QAction* tunToggleAction = nullptr;
    QAction* updateCurrentSubscriptionShortcutAction = nullptr;
    QAction* toggleQrPanelAction = nullptr;
    QHash<QAction*, QString> iconFiles;
};

MainWindowToolbarActions createMainWindowToolbarActions(QObject* owner, bool qrPreviewVisible);
