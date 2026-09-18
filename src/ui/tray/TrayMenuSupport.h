#pragma once

#include <functional>

#include <QIcon>
#include <QList>
#include <QString>

#include "domain/models/RuntimeState.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/SubItem.h"
#include "domain/models/VmessItem.h"

class QMenu;
class QObject;
class QWidget;

struct TrayServerEntry {
    QString indexId;
    QString displayName;
    QString testResult;
};

struct TrayRoutingEntry {
    QString id;
    QString displayName;
    QString customIconPath;
};

namespace TrayMenuSupport {

QString formatTestResult(const QString& value);
QString currentServerActionText(QMenu* menu, const QString& currentServerName);
QString currentServerActionToolTip(QMenu* menu, const QString& currentServerName);
QString formatServerMenuText(QMenu* menu, const TrayServerEntry& item);
QString formatServerActionToolTip(QMenu* menu, const TrayServerEntry& item);
QString buildToolTip(
    QMenu* menu,
    const QString& appVersion,
    const QString& currentServerName,
    ProxyUiState proxyUiState,
    bool systemProxyApplied,
    bool autoRunEnabled,
    bool tunEnabled,
    const QString& routingSummary);
QIcon defaultTrayIcon();
QIcon trayIconForState(ProxyUiState proxyUiState, bool systemProxyApplied);
void applyWindowIcon(const QIcon& icon, QWidget* window);
QList<VmessItem> serversInCurrentGroup(
    const QList<VmessItem>& servers,
    const QList<SubItem>& subscriptions,
    const QString& currentServerId);
QList<TrayServerEntry> makeServerEntries(const QList<VmessItem>& servers);
QList<TrayRoutingEntry> makeRoutingEntries(const QList<RoutingItem>& routings);
int serverMenuMaxCount();
QList<int> visibleServerIndexes(
    const QList<TrayServerEntry>& servers,
    const QString& currentServerId,
    int maximumCount);
QString describeRouting(const RoutingItem& item, int index);
// Keeps the text of plain menu rows aligned with checkable rows. Qt indents a
// checkable row by the check mark width but indents the other rows only by the
// menu wide icon column, which stays 0 while no action carries an icon.
void reserveMenuIconColumn(QMenu* menu);
void rebuildServerMenu(
    QMenu* menu,
    const QList<TrayServerEntry>& servers,
    const QString& currentServerId,
    QObject* receiver,
    const std::function<void(const QString&)>& selectServer);
void rebuildRoutingMenu(
    QMenu* menu,
    const QList<TrayRoutingEntry>& routings,
    const QString& currentRoutingId,
    QObject* receiver,
    const std::function<void(const QString&)>& selectRouting);

} // namespace TrayMenuSupport
