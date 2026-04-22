#include "services/ClashRestStatisticsBackend.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

ClashRestStatisticsBackend::ClashRestStatisticsBackend(QObject* parent)
    : QObject(parent)
{
    networkManager_ = new QNetworkAccessManager(this);
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::CoarseTimer);
    connect(timer_, &QTimer::timeout, this, &ClashRestStatisticsBackend::poll);
}

ClashRestStatisticsBackend::~ClashRestStatisticsBackend()
{
    stop();
}

OperationResult ClashRestStatisticsBackend::start(const QString& host, int port, int refreshRateSeconds)
{
    stop();

    const QString trimmedHost = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    endpoint_ = QStringLiteral("http://%1:%2/connections").arg(trimmedHost).arg(port);

    running_ = true;
    hasBaseline_ = false;
    baselineUp_ = 0;
    baselineDown_ = 0;

    const int intervalMs = qMax(1, refreshRateSeconds) * 1000;
    timer_->start(intervalMs);
    QTimer::singleShot(0, this, &ClashRestStatisticsBackend::poll);

    return OperationResult::ok(QStringLiteral("Clash REST statistics backend started on %1").arg(endpoint_));
}

void ClashRestStatisticsBackend::stop()
{
    if (timer_ != nullptr) {
        timer_->stop();
    }
    running_ = false;
    hasBaseline_ = false;
    baselineUp_ = 0;
    baselineDown_ = 0;
    updateAvailability(false);
}

void ClashRestStatisticsBackend::poll()
{
    if (!running_ || endpoint_.isEmpty() || networkManager_ == nullptr) {
        return;
    }

    QNetworkRequest request((QUrl(endpoint_)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = networkManager_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleReply(reply);
    });
}

void ClashRestStatisticsBackend::handleReply(QNetworkReply* reply)
{
    if (reply == nullptr) {
        return;
    }
    reply->deleteLater();

    if (!running_) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        updateAvailability(false);
        return;
    }

    const QByteArray payload = reply->readAll();
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        updateAvailability(false);
        return;
    }

    const QJsonObject object = document.object();
    const quint64 totalUp = static_cast<quint64>(object.value(QStringLiteral("uploadTotal")).toDouble(0.0));
    const quint64 totalDown = static_cast<quint64>(object.value(QStringLiteral("downloadTotal")).toDouble(0.0));

    updateAvailability(true);

    if (!hasBaseline_) {
        baselineUp_ = totalUp;
        baselineDown_ = totalDown;
        hasBaseline_ = true;
        return;
    }

    quint64 deltaUp = 0;
    quint64 deltaDown = 0;
    if (totalUp >= baselineUp_) {
        deltaUp = totalUp - baselineUp_;
    }
    if (totalDown >= baselineDown_) {
        deltaDown = totalDown - baselineDown_;
    }
    baselineUp_ = totalUp;
    baselineDown_ = totalDown;

    if (deltaUp > 0 || deltaDown > 0) {
        emit trafficDeltaReceived(deltaUp, deltaDown);
    }
}

void ClashRestStatisticsBackend::updateAvailability(bool available)
{
    if (available_ == available) {
        return;
    }
    available_ = available;
    emit pollingAvailabilityChanged(available);
}
