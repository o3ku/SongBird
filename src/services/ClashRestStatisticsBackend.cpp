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

    running_.store(true);
    hasBaseline_.store(false);
    baselineUp_.store(0);
    baselineDown_.store(0);

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
    running_.store(false);
    hasBaseline_.store(false);
    baselineUp_.store(0);
    baselineDown_.store(0);
    updateAvailability(false);
}

void ClashRestStatisticsBackend::poll()
{
    if (!running_.load() || endpoint_.isEmpty() || networkManager_ == nullptr) {
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

    if (!running_.load()) {
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
    const quint64 totalUp = static_cast<quint64>(object.value(QStringLiteral("uploadTotal")).toVariant().toLongLong());
    const quint64 totalDown = static_cast<quint64>(object.value(QStringLiteral("downloadTotal")).toVariant().toLongLong());

    updateAvailability(true);

    if (!hasBaseline_.load()) {
        baselineUp_.store(totalUp);
        baselineDown_.store(totalDown);
        hasBaseline_.store(true);
        return;
    }

    const quint64 prevUp = baselineUp_.load();
    const quint64 prevDown = baselineDown_.load();
    quint64 deltaUp = 0;
    quint64 deltaDown = 0;
    if (totalUp >= prevUp) {
        deltaUp = totalUp - prevUp;
    }
    if (totalDown >= prevDown) {
        deltaDown = totalDown - prevDown;
    }
    baselineUp_.store(totalUp);
    baselineDown_.store(totalDown);

    if (deltaUp > 0 || deltaDown > 0) {
        emit trafficDeltaReceived(deltaUp, deltaDown);
    }
}

void ClashRestStatisticsBackend::updateAvailability(bool available)
{
    if (available_.load() == available) {
        return;
    }
    available_.store(available);
    emit pollingAvailabilityChanged(available);
}
