#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

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
            if (!shutdownUiStatePersisted_) {
                persistUiState();
                shutdownUiStatePersisted_ = true;
            }
        });
    }
}
