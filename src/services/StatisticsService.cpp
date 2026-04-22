#include "services/StatisticsService.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTime>

namespace {

constexpr qint64 kDotNetUnixEpochOffsetTicks = 621355968000000000LL;

QJsonObject toJsonObject(const ServerStatItem& item)
{
    QJsonObject object;
    object.insert(QStringLiteral("itemId"), item.itemId);
    object.insert(QStringLiteral("totalUp"), static_cast<qint64>(item.totalUp));
    object.insert(QStringLiteral("totalDown"), static_cast<qint64>(item.totalDown));
    object.insert(QStringLiteral("todayUp"), static_cast<qint64>(item.todayUp));
    object.insert(QStringLiteral("todayDown"), static_cast<qint64>(item.todayDown));
    object.insert(QStringLiteral("dateNow"), item.dateNow);
    return object;
}

ServerStatItem fromJsonObject(const QJsonObject& object)
{
    ServerStatItem item;
    item.itemId = object.value(QStringLiteral("itemId")).toString();
    item.totalUp = static_cast<quint64>(object.value(QStringLiteral("totalUp")).toVariant().toULongLong());
    item.totalDown = static_cast<quint64>(object.value(QStringLiteral("totalDown")).toVariant().toULongLong());
    item.todayUp = static_cast<quint64>(object.value(QStringLiteral("todayUp")).toVariant().toULongLong());
    item.todayDown = static_cast<quint64>(object.value(QStringLiteral("todayDown")).toVariant().toULongLong());
    item.dateNow = object.value(QStringLiteral("dateNow")).toVariant().toLongLong();
    return item;
}

} // namespace

StatisticsService::StatisticsService(QString statisticsFilePath, QObject* parent)
    : QObject(parent)
    , statisticsFilePath_(std::move(statisticsFilePath))
{
    load();
}

void StatisticsService::load()
{
    statistics_.server.clear();

    QFile file(statisticsFilePath_);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (document.isObject()) {
            const QJsonArray array = document.object().value(QStringLiteral("server")).toArray();
            for (const QJsonValue& value : array) {
                if (value.isObject()) {
                    statistics_.server.append(fromJsonObject(value.toObject()));
                }
            }
        }
    }

    normalizeLoadedStatistics();
    emit statisticsChanged();
}

void StatisticsService::save()
{
    if (statisticsFilePath_.trimmed().isEmpty()) {
        return;
    }

    normalizeLoadedStatistics();

    const QFileInfo fileInfo(statisticsFilePath_);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return;
    }

    QJsonArray serverArray;
    for (const ServerStatItem& item : statistics_.server) {
        serverArray.append(toJsonObject(item));
    }

    QJsonObject root;
    root.insert(QStringLiteral("server"), serverArray);

    QSaveFile file(statisticsFilePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0) {
        return;
    }

    file.commit();
}

void StatisticsService::startSession(
    const Config& config,
    const QString& activeServerId,
    CoreType runtimeCore,
    int statePort,
    bool runtimeConfigReady,
    bool externallyManaged)
{
    sessionState_.enabled = config.enableStatistics;
    sessionState_.running = sessionState_.enabled;
    sessionState_.runtimeConfigReady = runtimeConfigReady;
    sessionState_.pollingAvailable = false;
    sessionState_.externallyManaged = externallyManaged;
    sessionState_.coreType = runtimeCore;
    sessionState_.activeServerId = activeServerId.trimmed();
    sessionState_.statePort = statePort;
    sessionState_.refreshRateSeconds = qMax(1, config.statisticsFreshRate);
    sessionState_.lastDeltaUp = 0;
    sessionState_.lastDeltaDown = 0;

    bool statisticsStateChanged = false;
    if (!sessionState_.activeServerId.isEmpty()) {
        ensureServerStat(sessionState_.activeServerId, &statisticsStateChanged);
    }

    if (statisticsStateChanged) {
        emit statisticsChanged();
    }

    emit sessionChanged();
}

void StatisticsService::stopSession()
{
    const bool hadSessionState = sessionState_.running
        || sessionState_.pollingAvailable
        || sessionState_.runtimeConfigReady
        || sessionState_.statePort > 0
        || sessionState_.refreshRateSeconds > 0
        || sessionState_.lastDeltaUp > 0
        || sessionState_.lastDeltaDown > 0;

    sessionState_.running = false;
    sessionState_.runtimeConfigReady = false;
    sessionState_.pollingAvailable = false;
    sessionState_.statePort = 0;
    sessionState_.refreshRateSeconds = 0;
    sessionState_.lastDeltaUp = 0;
    sessionState_.lastDeltaDown = 0;

    if (hadSessionState) {
        emit sessionChanged();
    }
}

void StatisticsService::setPollingAvailable(bool available)
{
    const bool previousAvailable = sessionState_.pollingAvailable;
    const quint64 previousUp = sessionState_.lastDeltaUp;
    const quint64 previousDown = sessionState_.lastDeltaDown;

    sessionState_.pollingAvailable = available;
    if (!available) {
        sessionState_.lastDeltaUp = 0;
        sessionState_.lastDeltaDown = 0;
    }

    if (previousAvailable != sessionState_.pollingAvailable
        || previousUp != sessionState_.lastDeltaUp
        || previousDown != sessionState_.lastDeltaDown) {
        emit sessionChanged();
    }
}

void StatisticsService::applyTrafficDelta(const QString& itemId, quint64 up, quint64 down)
{
    bool statisticsStateChanged = false;
    const QString trimmedItemId = itemId.trimmed();
    if (!trimmedItemId.isEmpty()) {
        bool itemChanged = false;
        ServerStatItem* item = ensureServerStat(trimmedItemId, &itemChanged);
        if (item != nullptr) {
            item->todayUp += up;
            item->todayDown += down;
            item->totalUp += up;
            item->totalDown += down;
            statisticsStateChanged = itemChanged || up > 0 || down > 0;
        }
    }

    const bool sessionStateChanged = !sessionState_.pollingAvailable
        || sessionState_.lastDeltaUp != up
        || sessionState_.lastDeltaDown != down;
    sessionState_.pollingAvailable = true;
    sessionState_.lastDeltaUp = up;
    sessionState_.lastDeltaDown = down;

    if (statisticsStateChanged) {
        emit statisticsChanged();
    }
    if (sessionStateChanged) {
        emit sessionChanged();
    }
}

OperationResult StatisticsService::clearAll()
{
    statistics_.server.clear();
    sessionState_.lastDeltaUp = 0;
    sessionState_.lastDeltaDown = 0;
    save();
    emit statisticsChanged();
    emit sessionChanged();
    return OperationResult::ok(QStringLiteral("Statistics cleared."));
}

QList<ServerStatItem> StatisticsService::statistics() const
{
    QList<ServerStatItem> items = statistics_.server;
    const qint64 ticks = currentDateTicks();
    for (ServerStatItem& item : items) {
        normalizeServerStat(item, ticks);
    }
    return items;
}

ServerStatItem StatisticsService::currentServerStat() const
{
    if (sessionState_.activeServerId.trimmed().isEmpty()) {
        return {};
    }

    for (const ServerStatItem& item : statistics_.server) {
        if (item.itemId == sessionState_.activeServerId) {
            ServerStatItem normalized = item;
            normalizeServerStat(normalized, currentDateTicks());
            return normalized;
        }
    }

    ServerStatItem item;
    item.itemId = sessionState_.activeServerId;
    item.dateNow = currentDateTicks();
    return item;
}

StatisticsSessionState StatisticsService::sessionState() const
{
    return sessionState_;
}

void StatisticsService::normalizeLoadedStatistics()
{
    const qint64 ticks = currentDateTicks();
    for (ServerStatItem& item : statistics_.server) {
        normalizeServerStat(item, ticks);
    }
}

ServerStatItem* StatisticsService::ensureServerStat(const QString& itemId, bool* changed)
{
    if (changed != nullptr) {
        *changed = false;
    }

    if (itemId.trimmed().isEmpty()) {
        return nullptr;
    }

    const qint64 ticks = currentDateTicks();
    for (ServerStatItem& item : statistics_.server) {
        if (item.itemId == itemId) {
            if (changed != nullptr) {
                *changed = normalizeServerStat(item, ticks);
            } else {
                normalizeServerStat(item, ticks);
            }
            return &item;
        }
    }

    statistics_.server.append(ServerStatItem{
        itemId,
        0,
        0,
        0,
        0,
        ticks});
    if (changed != nullptr) {
        *changed = true;
    }
    return &statistics_.server.last();
}

bool StatisticsService::normalizeServerStat(ServerStatItem& item, qint64 currentTicks)
{
    if (item.dateNow == currentTicks) {
        return false;
    }

    item.todayUp = 0;
    item.todayDown = 0;
    item.dateNow = currentTicks;
    return true;
}

qint64 StatisticsService::currentDateTicks()
{
    return QDateTime(QDate::currentDate(), QTime(0, 0)).toMSecsSinceEpoch() * 10000
        + kDotNetUnixEpochOffsetTicks;
}
