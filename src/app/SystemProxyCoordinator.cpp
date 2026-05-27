#include "app/SystemProxyCoordinator.h"

#include <optional>

#include <utility>

#include <QStringList>

#include "app/ProxySession.h"
#include "platform/windows/WindowsSystemProxyService.h"
#include "runtime/RoutingConfigFragments.h"
#include "services/ServerService.h"

namespace {

constexpr auto DefaultIeProxyExceptions =
    "localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;"
    "172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*";

QStringList splitProxyExceptions(const QString& value)
{
    QString normalized = value;
    normalized.replace(QChar(','), QChar(';'));

    QStringList parts;
    for (const QString& item : normalized.split(QChar(';'), Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty()) {
            parts.append(trimmed);
        }
    }
    return parts;
}

void appendUniqueProxyException(QStringList& entries, const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    for (const QString& existing : std::as_const(entries)) {
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    entries.append(trimmed);
}

QStringList collectRouteDerivedProxyExceptions(const Config& config)
{
    QList<RoutingRule> effectiveRules;
    for (const RoutingRule& rule : config.collection().routingCustomRules) {
        if (rule.enabled) {
            effectiveRules.append(rule);
        }
    }

    const std::optional<RoutingItem> selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    if (selectedRouting.has_value()) {
        for (const RoutingRule& rule : selectedRouting->rules) {
            if (rule.enabled) {
                effectiveRules.append(rule);
            }
        }
    }

    QStringList derived;
    for (const RoutingRule& rule : std::as_const(effectiveRules)) {
        if (rule.outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        for (const QString& domainValue : rule.domain) {
            const QString domain = domainValue.trimmed();
            if (domain.startsWith(QStringLiteral("domain:"), Qt::CaseInsensitive)) {
                const QString suffix = domain.mid(QStringLiteral("domain:").size()).trimmed();
                if (suffix.isEmpty()) {
                    continue;
                }
                appendUniqueProxyException(derived, suffix);
                appendUniqueProxyException(derived, QStringLiteral("*.%1").arg(suffix));
                continue;
            }

            if (domain.startsWith(QStringLiteral("full:"), Qt::CaseInsensitive)) {
                appendUniqueProxyException(derived, domain.mid(QStringLiteral("full:").size()).trimmed());
            }
        }
    }

    return derived;
}

} // namespace

SystemProxyCoordinator::SystemProxyCoordinator(Dependencies deps, Callbacks callbacks)
    : deps_(deps)
    , callbacks_(std::move(callbacks))
{
}

bool SystemProxyCoordinator::saveMode(SystemProxyMode mode)
{
    const int previousValue = deps_.config.sysProxyType;
    const bool previousMainProxyEnabled = deps_.config.ui().mainProxyEnabled;
    deps_.config.sysProxyType = toLegacySystemProxyModeValue(mode);
    deps_.config.ui().mainProxyEnabled = mode == SystemProxyMode::ForcedChange;
    if (!deps_.serverService.save(deps_.config)) {
        deps_.config.sysProxyType = previousValue;
        deps_.config.ui().mainProxyEnabled = previousMainProxyEnabled;
        appendResult(OperationResult::fail(QStringLiteral("Failed to save the selected system proxy mode.")));
        syncStatusIndicators();
        return false;
    }

    return true;
}

void SystemProxyCoordinator::setMode(
    SystemProxyMode mode,
    bool skipTunCleanup,
    bool immediateStop,
    bool showStartupOverlay)
{
    if (deps_.proxySession == nullptr) {
        return;
    }

    const bool coreProcessRunning = deps_.proxySession->isCoreRunning();
    const bool coreReady = deps_.proxySession->isActive();
    const bool coreActivationPending = deps_.proxySession->isActivationInProgress();
    if (mode == SystemProxyMode::ForcedChange
        && !coreReady
        && coreActivationPending) {
        if (showStartupOverlay) {
            deps_.proxySession->requestChecklistOverlay();
        }
        appendResult(OperationResult::ok(QStringLiteral("Core start is already in progress.")));
        syncStatusIndicators();
        return;
    }

    if (!saveMode(mode)) {
        return;
    }

    if (mode == SystemProxyMode::ForcedChange) {
        if (coreReady) {
            const bool proxyApplied = updateMode(mode);
            if (proxyApplied) {
                deps_.proxySession->adoptManagedSystemProxy(true);
            }
            appendResult(proxyApplied
                ? OperationResult::ok(QStringLiteral("System proxy mode set to %1.").arg(systemProxyModeDisplayName(mode)))
                : OperationResult::fail(QStringLiteral("Failed to apply system proxy mode %1.").arg(systemProxyModeDisplayName(mode))));
            syncStatusIndicators();
            return;
        }
        appendResult(OperationResult::ok(
            QStringLiteral("System proxy mode set to %1. It will be applied after the core starts.")
                .arg(systemProxyModeDisplayName(mode))));
        appendResult(OperationResult::ok(QStringLiteral("Starting the active core because Global system proxy mode was enabled.")));
        startManagedProxyCore(skipTunCleanup, showStartupOverlay);
        return;
    }

    const bool shouldStop = coreProcessRunning || coreActivationPending;
    const bool proxyApplied = updateMode(mode);
    if (proxyApplied && mode == SystemProxyMode::ForcedClear) {
        deps_.proxySession->adoptManagedSystemProxy(false);
    }
    appendResult(proxyApplied
        ? OperationResult::ok(QStringLiteral("System proxy mode set to %1.").arg(systemProxyModeDisplayName(mode)))
        : OperationResult::fail(QStringLiteral("Failed to apply system proxy mode %1.").arg(systemProxyModeDisplayName(mode))));
    if (shouldStop) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping the active core because system proxy was cleared.")));
        deps_.proxySession->stop(immediateStop);
        return;
    }

    syncStatusIndicators();
}

void SystemProxyCoordinator::clearStateAfterCoreStopped()
{
    if (deps_.config.sysProxyType == toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear)
        && !deps_.config.ui().mainProxyEnabled) {
        return;
    }

    saveMode(SystemProxyMode::ForcedClear);
}

void SystemProxyCoordinator::applyModeOnExit(bool windowsShutdown)
{
    if (exitProxyStateApplied_ || deps_.systemProxyService == nullptr) {
        return;
    }

    exitProxyStateApplied_ = true;
    const bool managedSystemProxyActive =
        deps_.proxySession != nullptr && deps_.proxySession->isManagedProxyActive();
    if (windowsShutdown) {
        if (!managedSystemProxyActive) {
            return;
        }
        deps_.systemProxyService->resetOnShutdown();
        deps_.proxySession->adoptManagedSystemProxy(false);
        return;
    }

    if (!managedSystemProxyActive) {
        return;
    }

    const SystemProxyMode mode = resolveSystemProxyModeOnExit(
        normalizeSystemProxyMode(deps_.config.sysProxyType),
        windowsShutdown);
    updateMode(mode);
    deps_.proxySession->adoptManagedSystemProxy(expectedSystemProxyEnabled(mode));
}

void SystemProxyCoordinator::cleanupRuntimeForExit(bool windowsShutdown)
{
    applyModeOnExit(windowsShutdown);
    if (deps_.proxySession != nullptr
        && (deps_.proxySession->isCoreRunning() || deps_.proxySession->isTransitioning())) {
        deps_.proxySession->stop(true);
    }
    syncStatusIndicators();
}

void SystemProxyCoordinator::enable(bool showStartupOverlay)
{
    setMode(SystemProxyMode::ForcedChange, false, false, showStartupOverlay);
}

void SystemProxyCoordinator::retryStartup(bool showStartupOverlay)
{
    enable(showStartupOverlay);
}

void SystemProxyCoordinator::disable()
{
    setMode(SystemProxyMode::ForcedClear);
}

bool SystemProxyCoordinator::updateMode(SystemProxyMode mode) const
{
    return deps_.systemProxyService == nullptr
        || deps_.systemProxyService->update(
            mode,
            deps_.config.localPort + 1,
            deps_.config.localPort,
            buildExceptions(),
            deps_.config.systemProxyAdvancedProtocol);
}

QString SystemProxyCoordinator::buildExceptions() const
{
    QStringList entries = splitProxyExceptions(
        deps_.config.defaults().defIeProxyExceptions.trimmed().isEmpty()
            ? QString::fromUtf8(DefaultIeProxyExceptions)
            : deps_.config.defaults().defIeProxyExceptions.trimmed());

    for (const QString& item : collectRouteDerivedProxyExceptions(deps_.config)) {
        appendUniqueProxyException(entries, item);
    }

    return entries.join(QStringLiteral(";"));
}

void SystemProxyCoordinator::appendResult(const OperationResult& result) const
{
    if (callbacks_.appendResult) {
        callbacks_.appendResult(result);
    }
}

void SystemProxyCoordinator::syncStatusIndicators() const
{
    if (callbacks_.syncStatusIndicators) {
        callbacks_.syncStatusIndicators();
    }
}

void SystemProxyCoordinator::startManagedProxyCore(bool skipTunCleanup, bool showStartupOverlay) const
{
    if (callbacks_.startManagedProxyCore) {
        callbacks_.startManagedProxyCore(skipTunCleanup, showStartupOverlay);
    }
}
