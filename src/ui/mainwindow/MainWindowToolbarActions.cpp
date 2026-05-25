#include "ui/mainwindow/MainWindowToolbarActions.h"

#include <QAction>
#include <QCoreApplication>

#include "domain/models/VmessItem.h"
#include "runtime/ProtocolCoreCompat.h"
#include "ui/mainwindow/ToolbarWidgets.h"
#include "ui/theme/AppTheme.h"

namespace {

QString trMainWindow(const char* text)
{
    return QCoreApplication::translate("MainWindow", text);
}

QAction* createAction(QObject* owner, const QString& text, const QString& objectName = {})
{
    auto* action = new QAction(text, owner);
    if (!objectName.isEmpty()) {
        action->setObjectName(objectName);
    }
    return action;
}

void setToolbarIcon(MainWindowToolbarActions& actions, QAction* action, const QString& fileName)
{
    actions.iconFiles.insert(action, fileName);
    action->setIcon(ToolbarWidgets::loadIcon(fileName));
}

} // namespace

MainWindowToolbarActions createMainWindowToolbarActions(QObject* owner, bool qrPreviewVisible)
{
    MainWindowToolbarActions actions;

    actions.addServerAction = createAction(owner, trMainWindow("Add Server"), QStringLiteral("addServerAction"));
    actions.editServerAction = createAction(owner, trMainWindow("Edit Server"));
    actions.copyUrlAction = createAction(owner, trMainWindow("Copy Url"), QStringLiteral("copyUrlAction"));
    actions.copyShareLinkAction = createAction(owner, trMainWindow("Copy Share Link"), QStringLiteral("copyShareLinkAction"));
    actions.removeServerAction = createAction(owner, trMainWindow("Remove"), QStringLiteral("removeServerAction"));
    actions.moveServerTopAction = createAction(owner, trMainWindow("Move To Top"), QStringLiteral("moveServerTopAction"));
    actions.moveServerUpAction = createAction(owner, trMainWindow("Move Up"), QStringLiteral("moveServerUpAction"));
    actions.moveServerUpAction->setIcon(AppTheme::themedSvgIcon(QStringLiteral(":/app/up.svg")));
    actions.moveServerDownAction = createAction(owner, trMainWindow("Move Down"), QStringLiteral("moveServerDownAction"));
    actions.moveServerDownAction->setIcon(AppTheme::themedSvgIcon(QStringLiteral(":/app/down.svg")));
    actions.moveServerBottomAction = createAction(owner, trMainWindow("Move To Bottom"), QStringLiteral("moveServerBottomAction"));
    actions.testAction = createAction(owner, trMainWindow("Test"), QStringLiteral("testAction"));
    actions.setDefaultServerAction = createAction(owner, trMainWindow("Set Current"), QStringLiteral("setDefaultServerAction"));
    actions.setDefaultServerWithTunAction = createAction(
        owner,
        trMainWindow("Set Current with TUN On"),
        QStringLiteral("setDefaultServerWithTunAction"));

    actions.importClipboardAction = createAction(owner, trMainWindow("Import Clipboard"), QStringLiteral("importClipboardAction"));
    actions.subAction = createAction(owner, trMainWindow("Subscriptions"));
    setToolbarIcon(actions, actions.subAction, QStringLiteral("subscription.svg"));

    actions.routingSettingsAction = createAction(owner, trMainWindow("Routing"), QStringLiteral("routingSettingsAction"));
    setToolbarIcon(actions, actions.routingSettingsAction, QStringLiteral("routing.svg"));

    actions.updateSubscriptionsAction = createAction(
        owner,
        trMainWindow("Update Subscriptions"),
        QStringLiteral("updateSubscriptionsAction"));
    for (const CoreType coreType : orderedCoreTypes()) {
        auto* action = createAction(
            owner,
            QCoreApplication::translate("MainWindow", "Update %1 Core").arg(coreTypeDisplayName(coreType)),
            QStringLiteral("updateCoreAction_%1").arg(static_cast<int>(coreType)));
        actions.updateCoreActions.insert(static_cast<int>(coreType), action);
    }
    actions.updateGeoResourcesAction = createAction(
        owner,
        trMainWindow("Update Geo Files"),
        QStringLiteral("updateGeoResourcesAction"));

    actions.settingsAction = createAction(owner, trMainWindow("Settings"), QStringLiteral("settingsAction"));
    setToolbarIcon(actions, actions.settingsAction, QStringLiteral("option.svg"));

    actions.aboutAction = createAction(owner, trMainWindow("About"), QStringLiteral("aboutAction"));
    actions.checkAppUpdateAction = createAction(
        owner,
        trMainWindow("Check for Updates"),
        QStringLiteral("checkAppUpdateAction"));
    actions.uwpLoopbackAction = createAction(owner, trMainWindow("UWP Loopback"), QStringLiteral("uwpLoopbackAction"));

    actions.proxyToggleAction = createAction(owner, trMainWindow("START"), QStringLiteral("proxyToggleAction"));
    actions.proxyToggleAction->setCheckable(true);
    actions.tunToggleAction = createAction(owner, trMainWindow("TUN"), QStringLiteral("tunToggleAction"));
    actions.tunToggleAction->setCheckable(true);
    actions.updateCurrentSubscriptionShortcutAction = createAction(
        owner,
        {},
        QStringLiteral("updateCurrentSubscriptionShortcutAction"));
    actions.toggleQrPanelAction = createAction(owner, trMainWindow("Share"), QStringLiteral("toggleQrPanelAction"));
    actions.toggleQrPanelAction->setCheckable(true);
    actions.toggleQrPanelAction->setChecked(qrPreviewVisible);

    return actions;
}
