#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// Polls a running core's Clash API for the member its auto-select group currently uses, and
// translates the group's "member-N" tag back into the server id whose config carries that
// position. Group activation only: the session exposes the endpoint (loopback port + secret)
// exactly for the lifetime of a group session, so a poller that outlives its core answers
// nothing and retries quietly until it is stopped.
//
// Lives on the GUI thread: the replies arrive on a persistent thread, so deleteLater() is the
// correct cleanup here (the guarded-delete discipline is for transient threads).
class ProxyGroupStatusPoller final : public QObject
{
    Q_OBJECT

public:
    explicit ProxyGroupStatusPoller(QObject* parent = nullptr);
    ~ProxyGroupStatusPoller() final;

    // Starts (or retargets) polling. memberIndexIds maps the group's member tags by position:
    // member-1 is the first entry. An empty list stops the poller.
    void start(int port, const QString& secret, const QStringList& memberIndexIds);
    void stop();

    bool isActive() const;
    QString currentMemberId() const;

signals:
    // The group's current member as a server id; empty while the core has not answered yet.
    void currentMemberChanged(const QString& indexId);

private:
    void pollNow();

    QNetworkAccessManager* networkManager_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    QStringList memberIndexIds_;
    QString currentMemberId_;
    QString baseUrl_;
    QString authorizationHeader_;
    // Bumped by every start()/stop(): a reply that comes back after a retarget belongs to a
    // different activation (the port is per-activation), so its answer must never be mapped
    // through the new member list. Same epoch = same target.
    quint32 pollEpoch_ = 0;
    bool requestInFlight_ = false;
};
