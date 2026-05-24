#include "app/GeoResourceUpdateCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include "services/GeoResourceUpdateService.h"

namespace {

template <typename Callback>
void invokeOnUiThread(QObject* context, Callback&& callback)
{
    if (context == nullptr) {
        return;
    }

    if (QThread::currentThread() == context->thread()) {
        callback();
        return;
    }

    QMetaObject::invokeMethod(context, std::forward<Callback>(callback), Qt::QueuedConnection);
}

} // namespace

GeoResourceUpdateCoordinator::GeoResourceUpdateCoordinator(Dependencies dependencies, QObject* parent)
    : QObject(parent)
    , deps_(std::move(dependencies))
{
}

void GeoResourceUpdateCoordinator::updateGeoResources()
{
    if (deps_.backgroundTasks == nullptr || (deps_.isAvailable && !deps_.isAvailable())) {
        return;
    }

    const BackgroundTaskCoordinator::Token token =
        deps_.backgroundTasks->tryBeginUserTask(BackgroundTaskCoordinator::Kind::GeoUpdate);
    if (!token.isValid()) {
        return;
    }

    const QString targetDirectory = deps_.targetDirectory ? deps_.targetDirectory() : QString();
    QObject* uiContext = deps_.uiContext ? deps_.uiContext() : QCoreApplication::instance();
    const QString title = QCoreApplication::translate("AppBootstrap", "Update Geo Files");
    const QString message = QCoreApplication::translate("AppBootstrap", "Updating Geo resources in the background...");
    if (deps_.appendLog) {
        deps_.appendLog(message);
    }
    if (deps_.showRoutineTransientStatus) {
        deps_.showRoutineTransientStatus(message, 3000);
    }

    const std::weak_ptr<char> lifetimeGuard = deps_.lifetimeGuard ? deps_.lifetimeGuard() : std::weak_ptr<char>();
    QThread* thread = QThread::create([this, targetDirectory, title, uiContext, token, lifetimeGuard]() {
        GeoResourceUpdateService geoResourceUpdateService(targetDirectory);
        const QList<OperationResult> results{
            geoResourceUpdateService.update(QStringLiteral("geosite")),
            geoResourceUpdateService.update(QStringLiteral("geoip"))};

        bool hasSuccess = false;
        QStringList failureLines;
        QStringList successLines;
        for (const OperationResult& result : results) {
            if (result.success) {
                hasSuccess = true;
                if (!result.message.trimmed().isEmpty()) {
                    successLines.append(result.message);
                }
            } else if (!result.message.trimmed().isEmpty()) {
                failureLines.append(result.message);
            }
        }

        invokeOnUiThread(uiContext, [this, title, results, hasSuccess, failureLines, successLines, token, lifetimeGuard]() {
            if (lifetimeGuard.expired()) {
                return;
            }
            if (deps_.backgroundTasks == nullptr || !deps_.backgroundTasks->isCurrent(token)) {
                return;
            }
            deps_.backgroundTasks->finish(token);
            for (const OperationResult& result : results) {
                if (deps_.appendResult) {
                    deps_.appendResult(result);
                }
            }

            QWidget* parent = deps_.dialogParent ? deps_.dialogParent() : nullptr;
            if (!failureLines.isEmpty()) {
                if (deps_.showWarning) {
                    deps_.showWarning(parent, title, failureLines.join(QChar('\n')));
                }
            } else if (!successLines.isEmpty() && deps_.showInformation) {
                deps_.showInformation(parent, title, successLines.join(QChar('\n')));
            }

            if (hasSuccess && deps_.restartCoreIfRunning) {
                deps_.restartCoreIfRunning(QCoreApplication::translate(
                    "AppBootstrap",
                    "Reloading core after updating Geo resources."));
            }
        });
    });

    if (deps_.trackBackgroundThread) {
        deps_.trackBackgroundThread(thread);
    }
    thread->start();
}
