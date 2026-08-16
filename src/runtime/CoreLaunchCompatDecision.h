#pragma once

#include <QList>
#include <QString>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/ProtocolCoreCompat.h"

enum class CoreLaunchCompatOutcome {
    // The core that will be launched can run the server's protocol.
    Compatible,
    // The core stored in settings for this protocol cannot run it, so the
    // launch would silently fall back to a different core.
    RequiresCoreSwitch,
    // No registered core supports the protocol at all.
    NoCompatibleCore
};

struct CoreLaunchCompatDecision {
    CoreLaunchCompatOutcome outcome = CoreLaunchCompatOutcome::Compatible;
    ConfigType configType = ConfigType::Unknown;
    // Core stored in settings for this protocol; Unknown when unset.
    CoreType storedCore = CoreType::Unknown;
    // Core the launch path would actually use.
    CoreType resolvedCore = CoreType::Unknown;

    bool requiresUserDecision() const
    {
        return outcome == CoreLaunchCompatOutcome::RequiresCoreSwitch;
    }
};

inline CoreLaunchCompatDecision evaluateCoreLaunchCompat(
    const Config& config,
    const VmessItem& server,
    const QList<CoreType>& existingCoreTypes)
{
    CoreLaunchCompatDecision decision;
    decision.configType = server.configType;

    // Custom configs carry their own core selection and are validated elsewhere.
    if (server.configType == ConfigType::Custom) {
        decision.resolvedCore = resolveSelectedCoreType(config, server, existingCoreTypes);
        return decision;
    }

    if (supportedCoreTypes(server.configType).isEmpty()) {
        decision.outcome = CoreLaunchCompatOutcome::NoCompatibleCore;
        return decision;
    }

    decision.storedCore = storedCoreTypeForProtocol(config, server.configType);
    decision.resolvedCore = resolveSelectedCoreType(config, server, existingCoreTypes);

    if (decision.storedCore != CoreType::Unknown
        && !protocolSupportsCore(server.configType, decision.storedCore)) {
        decision.outcome = CoreLaunchCompatOutcome::RequiresCoreSwitch;
    }

    return decision;
}

inline QString coreLaunchCompatFailureMessage(const CoreLaunchCompatDecision& decision)
{
    return QStringLiteral("No available core can run %1 servers.")
        .arg(configTypeDisplayName(decision.configType));
}

inline QString coreLaunchCompatSwitchPrompt(const CoreLaunchCompatDecision& decision)
{
    return QStringLiteral("%1 cannot run %2 servers.\n\nSwitch this protocol to %3?")
        .arg(coreTypeDisplayName(decision.storedCore))
        .arg(configTypeDisplayName(decision.configType))
        .arg(coreTypeDisplayName(decision.resolvedCore));
}
