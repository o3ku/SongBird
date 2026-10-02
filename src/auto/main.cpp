#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QTimer>

#include "appcore/StartupAdminElevation.h"
#include "auto/SongBirdAutoCoordinator.h"
#include "auto/SongBirdAutoWindow.h"
#include "common/AppPlatform.h"

#ifndef SONGBIRD_APP_VERSION
#define SONGBIRD_APP_VERSION "2.4.4"
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

    // SongBirdAuto is an English-only surface. Deliberately no QTranslator is installed here,
    // and there is nothing to install one from: the generated translations.qrc -- the only
    // place the compiled .qm files are embedded -- is appended to SONGBIRD_SOURCES, which
    // builds SongBird.exe alone, so this executable carries no compiled translation at all.
    // That is a product decision, not a forgotten step, and src/auto/ matches it: the strings
    // in this directory are plain QStringLiteral(), not wrappers, because a wrapper would
    // promise a translation that no binary can ever load.
    // The two halves have to move together. Installing a translator here without re-wrapping
    // src/auto/ would translate Qt's own dialogs while leaving every application string in
    // English -- the worst of both worlds. ctest localization-coverage enforces the rule
    // (Check E: no translation wrapper anywhere under an English-surface root).
    //
    // This comment names no translation function on purpose, so that grepping src/auto for one
    // comes back empty: the rule should be visible to a human auditor, not only to the checker.

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
