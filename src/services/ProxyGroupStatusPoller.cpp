#include "services/ProxyGroupStatusPoller.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include "runtime/ProxyGroupTags.h"

namespace {

// The front end has no reason to chase sub-second switches: the core picks a member and stays
// on it, so a steady cadence keeps the UI honest without leaning on the API.
constexpr int PollIntervalMs = 5 * 1000;

// A reply that never comes (a foreign process squatting on the recycled port and accepting
// without answering) must not freeze the cadence: without a deadline, requestInFlight_ stays
// true forever and the poller goes silent for the rest of the session.
constexpr int RequestTimeoutMs = 5 * 1000;

int memberIndexFromTag(const QString& tag)
{
    const QString prefix = QStringLiteral("member-");
    if (!tag.startsWith(prefix)) {
        return 0;
    }
    bool ok = false;
    const int index = tag.mid(prefix.size()).toInt(&ok);
    return ok ? index : 0;
}

} // namespace

ProxyGroupStatusPoller::ProxyGroupStatusPoller(QObject* parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
    , pollTimer_(new QTimer(this))
{
    pollTimer_->setInterval(PollIntervalMs);
    connect(pollTimer_, &QTimer::timeout, this, &ProxyGroupStatusPoller::pollNow);
}

ProxyGroupStatusPoller::~ProxyGroupStatusPoller()
{
    stop();
}

void ProxyGroupStatusPoller::start(int port, const QString& secret, const QStringList& memberIndexIds)
{
    if (port <= 0 || memberIndexIds.isEmpty()) {
        stop();
        return;
    }

    const bool sameTarget = baseUrl_ == QStringLiteral("http://127.0.0.1:%1").arg(port)
        && memberIndexIds_ == memberIndexIds;
    if (sameTarget && isActive()) {
        return;
    }

    memberIndexIds_ = memberIndexIds;
    baseUrl_ = QStringLiteral("http://127.0.0.1:%1").arg(port);
    authorizationHeader_ = QStringLiteral("Bearer %1").arg(secret.trimmed());
    currentMemberId_.clear();
    // A request from the previous target may still be in flight; it answers to a different
    // activation now, and the epoch bump below retires it.
    requestInFlight_ = false;
    ++pollEpoch_;
    pollTimer_->start();
    pollNow();
}

void ProxyGroupStatusPoller::stop()
{
    pollTimer_->stop();
    baseUrl_.clear();
    authorizationHeader_.clear();
    memberIndexIds_.clear();
    ++pollEpoch_;
    if (!currentMemberId_.isEmpty()) {
        currentMemberId_.clear();
        emit currentMemberChanged(QString());
    }
}

bool ProxyGroupStatusPoller::isActive() const
{
    return pollTimer_->isActive() && !baseUrl_.isEmpty();
}

QString ProxyGroupStatusPoller::currentMemberId() const
{
    return currentMemberId_;
}

void ProxyGroupStatusPoller::pollNow()
{
    if (baseUrl_.isEmpty() || requestInFlight_) {
        return;
    }

    QNetworkRequest request(QUrl(baseUrl_ + QStringLiteral("/proxies/proxy")));
    request.setTransferTimeout(RequestTimeoutMs);
    if (!authorizationHeader_.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("Authorization"), authorizationHeader_.toUtf8());
    }
    const quint32 requestEpoch = pollEpoch_;

    requestInFlight_ = true;
    QNetworkReply* reply = networkManager_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestEpoch]() {
        reply->deleteLater();
        requestInFlight_ = false;
        // Not just isActive(): a retarget or a stop happened since the request left, so the
        // answer -- however well formed -- describes a different activation's group and must
        // not be mapped through this target's member list.
        if (!isActive() || requestEpoch != pollEpoch_) {
            return;
        }

        const QByteArray payload = reply->error() == QNetworkReply::NoError
            ? reply->readAll()
            : QByteArray();
        if (payload.isEmpty()) {
            return;
        }

        const QJsonObject group = QJsonDocument::fromJson(payload).object();
        const int memberIndex = memberIndexFromTag(group.value(QStringLiteral("now")).toString());
        if (memberIndex < 1 || memberIndex > memberIndexIds_.size()) {
            return;
        }

        const QString memberId = memberIndexIds_.at(memberIndex - 1);
        if (memberId == currentMemberId_) {
            return;
        }
        currentMemberId_ = memberId;
        emit currentMemberChanged(currentMemberId_);
    });
}
