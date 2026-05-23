#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QHash>

#include "common/OperationResult.h"

struct WindowsUwpPackageInfo {
    QString name;
    QString packageFamilyName;
    QString packageFullName;
    QString publisher;
    QString installLocation;
    bool loopbackEnabled = false;
};

class WindowsUwpLoopbackService {
public:
    bool isAvailable() const;
    QList<WindowsUwpPackageInfo> listPackages(OperationResult* result = nullptr) const;
    QSet<QString> listExemptPackageFamilyNames(OperationResult* result = nullptr) const;
    OperationResult setLoopbackEnabled(const QString& packageFamilyName, bool enabled) const;
    OperationResult setLoopbackEnabledElevated(const QHash<QString, bool>& enabledByPackageFamilyName) const;

private:
    OperationResult runProcess(const QString& program, const QStringList& arguments, QString* output) const;
    OperationResult runElevatedScript(const QString& scriptPath) const;
    QString checkNetIsolationPath() const;
};
