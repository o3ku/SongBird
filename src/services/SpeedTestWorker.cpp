#include "services/SpeedTestWorker.h"

#include <QFileInfo>
#include <QMap>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <future>
#include <utility>
#include <vector>

#include "common/ServerDisplayName.h"
#include "services/SpeedTestBatchRuntimeRunner.h"
#include "services/SpeedTestRuntimeRunner.h"
#include "services/SpeedTestServiceInternal.h"
#include "services/SpeedTestUrlProbe.h"

namespace {

// Per-item fallback spawns a full core per server; running too many in
// parallel just contends on disk/CPU and pushes every spawn past its
// startup timeout. Keep this conservative.
constexpr int kFallbackMaxConcurrency = 8;
constexpr int kDirectProxyProbeTimeoutMs = 3000;
namespace BatchRuntimeRunner = SpeedTestBatchRuntimeRunner;
namespace RuntimeRunner = SpeedTestRuntimeRunner;
namespace UrlProbe = SpeedTestUrlProbe;

bool canProbeUpstreamProxyDirectly(const VmessItem& server)
{
    return server.configType == ConfigType::Socks
        || server.configType == ConfigType::HTTP;
}

} // namespace

SpeedTestWorker::SpeedTestWorker(QString customConfigDirectory, std::atomic_bool& cancelFlag, QObject* parent)
    : QObject(parent)
    , customConfigDirectory_(std::move(customConfigDirectory))
    , cancelled_(cancelFlag)
{
}

void SpeedTestWorker::runBatch(const Config& config, const QList<SpeedTestRequestItem>& items)
{
    int completed = 0;
    const Config probeConfigTemplate = SpeedTestServiceInternal::makeUrlTestRuntimeConfig(config);
    const QString urlTestUrl = RuntimeRunner::defaultUrlTestUrl(config);

    auto logCallback = [this](const QString& message) {
        QMetaObject::invokeMethod(this, [this, message]() {
            emit logGenerated(message);
        }, Qt::QueuedConnection);
    };

    auto emitResultCallback = [this, &completed](const QString& indexId, const QString& result) {
        ++completed;
        if (!cancelled_.load()) {
            emit testResultReady(indexId, result);
        }
    };

    // Group items by core program path so each group shares one core process.
    // Custom configs are handled separately because they cannot be merged into
    // a multi-outbound Xray config.
    QMap<QString, QList<int>> groupsByCore;
    QList<int> customIndices;
    QList<int> directProxyIndices;
    for (int i = 0; i < items.size(); ++i) {
        const SpeedTestRequestItem& item = items[i];
        if (item.server.configType == ConfigType::Custom) {
            customIndices.append(i);
            continue;
        }
        if (canProbeUpstreamProxyDirectly(item.server)) {
            directProxyIndices.append(i);
            continue;
        }

        const QFileInfo coreInfo(item.coreInfo.program);
        const QString canonical = coreInfo.canonicalFilePath();
        const QString key = canonical.isEmpty() ? item.coreInfo.program : canonical;
        groupsByCore[key].append(i);
    }

    for (int index : customIndices) {
        if (cancelled_.load()) {
            break;
        }
        const QString serverName = serverDisplayName(items[index].server);
        emit logGenerated(QStringLiteral("URL Test: %1").arg(serverName));
        const QString result = QStringLiteral("Unsupported");
        ++completed;
        emit testResultReady(items[index].server.indexId, result);
        emit logGenerated(QStringLiteral("URL Test result | %1 -> %2").arg(serverName, result));
    }

    runDirectProxyGroup(directProxyIndices, items, urlTestUrl, completed);

    for (auto it = groupsByCore.constBegin(); it != groupsByCore.constEnd(); ++it) {
        if (cancelled_.load()) {
            break;
        }

        const QList<int>& indices = it.value();
        QList<SpeedTestRequestItem> groupItems;
        groupItems.reserve(indices.size());
        for (int idx : indices) {
            groupItems.append(items[idx]);
            emit logGenerated(QStringLiteral("URL Test: %1").arg(serverDisplayName(items[idx].server)));
        }

        const bool batched = BatchRuntimeRunner::runBatchedGroup(
            probeConfigTemplate,
            urlTestUrl,
            groupItems,
            customConfigDirectory_,
            cancelled_,
            logCallback,
            emitResultCallback);

        if (batched) {
            continue;
        }

        runFallbackGroup(indices, items, probeConfigTemplate, urlTestUrl, completed);
    }

    const bool wasCancelled = cancelled_.load();
    emit batchFinished(wasCancelled
        ? QStringLiteral("URL Test cancelled after %1 server(s).").arg(completed)
        : QStringLiteral("URL Test finished for %1 server(s).").arg(completed));
}

void SpeedTestWorker::runDirectProxyGroup(
    const QList<int>& indices,
    const QList<SpeedTestRequestItem>& items,
    const QString& urlTestUrl,
    int& completed)
{
    for (int idx : indices) {
        if (cancelled_.load()) {
            break;
        }

        const SpeedTestRequestItem& item = items[idx];
        const QString serverName = serverDisplayName(item.server);
        emit logGenerated(QStringLiteral("URL Test direct proxy: %1").arg(serverName));
        const SpeedTestServiceInternal::UrlProbeResult probeResult =
            UrlProbe::probeUpstreamProxyWithRetry(
                item.server,
                urlTestUrl,
                kDirectProxyProbeTimeoutMs,
                cancelled_);
        const QString result = SpeedTestServiceInternal::formatUrlProbeResult(probeResult);
        ++completed;
        if (!cancelled_.load()) {
            emit testResultReady(item.server.indexId, result);
            emit logGenerated(QStringLiteral("URL Test result | %1 -> %2").arg(serverName, result));
        }
    }
}

void SpeedTestWorker::runFallbackGroup(
    const QList<int>& indices,
    const QList<SpeedTestRequestItem>& items,
    const Config& probeConfigTemplate,
    const QString& urlTestUrl,
    int& completed)
{
    struct PendingItem {
        QString indexId;
        QString serverName;
        std::future<QString> future;
    };

    const int itemCount = indices.size();
    const int maxConcurrency = std::max(1, std::min(kFallbackMaxConcurrency, itemCount));
    std::vector<PendingItem> pending;
    pending.reserve(static_cast<std::size_t>(maxConcurrency));

    auto emitCompletedResult = [this, &completed](PendingItem& pendingItem) {
        QString result;
        try {
            result = pendingItem.future.get();
        } catch (...) {
            result = QStringLiteral("Failed");
        }

        if (result.trimmed().isEmpty()) {
            result = QStringLiteral("Failed");
        }

        ++completed;
        if (!cancelled_.load()) {
            emit testResultReady(pendingItem.indexId, result);
            emit logGenerated(QStringLiteral("URL Test result | %1 -> %2")
                                  .arg(pendingItem.serverName, result));
        }
    };

    auto flushReadyTasks = [&pending, &emitCompletedResult]() {
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                emitCompletedResult(*it);
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
    };

    for (int idx : indices) {
        if (cancelled_.load()) {
            break;
        }

        const SpeedTestRequestItem item = items[idx];
        const QString serverName = serverDisplayName(item.server);

        pending.push_back(PendingItem{
            item.server.indexId,
            serverName,
            std::async(std::launch::async, [this, item, &probeConfigTemplate, &urlTestUrl]() -> QString {
                if (item.server.configType == ConfigType::Custom) {
                    return QStringLiteral("Unsupported");
                }
                if (cancelled_.load()) {
                    return QStringLiteral("Cancelled");
                }
                return RuntimeRunner::runUrlTest(
                    probeConfigTemplate,
                    urlTestUrl,
                    item,
                    customConfigDirectory_,
                    cancelled_,
                    [this](const QString& message) {
                        QMetaObject::invokeMethod(this, [this, message]() {
                            emit logGenerated(message);
                        }, Qt::QueuedConnection);
                    });
            })});

        while (!cancelled_.load() && pending.size() >= static_cast<std::size_t>(maxConcurrency)) {
            flushReadyTasks();
            if (pending.size() >= static_cast<std::size_t>(maxConcurrency)) {
                QThread::msleep(25);
            }
        }
    }

    while (!pending.empty()) {
        flushReadyTasks();
        if (!pending.empty()) {
            QThread::msleep(25);
        }
    }
}
