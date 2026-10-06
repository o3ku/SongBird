#include "appcore/ProxyHealthWatchPolicy.h"

#include <QCoreApplication>

namespace {

// One complete sentence rather than a frame around a fragment, for the reason spelled out at
// length in ProxyCrashRestartPolicy.cpp: a translator must own the whole sentence, because word
// order moves the count relative to the rest of it. The count is always >= 2 here -- the first
// failure never warns -- so the English plural reads correctly without a numerus form.
QString warningMessage(int consecutiveFailures, const QString& failureReason)
{
    QString message = QCoreApplication::translate(
                          "ProxySession",
                          "Node unavailable: %1 consecutive health checks failed.")
                          .arg(consecutiveFailures);

    const QString reason = failureReason.trimmed();
    if (!reason.isEmpty()) {
        // House style: context + " " + reason. The probe's own message is already a standalone
        // sentence, so it is appended rather than substituted into a slot.
        message += QLatin1Char(' ') + reason;
    }
    return message;
}

QString clearedMessage()
{
    return QCoreApplication::translate(
        "ProxySession", "Node reachable again; clearing the availability warning.");
}

} // namespace

ProxyHealthWatchPolicy::ProxyHealthWatchPolicy(int failuresBeforeWarn)
    : failuresBeforeWarn_(failuresBeforeWarn < 1 ? 1 : failuresBeforeWarn)
{
}

ProxyHealthWatchPolicy::Decision ProxyHealthWatchPolicy::recordResult(
    bool reachable, const QString& failureReason)
{
    if (reachable) {
        const bool hadWarning = warningShown_;
        consecutiveFailures_ = 0;
        warningShown_ = false;
        if (hadWarning) {
            return Decision{Action::Clear, clearedMessage()};
        }
        return Decision{Action::None, QString()};
    }

    ++consecutiveFailures_;

    // Already warned: keep counting, because the count is useful for diagnostics, but stay silent.
    // Re-warning on every failed probe would only make the status bar repeat the same line.
    if (warningShown_ || consecutiveFailures_ < failuresBeforeWarn_) {
        return Decision{Action::None, QString()};
    }

    warningShown_ = true;
    return Decision{Action::Warn, warningMessage(consecutiveFailures_, failureReason)};
}

void ProxyHealthWatchPolicy::reset()
{
    consecutiveFailures_ = 0;
    warningShown_ = false;
}
