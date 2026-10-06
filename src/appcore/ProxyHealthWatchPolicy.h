#pragma once

#include <QString>

// Pure decision logic for the main app's node health watch.
//
// Why it exists: SongBird.exe could not tell that the running core was alive while its active node
// had gone dark. In the log that motivated this, an AnyTLS node was dead for 2 h 39 m and the app
// showed nothing at all -- 183 core ERROR lines were the only trace, and the user had no way to
// know the node, rather than the site, was the problem. SongBirdAuto has health-checked its active
// node since it was written (SongBirdAutoCoordinator), but the main app never did.
//
// This class holds no timer, no thread and no I/O. It turns a stream of probe results into
// "show a warning" / "clear the warning" decisions, so the ProxySession wiring stays thin and the
// policy itself stays unit-testable -- the same split ProxyCrashRestartPolicy uses.
//
// The one deliberate asymmetry: the first failure does NOT warn. The core dials the node per
// connection, so a single missed probe is ordinary jitter that heals itself with no user action;
// warning on it would only train the user to ignore the status bar. Two consecutive failures is
// the default bar.
class ProxyHealthWatchPolicy final
{
public:
    enum class Action {
        None,  // nothing to show; stay silent
        Warn,  // the node has failed enough consecutive probes to be worth reporting
        Clear  // a warning that was shown is no longer true
    };

    struct Decision {
        Action action = Action::None;
        QString message;
    };

    static constexpr int kDefaultFailuresBeforeWarn = 2;

    // A threshold below 1 is treated as 1, i.e. warn on the first failure. That is a legitimate
    // setting for a caller that wants no tolerance, so it is clamped rather than rejected.
    explicit ProxyHealthWatchPolicy(int failuresBeforeWarn = kDefaultFailuresBeforeWarn);

    // `failureReason` is the caller's own diagnostic for the failed probe (for the availability
    // probe, "Availability check: -1 ms"). It is appended to the warning verbatim when non-empty
    // and ignored otherwise, so a caller with nothing to add does not leave a dangling separator.
    Decision recordResult(bool reachable, const QString& failureReason);

    void reset();

    int failuresBeforeWarn() const { return failuresBeforeWarn_; }
    int consecutiveFailures() const { return consecutiveFailures_; }
    bool warningShown() const { return warningShown_; }

private:
    int failuresBeforeWarn_ = kDefaultFailuresBeforeWarn;
    int consecutiveFailures_ = 0;
    bool warningShown_ = false;
};
