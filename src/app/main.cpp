#include <QApplication>
#include <QCommandLineParser>
#include <QtGlobal>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>
#include <QTranslator>

#include "app/AppBootstrap.h"
#include "app/StartupAdminElevation.h"
#include "app/SingleInstanceBootstrap.h"
#include "ui/theme/AppTheme.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif

#ifndef QT_V2RAYN_APP_VERSION
#define QT_V2RAYN_APP_VERSION "0.4.0"
#endif

namespace {

QString resolveRequestedConfigPath(const QStringList& arguments)
{
    for (int index = 1; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index);
        if (argument == QStringLiteral("--config") && index + 1 < arguments.size()) {
            return arguments.at(index + 1);
        }

        if (argument.startsWith(QStringLiteral("--config="))) {
            return argument.mid(QStringLiteral("--config=").size());
        }
    }

    const QString currentDirectoryPath = QDir::current().filePath(QStringLiteral("guiNConfig.json"));
    if (QFileInfo::exists(currentDirectoryPath)) {
        return currentDirectoryPath;
    }

    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("guiNConfig.json"));
}

QString normalizeLanguageCode(QString languageCode)
{
    languageCode = languageCode.trimmed();
    if (languageCode.isEmpty()) {
        return {};
    }

    languageCode.replace(QChar('-'), QChar('_'));
    const QString normalized = languageCode.toLower();
    if (normalized == QStringLiteral("system")
        || normalized == QStringLiteral("default")
        || normalized == QStringLiteral("auto")) {
        return {};
    }

    if (normalized == QStringLiteral("zh")
        || normalized == QStringLiteral("zh_cn")
        || normalized == QStringLiteral("zh_hans")) {
        return QStringLiteral("zh_CN");
    }

    if (normalized.startsWith(QStringLiteral("en"))) {
        return QStringLiteral("en");
    }

    return {};
}

QString loadConfiguredLanguageCode(const QString& configPath)
{
    QFile file(configPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    const QJsonObject uiItem = root.value(QStringLiteral("uiItem")).toObject();
    return normalizeLanguageCode(
        uiItem.value(QStringLiteral("languageCode")).toString(root.value(QStringLiteral("languageCode")).toString()));
}

bool loadConfiguredTunEnabled(const QString& configPath)
{
    QFile file(configPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    return startupConfigHasTunEnabled(file.readAll());
}

void installConfiguredTranslator(QApplication& app, QTranslator& translator, const QString& languageCode)
{
    if (languageCode == QStringLiteral("en")) {
        return;
    }

    const QLocale locale = languageCode == QStringLiteral("zh_CN")
        ? QLocale(QLocale::Chinese, QLocale::China)
        : QLocale();

    const QString qmPrefix = QStringLiteral("v2rayq");
    const QString qmSeparator = QStringLiteral("_");

    // Try embedded resource first, then filesystem
    const QStringList searchDirs = {
        QStringLiteral(":/translations"),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")),
        QCoreApplication::applicationDirPath()
    };

    for (const QString& dir : searchDirs) {
        if (translator.load(locale, qmPrefix, qmSeparator, dir)) {
            app.installTranslator(&translator);
            return;
        }
    }
}

bool isWindowsPlatform()
{
#if defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

bool isProcessElevated()
{
#if defined(Q_OS_WIN)
    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD returnedSize = 0;
    const BOOL ok = GetTokenInformation(
        tokenHandle,
        TokenElevation,
        &elevation,
        sizeof(elevation),
        &returnedSize);
    CloseHandle(tokenHandle);
    return ok != FALSE && elevation.TokenIsElevated != 0;
#else
    return true;
#endif
}

QString startupAdminPromptTitle()
{
    return QCoreApplication::translate("main", "Administrator Permission");
}

QString startupAdminPromptMessage()
{
    return QCoreApplication::translate(
        "main",
        "v2rayq is not running with administrator privileges.\nRestart as administrator now?");
}

QString startupAdminRestartFailureMessage()
{
    return QCoreApplication::translate(
        "main",
        "Failed to restart v2rayq with administrator privileges.");
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
    app.setApplicationName(QStringLiteral("v2rayq"));
    app.setOrganizationName(QStringLiteral("v2rayq"));
    app.setApplicationVersion(QStringLiteral(QT_V2RAYN_APP_VERSION));
    AppTheme::applyApplicationTheme(app);
    const QIcon appIcon(QStringLiteral(":/app/logo.ico"));
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    QTranslator translator;
    const QString requestedConfigPath = resolveRequestedConfigPath(QCoreApplication::arguments());
    installConfiguredTranslator(app, translator, loadConfiguredLanguageCode(requestedConfigPath));

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Pure Qt prototype for v2rayN."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption configOption(
        QStringList{QStringLiteral("config")},
        QCoreApplication::translate("main", "Use a specific guiNConfig.json file."),
        QStringLiteral("path"));
    const QCommandLineOption autoStartOption(
        QStringList{QStringLiteral("auto-start")},
        QCoreApplication::translate("main", "Start the selected core after the UI is initialized."));
    const QCommandLineOption startHiddenOption(
        QStringList{QStringLiteral("start-hidden")},
        QCoreApplication::translate("main", "Hide to tray or minimize after startup."));
    const QCommandLineOption disableSingleInstanceOption(
        QStringList{QStringLiteral("disable-single-instance")},
        QCoreApplication::translate("main", "Skip the single-instance guard for automation or testing."));
    QCommandLineOption adminRelaunchOption(QStringList{QStringLiteral("admin-relaunch")});
    adminRelaunchOption.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption restartWaitPidOption(
        QStringList{QStringLiteral("restart-wait-pid")},
        QString(),
        QStringLiteral("pid"));
    restartWaitPidOption.setFlags(QCommandLineOption::HiddenFromHelp);
    const QCommandLineOption disableGlobalHotkeysOption(
        QStringList{QStringLiteral("disable-global-hotkeys")},
        QCoreApplication::translate("main", "Do not register Windows global hotkeys on startup."));
    const QCommandLineOption skipCoreOption(
        QStringList{QStringLiteral("skip-core")},
        QCoreApplication::translate("main", "Skip startup core detection and auto-start for testing."));
    const QCommandLineOption nonInteractiveOption(
        QStringList{QStringLiteral("non-interactive")},
        QCoreApplication::translate("main", "Disable blocking prompts for automation or testing."));
    const QCommandLineOption quitAfterMsOption(
        QStringList{QStringLiteral("quit-after-ms")},
        QCoreApplication::translate("main", "Quit the application after the specified milliseconds."),
        QStringLiteral("milliseconds"));

    parser.addOption(configOption);
    parser.addOption(autoStartOption);
    parser.addOption(startHiddenOption);
    parser.addOption(disableSingleInstanceOption);
    parser.addOption(adminRelaunchOption);
    parser.addOption(restartWaitPidOption);
    parser.addOption(disableGlobalHotkeysOption);
    parser.addOption(skipCoreOption);
    parser.addOption(nonInteractiveOption);
    parser.addOption(quitAfterMsOption);
    parser.process(app);

    bool quitAfterMsOk = true;
    const int quitAfterMs = parser.isSet(quitAfterMsOption)
        ? parser.value(quitAfterMsOption).toInt(&quitAfterMsOk)
        : 0;
    if (!quitAfterMsOk || quitAfterMs < 0) {
        return 2;
    }

    if (parser.isSet(nonInteractiveOption)) {
        qputenv("QT_V2RAYN_NONINTERACTIVE", QByteArrayLiteral("1"));
    }

    const qint64 restartWaitPid = parser.isSet(restartWaitPidOption)
        ? parseRestartWaitPidArgument(parser.value(restartWaitPidOption))
        : 0;
    if (restartWaitPid > 0) {
        waitForProcessExit(restartWaitPid, 10000);
    }

    const bool interactivePromptsEnabled = startupAdminInteractivePromptsEnabled(
        parser.isSet(nonInteractiveOption),
        qEnvironmentVariableIsSet("QT_V2RAYN_NONINTERACTIVE"));
    const bool configuredTunEnabled = loadConfiguredTunEnabled(requestedConfigPath);
    const StartupAdminElevationDecision startupAdminElevation = evaluateStartupAdminElevation(
        isWindowsPlatform(),
        isProcessElevated(),
        interactivePromptsEnabled,
        configuredTunEnabled,
        QCoreApplication::arguments());
    if (startupAdminElevation.shouldPromptForElevation) {
        if (restartProcessAsAdministrator(
                QCoreApplication::applicationFilePath(),
                startupAdminElevation.relaunchArguments)) {
            return 0;
        }
    }

    SingleInstanceBootstrap singleInstanceBootstrap;
    const StartupSingleInstanceAcquireDecision singleInstanceDecision =
        evaluateStartupSingleInstanceAcquireDecision(
            parser.isSet(disableSingleInstanceOption),
            parser.isSet(adminRelaunchOption) || restartWaitPid > 0);
    if (!tryAcquireStartupSingleInstance(
            singleInstanceDecision,
            [&singleInstanceBootstrap]() {
                return singleInstanceBootstrap.tryAcquire();
            },
            [](int) {})) {
        QMessageBox::information(
            nullptr,
            QStringLiteral("v2rayq"),
            QCoreApplication::translate("main", "Another v2rayq instance is already running."));
        return 0;
    }

    AppBootstrap bootstrap(
        parser.value(configOption),
        parser.isSet(autoStartOption),
        parser.isSet(startHiddenOption),
        !parser.isSet(disableGlobalHotkeysOption),
        parser.isSet(skipCoreOption));
    if (!bootstrap.run()) {
        return 1;
    }

    if (quitAfterMs > 0) {
        QTimer::singleShot(quitAfterMs, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
