#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QTimer>

#include "app/StartupAdminElevation.h"
#include "auto/SongBirdAutoCoordinator.h"
#include "auto/SongBirdAutoWindow.h"
#include "common/AppPlatform.h"

#ifndef SONGBIRD_APP_VERSION
#define SONGBIRD_APP_VERSION "2.4.0"
#endif

namespace {

QString defaultAutoConfigPath()
{
    const QString localPath = QDir::current().filePath(QStringLiteral("songbird-auto.json"));
    if (QFileInfo::exists(localPath)) {
        return localPath;
    }
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("songbird-auto.json"));
}

bool loadConfiguredTunEnabled(const QString& configPath)
{
    QFile file(configPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QByteArray payload = file.readAll();
    file.close();
    return startupConfigHasTunEnabled(payload);
}

} // namespace

int main(int argc, char* argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName(QStringLiteral("SongBirdAuto"));
    app.setOrganizationName(QStringLiteral("SongBird"));
    app.setApplicationVersion(QStringLiteral(SONGBIRD_APP_VERSION));

    const QIcon appIcon(QStringLiteral(":/app/logo-auto.ico"));
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SongBirdAuto"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption configOption(
        QStringList{QStringLiteral("config")},
        QStringLiteral("Use a specific songbird-auto.json file."),
        QStringLiteral("path"));
    QCommandLineOption adminRelaunchOption(QStringList{QStringLiteral("admin-relaunch")});
    adminRelaunchOption.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption restartWaitPidOption(
        QStringList{QStringLiteral("restart-wait-pid")},
        QString(),
        QStringLiteral("pid"));
    restartWaitPidOption.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption autoStartOption(QStringList{QStringLiteral("auto-start")});
    autoStartOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(configOption);
    parser.addOption(adminRelaunchOption);
    parser.addOption(restartWaitPidOption);
    parser.addOption(autoStartOption);
    parser.process(app);

    const qint64 restartWaitPid = parser.isSet(restartWaitPidOption)
        ? parseRestartWaitPidArgument(parser.value(restartWaitPidOption))
        : 0;
    if (restartWaitPid > 0) {
        waitForProcessExit(restartWaitPid, 10000);
    }

    const QString configPath = parser.isSet(configOption)
        ? parser.value(configOption)
        : defaultAutoConfigPath();

    if (isWindowsPlatform()
        && !isProcessElevated()
        && loadConfiguredTunEnabled(configPath)) {
        const QStringList arguments = startupRelaunchArgumentsForRunningInstance(
            QCoreApplication::arguments(),
            true,
            QCoreApplication::applicationPid());
        if (restartProcessAsAdministrator(QCoreApplication::applicationFilePath(), arguments)) {
            return 0;
        }
    }

    SongBirdAutoCoordinator coordinator(configPath);
    SongBirdAutoWindow window(coordinator);
    window.show();

    if (!coordinator.initialize()) {
        return 1;
    }

    if (parser.isSet(autoStartOption)) {
        QTimer::singleShot(0, &window, &SongBirdAutoWindow::startProxy);
    }

    return app.exec();
}
