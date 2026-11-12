#pragma once

#include <functional>

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "app/AutoSwitchGroupLogic.h"
#include "common/OperationResult.h"
#include "services/SpeedTestRequestItem.h"

// The orchestration half of "auto switch in group". The pure decisions live in
// AutoSwitchGroupLogic; this class owns the state the decisions read (the warm probe results,
// the consecutive-switch counter, the pending-failover-probe flag), the warm-probe timer, and
// the sequencing of the failure path:
//
//   session failure
//     -> warm data names a fresh reachable member: switch now (no probe on the failure path)
//     -> no warm target: probe the group's rankable members once, then switch to the best
//        answer, or to the round-robin neighbour when no probe answer is reachable
//     -> probe cannot start (busy): round-robin neighbour, the behaviour the feature has
//        always had
//     -> every member has failed once in this streak: stop and say so
//
// Any successful activation resets the streak. While a session is proxying, a low-frequency
// warm round keeps the group's top candidates measured so the common failure switches in one
// core restart with no probe delay.
class AutoSwitchCoordinator final : public QObject {
    Q_OBJECT

public:
    // Starts one probe batch and reports its completion through onFinished (exactly once, on
    // the GUI thread). completionContext is the QObject whose destruction must cancel the
    // completion (Qt connection semantics); returns false when the batch could not start.
    // Injected rather than calling SpeedTestController directly so a test can script the probe
    // without cores or worker threads -- the same reason ProxyHealthWatchdog takes an
    // AvailabilityCheck instead of a probe service.
    using ProbeRunner = std::function<
        bool(QObject* completionContext,
             const QList<SpeedTestRequestItem>& items,
             std::function<void()> onFinished)>;

    struct Dependencies {
        // The "auto switch in group" preference and the group the session is switching within
        // (the running target: the active group id, or the active node's own location group),
        // read at decision time -- both can change between two failures. Deliberately not the
        // location view's selection: the user browsing another tab must not disable the feature.
        std::function<bool()> autoSwitchEnabled;
        std::function<QString()> currentLocationId;
        std::function<QString()> currentServerId;
        std::function<QList<VmessItem>()> servers;
        // A warm round only runs while a session is proxying; a failover switch only runs
        // while nothing is proxying or activating (the user acting first wins).
        std::function<bool()> sessionProxying;
        std::function<bool()> proxyOccupied;
        // Builds the probe batch for the given ids (resolving the launch core and its info);
        // the ids are pre-filtered to probe-rankable members.
        std::function<QList<SpeedTestRequestItem>(const QStringList&)> buildProbeItems;
        ProbeRunner runProbe;
        // Writes the default server and starts the proxy -- AppBootstrap's existing pair from
        // the pre-coordinator implementation.
        std::function<void(const QString&)> switchToServer;
        std::function<void(const OperationResult&)> appendResult;
    };

    explicit AutoSwitchCoordinator(Dependencies dependencies, QObject* parent = nullptr);

    // ProxySession::failed: the session is already torn down when this runs.
    void handleSessionFailure();

    // ProxySession::activated: ends any failure streak and kicks a warm round so the data is
    // fresh for as long as this session lives.
    void noteSessionActivated();

    // SpeedTestController::testResultReady, connected by the wiring: every finished probe
    // measurement -- the warm round's, the failover probe's, and the user's own "test servers"
    // batch alike -- refreshes the warm data.
    void handleProbeResultReady(const QString& indexId, const QString& result);

private:
    void runWarmRound();
    void startFailoverProbe(const AutoSwitchGroupLogic::GroupScan& scan);
    void finishFailoverProbe();
    void executeSwitch(const QString& indexId, const QString& reason);
    AutoSwitchGroupLogic::GroupScan scanCurrentGroup() const;
    // The log lines name servers the way the rest of the app does; the id alone is the
    // last-resort fallback for a server that vanished between the scan and the message.
    QString displayNameFor(const QString& indexId) const;
    Dependencies deps_;
    QTimer warmTimer_;
    AutoSwitchGroupLogic::WarmProbeMap warm_;
    int consecutiveSwitches_ = 0;
    bool failoverProbePending_ = false;
};
