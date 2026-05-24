#pragma once

#include <functional>

#include <QString>
#include <QStringList>

class AppUpdateInstallService {
public:
    using ElevatedLauncher = std::function<bool(const QString& program, const QStringList& arguments)>;

    explicit AppUpdateInstallService(ElevatedLauncher elevatedLauncher = {});

    bool launchUpdateScript(const QString& newExecutablePath) const;

private:
    ElevatedLauncher elevatedLauncher_;
};
