#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"
#include "app/ServerCollectionCoordinator.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSessionManager>
#include <QWidget>

#include "ui/mainwindow/MainWindow.h"

void AppBootstrap::wireShutdownHooks()
{
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, objects_->mainWindow.get(), [this]() {
        shuttingDown_.store(true);
        cleanupRuntimeForExit(windowsShutdownRequested_);
        // A hide is deferred until the window checkpoints, and an exit is the last checkpoint there
        // is: without this the one edit that is deliberately not written on the spot would be the
        // one edit a quit loses. persistUiState() follows, so its capture lands on top of the list
        // written here rather than on a stale one.
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->persistPendingSubscriptionEdits();
        }
        if (!shutdownUiStatePersisted_) {
            persistUiState();
            shutdownUiStatePersisted_ = true;
        }
    });
    if (QGuiApplication* guiApplication = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        QObject::connect(guiApplication, &QGuiApplication::commitDataRequest, objects_->mainWindow.get(), [this](QSessionManager&) {
            windowsShutdownRequested_ = true;
            shuttingDown_.store(true);
            cleanupRuntimeForExit(true);
            if (objects_->serverCollectionCoordinator != nullptr) {
                objects_->serverCollectionCoordinator->persistPendingSubscriptionEdits();
            }
            if (!shutdownUiStatePersisted_) {
                persistUiState();
                shutdownUiStatePersisted_ = true;
            }
        });
    }
}
