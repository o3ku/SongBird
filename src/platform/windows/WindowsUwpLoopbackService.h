#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QHash>

#include "common/OperationResult.h"
#include "platform/IUwpLoopbackService.h"

class WindowsUwpLoopbackService : public IUwpLoopbackService {
public:
    bool isAvailable() const override;
    QList<WindowsUwpPackageInfo> listPackages(OperationResult* result = nullptr) const override;
    // Not part of IUwpLoopbackService: only listPackages() below calls it.
    QSet<QString> listExemptPackageFamilyNames(OperationResult* result = nullptr) const;
    OperationResult setLoopbackEnabled(const QString& packageFamilyName, bool enabled) const override;
    OperationResult setLoopbackEnabledElevated(
        const QHash<QString, bool>& enabledByPackageFamilyName) const override;

private:
    OperationResult runProcess(const QString& program, const QStringList& arguments, QString* output) const;
    OperationResult runElevatedScript(const QString& scriptPath) const;
    QString checkNetIsolationPath() const;
};
