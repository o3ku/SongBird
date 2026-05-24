#pragma once

#include <functional>
#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>

#include "app/BackgroundTaskCoordinator.h"
#include "common/OperationResult.h"

class QThread;
class QWidget;

class GeoResourceUpdateCoordinator final : public QObject
{
    Q_OBJECT

public:
    struct Dependencies {
        BackgroundTaskCoordinator* backgroundTasks = nullptr;
        std::function<QString()> targetDirectory;
        std::function<QObject*()> uiContext;
        std::function<QWidget*()> dialogParent;
        std::function<std::weak_ptr<char>()> lifetimeGuard;
        std::function<bool()> isAvailable;
        std::function<void(QThread*)> trackBackgroundThread;
        std::function<void(const QString&)> appendLog;
        std::function<void(const QString&, int)> showRoutineTransientStatus;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void(QWidget*, const QString&, const QString&)> showWarning;
        std::function<void(QWidget*, const QString&, const QString&)> showInformation;
        std::function<void(const QString&)> restartCoreIfRunning;
    };

    explicit GeoResourceUpdateCoordinator(Dependencies dependencies, QObject* parent = nullptr);

    void updateGeoResources();

private:
    Dependencies deps_;
};
