#pragma once

#include <functional>
#include <memory>

#include <QList>
#include <QPointer>
#include <QString>

#include "domain/models/Config.h"

class QThread;
class QWidget;
class SettingsDialog;

class SettingsDialogRunner
{
public:
    enum class Outcome {
        Cancelled,
        RestoreBackup,
        UpdateSubscriptions,
        Save
    };

    struct Result {
        Outcome outcome = Outcome::Cancelled;
        Config config;
        QList<int> selectedSubscriptionRows;
    };

    using TrackThreadFn = std::function<void(QThread*)>;
    using DetectCoreVersionFn = std::function<QString(CoreType)>;
    using HandleCoreDownloadFn = std::function<void(CoreType, QPointer<SettingsDialog>)>;

    SettingsDialogRunner(
        QWidget* parent,
        TrackThreadFn trackThread,
        DetectCoreVersionFn detectCoreVersion,
        HandleCoreDownloadFn handleCoreDownload);

    Result exec(
        const Config& config,
        int initialTab,
        const QList<CoreType>& existingCoreTypes,
        const std::weak_ptr<char>& lifetimeGuard) const;

private:
    QWidget* parent_ = nullptr;
    TrackThreadFn trackThread_;
    DetectCoreVersionFn detectCoreVersion_;
    HandleCoreDownloadFn handleCoreDownload_;
};
