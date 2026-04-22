#include "services/GrpcStatisticsBackend.h"

#include <algorithm>
#include <chrono>

#include "services/GrpcStatisticsParser.h"

#if QT_V2RAYN_HAS_GRPC_STATISTICS
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "Statistics.grpc.pb.h"
#endif

GrpcStatisticsBackend::GrpcStatisticsBackend(QObject* parent)
    : QObject(parent)
{
}

GrpcStatisticsBackend::~GrpcStatisticsBackend()
{
    stop();
}

bool GrpcStatisticsBackend::isSupported() const
{
#if QT_V2RAYN_HAS_GRPC_STATISTICS
    return true;
#else
    return false;
#endif
}

bool GrpcStatisticsBackend::isRunning() const
{
    return running_.load();
}

OperationResult GrpcStatisticsBackend::start(const QString& host, int port, int refreshRateSeconds)
{
    stop();

    if (!isSupported()) {
        return OperationResult::ok(QStringLiteral("This build does not include gRPC statistics polling."));
    }

    const QString trimmedHost = host.trimmed();
    if (trimmedHost.isEmpty() || port <= 0) {
        return OperationResult::fail(QStringLiteral("gRPC statistics polling requires a valid loopback endpoint."));
    }

    running_.store(true);
    stopRequested_.store(false);

    const QString endpoint = QStringLiteral("%1:%2").arg(trimmedHost).arg(port);
    worker_ = std::thread([this, endpoint, refreshRateSeconds]() {
        runWorker(endpoint, refreshRateSeconds);
    });

    return OperationResult::ok(QStringLiteral("gRPC statistics polling started on %1.").arg(endpoint));
}

void GrpcStatisticsBackend::stop()
{
    stopRequested_.store(true);
    stopCondition_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }

    running_.store(false);
    updateAvailability(false);
    stopRequested_.store(false);
}

void GrpcStatisticsBackend::updateAvailability(bool available)
{
    if (pollingAvailable_.exchange(available) == available) {
        return;
    }

    emit pollingAvailabilityChanged(available);
}

void GrpcStatisticsBackend::runWorker(QString endpoint, int refreshRateSeconds)
{
#if QT_V2RAYN_HAS_GRPC_STATISTICS
    namespace stats = v2ray::core::app::stats::command;

    const int intervalSeconds = std::max(refreshRateSeconds, 1);
    const auto requestTimeout = std::chrono::milliseconds(std::clamp(intervalSeconds * 500, 500, 2000));
    const std::shared_ptr<grpc::Channel> channel =
        grpc::CreateChannel(endpoint.toStdString(), grpc::InsecureChannelCredentials());
    std::unique_ptr<stats::StatsService::Stub> stub = stats::StatsService::NewStub(channel);

    while (!stopRequested_.load()) {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + requestTimeout);

        stats::QueryStatsRequest request;
        request.set_pattern("");
        request.set_reset(true);

        stats::QueryStatsResponse response;
        const grpc::Status status = stub->QueryStats(&context, request, &response);
        if (status.ok()) {
            QList<StatisticsCounterEntry> counters;
            counters.reserve(response.stat_size());
            for (const auto& stat : response.stat()) {
                counters.append(StatisticsCounterEntry{
                    QString::fromStdString(stat.name()),
                    stat.value()});
            }

            const GrpcTrafficDelta traffic = GrpcStatisticsParser::parseCounters(counters);
            updateAvailability(true);
            emit trafficDeltaReceived(traffic.up, traffic.down);
        } else {
            updateAvailability(false);
        }

        std::unique_lock<std::mutex> lock(stopMutex_);
        stopCondition_.wait_for(
            lock,
            std::chrono::seconds(intervalSeconds),
            [this]() {
                return stopRequested_.load();
            });
    }
#else
    Q_UNUSED(endpoint)
    Q_UNUSED(refreshRateSeconds)
#endif

    running_.store(false);
}
