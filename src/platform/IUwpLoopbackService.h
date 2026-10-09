#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include "common/OperationResult.h"
#include "platform/UwpPackageInfo.h"

// The UWP loopback-exemption side effect, reduced to the operations its callers use.
//
// Same reasoning as ISystemProxyService and IAutoRunService: the implementation shells out to
// CheckNetIsolation.exe and, for the un-elevated path, waits on an elevated helper script, so
// none of its branches -- the slow one in particular -- can be produced on demand in a test.
// Callers take this interface instead, so a stand-in can hold a call open and the caller's
// behaviour while it runs becomes observable.
//
// Deliberately narrower than WindowsUwpLoopbackService: listExemptPackageFamilyNames() is
// called from inside the implementation's own listPackages(), so it is not part of the seam.
class IUwpLoopbackService {
public:
    virtual ~IUwpLoopbackService() = default;

    virtual bool isAvailable() const = 0;
    virtual QList<WindowsUwpPackageInfo> listPackages(OperationResult* result = nullptr) const = 0;
    virtual OperationResult setLoopbackEnabled(const QString& packageFamilyName, bool enabled) const = 0;
    virtual OperationResult setLoopbackEnabledElevated(
        const QHash<QString, bool>& enabledByPackageFamilyName) const = 0;
};
