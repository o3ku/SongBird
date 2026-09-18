#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"

class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;

    // Returns the stored configuration. When loading fails (missing file is not
    // a failure; corrupt or unreadable content is), this returns a default
    // Config and lastLoadError() carries a non-empty message. Callers that
    // mutate and save the loaded config MUST check lastLoadError() first so a
    // corrupt file is never overwritten with an empty configuration.
    virtual Config load() = 0;

    // Persists the configuration and its companion state file. Both are written
    // even when one fails, so a state-file problem never hides that the primary
    // config was persisted (or vice versa); the return value is therefore true
    // only when *both* succeeded. lastSaveError() names the file and step that
    // failed so callers can report a specific reason instead of a generic one.
    virtual bool save(const Config& config) = 0;

    virtual QString lastLoadError() const = 0;
    virtual QString lastSaveError() const = 0;

    // Turns a save() that returned false into a reportable failure. `context`
    // describes what the app was doing ("Failed to save configuration after
    // adding the server."); the repository's specific reason -- which file, and
    // at which step -- is appended to it.
    //
    // This exists because every caller used to build its own generic message and
    // drop lastSaveError() on the floor, so the detail the repository carefully
    // recorded never reached a user. Keep using this instead of constructing the
    // message by hand. `context` alone is used when the repository has nothing to
    // add, which is the normal case for test doubles.
    OperationResult saveFailureResult(const QString& context) const
    {
        const QString reason = lastSaveError().trimmed();
        if (reason.isEmpty()) {
            return OperationResult::fail(context);
        }
        return OperationResult::fail(context.isEmpty()
            ? reason
            : QStringLiteral("%1 %2").arg(context, reason));
    }
};
