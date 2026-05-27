#pragma once

#include <functional>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include "app/BackgroundTaskCoordinator.h"
#include "app/IUserFeedback.h"
#include "common/OperationResult.h"
#include "services/AppUpdateService.h"

class QThread;
class QWidget;

class AppUpdateCheckCoordinator : public QObject {
    Q_OBJECT

public:
    using CheckForUpdate = std::function<OperationResult(
        const QString& currentVersion,
        bool allowPrerelease,
        AppUpdateCheckResult* result)>;
    using DownloadUpdate = std::function<OperationResult(
        const QUrl& downloadUrl,
        const QString& assetName,
        const QString& targetDirectory,
        QString* savedPath)>;
    using ThreadRunner = std::function<void(std::function<void()> task)>;
    using UiInvoker = std::function<void(QObject* context, std::function<void()> callback)>;

    struct Dependencies {
        BackgroundTaskCoordinator* backgroundTasks = nullptr;
        IUserFeedback* feedback = nullptr;
        std::function<QString()> currentVersion;
        std::function<bool()> allowPrerelease;
        std::function<QString()> applicationDirectory;
        std::function<bool()> isShuttingDown;
        std::function<void(QThread*)> trackBackgroundThread;
        CheckForUpdate checkForUpdate;
        DownloadUpdate downloadUpdate;
        ThreadRunner runInBackground;
        UiInvoker invokeOnUiThread;
    };

    explicit AppUpdateCheckCoordinator(Dependencies dependencies, QObject* parent = nullptr);

    bool isCheckRunning() const;
    void checkAppUpdates(bool manual);
    void downloadAppUpdate(const AppUpdateCheckResult& updateResult, QPointer<QWidget> dialogParent = {});
    void downloadLatestAvailableUpdate(QPointer<QWidget> dialogParent = {});

signals:
    void updateAvailable(const AppUpdateCheckResult& result, const QString& message);
    void updateUnavailable();

private:
    void runInBackground(std::function<void()> task);
    void invokeOnUiThread(QObject* context, std::function<void()> callback);
    QWidget* resolveMessageParent(const QPointer<QWidget>& dialogParent) const;

    Dependencies deps_;
    AppUpdateCheckResult latestAvailableUpdate_;
    bool checkRunning_ = false;
};
