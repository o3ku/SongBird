#pragma once

#include <functional>

#include <QIcon>
#include <QList>
#include <QString>

#include "app/RuntimeState.h"
#include "domain/models/RoutingItem.h"
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
    const QString& currentServerName,
    ProxyUiState proxyUiState,
    bool systemProxyApplied,
    bool autoRunEnabled,
    const QString& routingSummary,
    bool backgroundTaskRunning,
    const QString& backgroundTaskDescription);
QIcon defaultTrayIcon();
QIcon trayIconForState(ProxyUiState proxyUiState, bool systemProxyApplied);
void applyWindowIcon(const QIcon& icon, QWidget* window);
QList<TrayServerEntry> makeServerEntries(const QList<VmessItem>& servers);
QList<TrayRoutingEntry> makeRoutingEntries(const QList<RoutingItem>& routings);
int serverMenuMaxCount();
QList<int> visibleServerIndexes(
    const QList<TrayServerEntry>& servers,
    const QString& currentServerId,
    int maximumCount);
QString describeRouting(const RoutingItem& item, int index);
void rebuildServerMenu(
    QMenu* menu,
    const QList<TrayServerEntry>& servers,
    const QString& currentServerId,
    QObject* receiver,
    const std::function<void(const QString&)>& selectServer);
void rebuildRoutingMenu(
    QMenu* menu,
    const QList<TrayRoutingEntry>& routings,
    int currentRoutingIndex,
    bool advancedRoutingEnabled,
    QObject* receiver,
    const std::function<void(int)>& selectRouting);

} // namespace TrayMenuSupport
