#include "ui/tray/TrayMenuSupport.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QMenu>
#include <QObject>
#include <QSet>
#include <QStyle>
#include <QStringList>
#include <QWidget>

#include <algorithm>

#include "common/AppPlatform.h"
#include "common/ServerDisplayName.h"
#include "services/SpeedTestServiceInternal.h"

namespace {

constexpr int TrayServerNameTextMaxWidth = 300;
constexpr int TrayCurrentServerNameTextMaxWidth = 260;
constexpr int TrayCurrentServerTooltipNameTextMaxWidth = 360;
constexpr int TrayServerMenuMaxCount = 20;

struct TrayServerSortKey
{
    int resultRank = 0;
    double latencyMs = 0;
};

QString trayText(const char* sourceText)
{
    return QCoreApplication::translate("TrayController", sourceText);
}

QString elidedText(QMenu* menu, const QString& text, int maximumWidth)
{
    if (menu == nullptr) {
        return text;
    }

    return menu->fontMetrics().elidedText(
        text,
        Qt::ElideRight,
        maximumWidth);
}

QString coreStatusText(ProxyUiState state)
{
    switch (state) {
    case ProxyUiState::Active:
        return trayText("Running");
    case ProxyUiState::Transitioning:
    case ProxyUiState::Inconsistent:
        return trayText("Starting");
    case ProxyUiState::Idle:
        break;
    }
    return trayText("Stopped");
}

TrayServerSortKey makeTrayServerSortKey(const TrayServerEntry& item)
{
    const QString normalized = item.testResult.trimmed();
    if (normalized.isEmpty()) {
        return TrayServerSortKey{1, 0};
    }

    double latencyMs = -1;
    if (SpeedTestServiceInternal::tryParseUrlProbeLatency(normalized, latencyMs)) {
        return TrayServerSortKey{0, latencyMs};
    }

    return TrayServerSortKey{2, 0};
}

QSet<QString> enabledSubscriptionIds(const QList<SubItem>& subscriptions)
{
    QSet<QString> ids;
    for (const SubItem& item : subscriptions) {
        if (!item.enabled) {
            continue;
        }

        const QString id = item.id.trimmed();
        if (!id.isEmpty()) {
            ids.insert(id);
        }
    }
    return ids;
}

bool isUngroupedSubscriptionId(const QString& subscriptionId, const QSet<QString>& knownSubscriptionIds)
{
    const QString trimmed = subscriptionId.trimmed();
    return trimmed.isEmpty() || !knownSubscriptionIds.contains(trimmed);
}

QString trayRoutingSummaryText(const QString& routingSummary)
{
    QString routing = routingSummary.trimmed();
    const QString advanced = QCoreApplication::translate("AppBootstrap", "Advanced");
    if (routing == advanced) {
        return {};
    }

    const QString advancedSuffix = QStringLiteral(" (%1)").arg(advanced);
    if (routing.endsWith(advancedSuffix)) {
        routing.chop(advancedSuffix.size());
    }
    return routing.trimmed();
}

} // namespace

QString TrayMenuSupport::formatTestResult(const QString& value)
{
    const QString normalized = value.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    double latencyMs = -1;
    if (SpeedTestServiceInternal::tryParseUrlProbeLatency(normalized, latencyMs)) {
        return QStringLiteral("%1 ms").arg(qRound(latencyMs));
    }

    return trayText("Unavailable");
}

QString TrayMenuSupport::currentServerActionText(QMenu* menu, const QString& currentServerName)
{
    const QString trimmed = currentServerName.trimmed();
    const QString currentServerText = trimmed.isEmpty()
        ? noServerPlaceholderText()
        : elidedText(menu, trimmed, TrayCurrentServerNameTextMaxWidth);
    return trayText("Current: %1").arg(currentServerText);
}

QString TrayMenuSupport::currentServerActionToolTip(QMenu* menu, const QString& currentServerName)
{
    const QString trimmed = currentServerName.trimmed();
    return trimmed.isEmpty()
        ? QString()
        : elidedText(menu, trimmed, TrayCurrentServerTooltipNameTextMaxWidth);
}

QString TrayMenuSupport::formatServerMenuText(QMenu* menu, const TrayServerEntry& item)
{
    const QString name = elidedText(menu, item.displayName, TrayServerNameTextMaxWidth);
    const QString result = formatTestResult(item.testResult);
    return result.isEmpty()
        ? name
        : QStringLiteral("%1 | %2").arg(name, result);
}

QString TrayMenuSupport::formatServerActionToolTip(QMenu* menu, const TrayServerEntry& item)
{
    const QString result = formatTestResult(item.testResult);
    const QString tooltipName = elidedText(menu, item.displayName, TrayCurrentServerTooltipNameTextMaxWidth);
    return result.isEmpty()
        ? tooltipName
        : QStringLiteral("%1 | %2").arg(tooltipName, result);
}

QString TrayMenuSupport::buildToolTip(
    QMenu* menu,
    const QString& appVersion,
    const QString& currentServerName,
    ProxyUiState proxyUiState,
    bool systemProxyApplied,
    bool autoRunEnabled,
    bool tunEnabled,
    const QString& routingSummary)
{
    const bool active = proxyUiState == ProxyUiState::Active;
    const QString proxyText = active && systemProxyApplied ? trayText("ON") : trayText("OFF");
    const QString currentServerText = currentServerName.trimmed().isEmpty()
        ? trayText("No default server")
        : currentServerName.trimmed();
    const QString currentServerDisplayText = elidedText(
        menu,
        currentServerText,
        TrayCurrentServerNameTextMaxWidth);
    const QString title = appVersion.trimmed().isEmpty()
        ? QStringLiteral("SongBird")
        : QStringLiteral("SongBird %1").arg(appVersion.trimmed());

    QStringList lines{
        title,
        trayText("Current: %1").arg(currentServerDisplayText),
        trayText("Auto Run: %1").arg(autoRunEnabled ? trayText("ON") : trayText("OFF"))};
    const QString routing = trayRoutingSummaryText(routingSummary);
    if (!routing.isEmpty()) {
        lines.append(trayText("Routing: %1").arg(routing));
    }
    lines.append(trayText("Core %1 | Proxy %2 | Tun %3")
                     .arg(coreStatusText(proxyUiState))
                     .arg(proxyText)
                     .arg(tunEnabled ? trayText("ON") : trayText("OFF")));
    return lines.join(QChar('\n'));
}

QIcon TrayMenuSupport::defaultTrayIcon()
{
    QIcon icon(QStringLiteral(":/app/logo.svg"));
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    return icon;
}

QIcon TrayMenuSupport::trayIconForState(ProxyUiState proxyUiState, bool systemProxyApplied)
{
    if (proxyUiState == ProxyUiState::Active && systemProxyApplied) {
        return QIcon(QStringLiteral(":/app/logo-fire.svg"));
    }

    return defaultTrayIcon();
}

void TrayMenuSupport::applyWindowIcon(const QIcon& icon, QWidget* window)
{
    if (icon.isNull()) {
        return;
    }

    QApplication::setWindowIcon(icon);
    if (window != nullptr) {
        window->setWindowIcon(icon);
    }
}

QList<VmessItem> TrayMenuSupport::serversInCurrentGroup(
    const QList<VmessItem>& servers,
    const QList<SubItem>& subscriptions,
    const QString& currentServerId)
{
    const QString trimmedCurrentId = currentServerId.trimmed();
    const VmessItem* currentServer = nullptr;
    for (const VmessItem& server : servers) {
        if (!trimmedCurrentId.isEmpty() && server.indexId.trimmed() == trimmedCurrentId) {
            currentServer = &server;
            break;
        }
    }
    if (currentServer == nullptr) {
        return servers;
    }

    const QSet<QString> knownSubscriptionIds = enabledSubscriptionIds(subscriptions);
    const QString currentSubscriptionId = currentServer->subId.trimmed();
    const bool currentUngrouped = isUngroupedSubscriptionId(currentSubscriptionId, knownSubscriptionIds);

    QList<VmessItem> groupedServers;
    groupedServers.reserve(servers.size());
    for (const VmessItem& server : servers) {
        const QString subscriptionId = server.subId.trimmed();
        const bool sameGroup = currentUngrouped
            ? isUngroupedSubscriptionId(subscriptionId, knownSubscriptionIds)
            : subscriptionId == currentSubscriptionId;
        if (sameGroup) {
            groupedServers.append(server);
        }
    }
    return groupedServers;
}

QList<TrayServerEntry> TrayMenuSupport::makeServerEntries(const QList<VmessItem>& servers)
{
    QList<TrayServerEntry> entries;
    entries.reserve(servers.size());
    for (const VmessItem& item : servers) {
        entries.append(TrayServerEntry{item.indexId, serverDisplayName(item), item.testResult});
    }
    return entries;
}

QList<TrayRoutingEntry> TrayMenuSupport::makeRoutingEntries(const QList<RoutingItem>& routings)
{
    QList<TrayRoutingEntry> entries;
    entries.reserve(routings.size());
    for (int index = 0; index < routings.size(); ++index) {
        const RoutingItem& item = routings.at(index);
        entries.append(TrayRoutingEntry{
            item.id,
            describeRouting(item, index),
            item.customIcon});
    }
    return entries;
}

int TrayMenuSupport::serverMenuMaxCount()
{
    return TrayServerMenuMaxCount;
}

QList<int> TrayMenuSupport::visibleServerIndexes(
    const QList<TrayServerEntry>& servers,
    const QString& currentServerId,
    int maximumCount)
{
    QList<int> serverIndexes;
    serverIndexes.reserve(servers.size());
    int currentServerIndex = -1;
    for (int index = 0; index < servers.size(); ++index) {
        if (servers.at(index).indexId == currentServerId) {
            currentServerIndex = index;
            continue;
        }
        serverIndexes.append(index);
    }

    std::stable_sort(serverIndexes.begin(), serverIndexes.end(), [&servers](int left, int right) {
        const TrayServerSortKey leftKey = makeTrayServerSortKey(servers.at(left));
        const TrayServerSortKey rightKey = makeTrayServerSortKey(servers.at(right));
        if (leftKey.resultRank != rightKey.resultRank) {
            return leftKey.resultRank < rightKey.resultRank;
        }
        if (leftKey.latencyMs != rightKey.latencyMs) {
            return leftKey.latencyMs < rightKey.latencyMs;
        }
        return left < right;
    });

    QList<int> visibleIndexes;
    visibleIndexes.reserve(qMin(maximumCount, servers.size()));
    if (currentServerIndex >= 0) {
        visibleIndexes.append(currentServerIndex);
    }
    for (int index : serverIndexes) {
        if (visibleIndexes.size() >= maximumCount) {
            break;
        }
        visibleIndexes.append(index);
    }

    return visibleIndexes;
}

QString TrayMenuSupport::describeRouting(const RoutingItem& item, int index)
{
    const QString remarks = item.remarks.trimmed();
    return remarks.isEmpty()
        ? trayText("Routing %1").arg(index + 1)
        : remarks;
}

void TrayMenuSupport::rebuildServerMenu(
    QMenu* menu,
    const QList<TrayServerEntry>& servers,
    const QString& currentServerId,
    QObject* receiver,
    const std::function<void(const QString&)>& selectServer)
{
    if (menu == nullptr) {
        return;
    }

    menu->clear();
    if (servers.isEmpty()) {
        QAction* placeholder = menu->addAction(trayText("No servers"));
        placeholder->setEnabled(false);
        return;
    }

    auto* group = new QActionGroup(menu);
    group->setExclusive(true);

    const QList<int> visibleIndexes = visibleServerIndexes(servers, currentServerId, serverMenuMaxCount());
    for (int menuIndex = 0; menuIndex < visibleIndexes.size(); ++menuIndex) {
        const int serverIndex = visibleIndexes.at(menuIndex);
        const TrayServerEntry& item = servers.at(serverIndex);
        QAction* action = menu->addAction(formatServerMenuText(menu, item));
        action->setToolTip(formatServerActionToolTip(menu, item));
        action->setCheckable(true);
        action->setChecked(item.indexId == currentServerId);
        action->setData(item.indexId);
        action->setObjectName(QStringLiteral("trayServerAction_%1").arg(menuIndex));
        group->addAction(action);
        QObject::connect(action, &QAction::triggered, receiver, [action, selectServer]() {
            selectServer(action->data().toString());
        });
    }
}

void TrayMenuSupport::rebuildRoutingMenu(
    QMenu* menu,
    const QList<TrayRoutingEntry>& routings,
    const QString& currentRoutingId,
    QObject* receiver,
    const std::function<void(const QString&)>& selectRouting)
{
    if (menu == nullptr) {
        return;
    }

    menu->clear();
    if (routings.isEmpty()) {
        QAction* placeholder = menu->addAction(trayText("No routing entries"));
        placeholder->setEnabled(false);
        return;
    }

    auto* group = new QActionGroup(menu);
    group->setExclusive(true);

    for (int index = 0; index < routings.size(); ++index) {
        QAction* action = menu->addAction(routings.at(index).displayName);
        action->setCheckable(true);
        action->setChecked(routings.at(index).id == currentRoutingId);
        action->setData(routings.at(index).id);
        action->setObjectName(QStringLiteral("trayRoutingAction_%1").arg(index));
        group->addAction(action);
        QObject::connect(action, &QAction::triggered, receiver, [action, selectRouting]() {
            selectRouting(action->data().toString());
        });
    }
}
