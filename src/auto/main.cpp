#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

#include "appcore/StartupAdminElevation.h"
#include "auto/SongBirdAutoCoordinator.h"
#include "auto/SongBirdAutoWindow.h"
#include "common/AppPaths.h"
#include "common/AppPlatform.h"
#include "platform/SingleInstanceGuard.h"

#ifndef SONGBIRD_APP_VERSION
// Deliberately not a version number. src/CMakeLists.txt always defines this from
// project(VERSION), so the only way to get here is a build that bypassed CMake -- and a
// plausible-looking fallback would report a version nobody is running. Saying "unknown" makes
// that build fail visibly instead.
#define SONGBIRD_APP_VERSION "unknown"
#endif

namespace {

QString defaultAutoConfigPath()
{
    const QString localPath = QDir::current().filePath(QStringLiteral("songbird-auto.json"));
    if (QFileInfo::exists(localPath)) {
        return localPath;
    }
    return QDir(AppPaths::applicationDirectory()).filePath(QStringLiteral("songbird-auto.json"));
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
    QCommandLineOption disableSingleInstanceOption(
        QStringList{QStringLiteral("disable-single-instance")},
        // Bare literal, not translate(): src/auto is this tree's one English surface, and
        // check-localization-coverage.ps1's Check E rejects a wrapper here outright. The main
        // app's identically worded option is translated because SongBird.exe installs a
        // translator and SongBirdAuto.exe does not -- the wording matches so a reader moving
        // between the two files is not left wondering whether one of them is a typo.
        QStringLiteral("Skip the single-instance guard for automation or testing."));
    parser.addOption(configOption);
    parser.addOption(adminRelaunchOption);
    parser.addOption(restartWaitPidOption);
    parser.addOption(autoStartOption);
    parser.addOption(disableSingleInstanceOption);
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

    // One Auto process at a time, through the same guard shape as the main app.
    //
    // This used to be "the port conflict will stop a second instance anyway", which is not a
    // lock: a second instance can come up far enough to run recoverSystemProxyFromPreviousRun()
    // -- the coordinator's startup step that restores the registry from the persisted snapshot
    // -- and treat the FIRST instance's live, owned system proxy as a killed run's leftover.
    // The restore would wipe values the other process is actively serving. The guard is what
    // makes the startup recovery sound, so it has to be taken before the coordinator (and
    // therefore before initialize()) runs. The key differs from the main app's on purpose:
    // SongBird.exe and SongBirdAuto.exe can run side by side, each guarding its own kind.
    //
    // The decision and the retry ladder live in appcore/StartupAdminElevation.h beside the
    // main app's copy of this dance: an admin relaunch or a restart handoff (restart-wait-pid)
    // is the one context where the previous instance may still be holding the guard, and the
    // decision answers it with a bounded retry instead of a refusal or a spin.
    SingleInstanceGuard singleInstanceGuard(QStringLiteral("SongBirdAuto-single-instance"));
    const StartupSingleInstanceAcquireDecision singleInstanceDecision =
        evaluateStartupSingleInstanceAcquireDecision(
            parser.isSet(disableSingleInstanceOption),
            parser.isSet(adminRelaunchOption) || restartWaitPid > 0);
    if (!tryAcquireStartupSingleInstance(
            singleInstanceDecision,
            [&singleInstanceGuard]() {
                return singleInstanceGuard.tryAcquire();
            },
            [](int intervalMs) {
                // Honoured rather than ignored: the decision's retryIntervalMs is the whole
                // difference between a bounded wait for the old instance and a zero-cost busy
                // loop. See the same call site in src/app/main.cpp.
                QThread::msleep(intervalMs);
            })) {
        QMessageBox::information(
            nullptr,
            QStringLiteral("SongBirdAuto"),
            QStringLiteral("Another SongBirdAuto instance is already running."));
        return 0;
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
