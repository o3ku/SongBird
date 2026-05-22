#include "app/SettingsDialogSession.h"

#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QVector>

#include "runtime/ProtocolCoreCompat.h"
#include "ui/dialogs/SettingsDialog.h"

SettingsDialogSession::SettingsDialogSession(
    QWidget* parent,
    TrackThreadFn trackThread,
    DetectCoreVersionFn detectCoreVersion,
    HandleCoreDownloadFn handleCoreDownload)
    : parent_(parent)
    , trackThread_(std::move(trackThread))
    , detectCoreVersion_(std::move(detectCoreVersion))
    , handleCoreDownload_(std::move(handleCoreDownload))
{
}

SettingsDialogSession::Result SettingsDialogSession::exec(
    const Config& config,
    int initialTab,
    const QList<CoreType>& existingCoreTypes,
    const std::weak_ptr<char>& lifetimeGuard) const
{
    struct ThreadJoiner {
        QVector<QPointer<QThread>> threads;

        ~ThreadJoiner()
        {
            const QVector<QPointer<QThread>> threadList = threads;
            for (const QPointer<QThread>& threadGuard : threadList) {
                QThread* thread = threadGuard.data();
                if (thread == nullptr) {
                    continue;
                }
                thread->requestInterruption();
                thread->quit();
                if (!thread->wait(3000)) {
                    thread->terminate();
                    thread->wait();
                }
            }
        }
    } threadJoiner;

    Result result;

    SettingsDialog dialog(parent_);
    QPointer<SettingsDialog> dialogGuard(&dialog);
    dialog.selectTab(initialTab);
    dialog.setConfig(config);
    dialog.setExistingCoreTypes(existingCoreTypes);

    for (const CoreType coreType : availableCoreTypes()) {
        auto* thread = QThread::create([coreType, dialogGuard, lifetimeGuard, detectCoreVersion = detectCoreVersion_]() {
            if (lifetimeGuard.expired()) {
                return;
            }

            const QString version = detectCoreVersion(coreType);
            if (!dialogGuard.isNull()) {
                QMetaObject::invokeMethod(dialogGuard.data(), [dialogGuard, coreType, version]() {
                    if (!dialogGuard.isNull()) {
                        dialogGuard->setCoreVersion(coreType, version);
                    }
                }, Qt::QueuedConnection);
            }
        });
        threadJoiner.threads.append(thread);
        trackThread_(thread);
        thread->start();
    }

    QObject::connect(&dialog, &SettingsDialog::coreDownloadRequested, &dialog, [this, dialogGuard](int coreTypeValue) {
        if (dialogGuard.isNull()) {
            return;
        }
        handleCoreDownload_(static_cast<CoreType>(coreTypeValue), dialogGuard);
    });

    if (dialog.exec() != QDialog::Accepted) {
        return result;
    }

    if (dialog.wasRestoreBackupRequested()) {
        result.outcome = Outcome::RestoreBackup;
        return result;
    }

    if (dialog.wasUpdateSubRequested()) {
        result.outcome = Outcome::UpdateSubscriptions;
        result.config = dialog.config();
        result.selectedSubscriptionRows = dialog.selectedSubRows();
        return result;
    }

    result.outcome = Outcome::Save;
    result.config = dialog.config();
    return result;
}
