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
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>
#include <QTranslator>

#include "app/AppBootstrap.h"
#include "app/ConfigPathResolver.h"
#include "app/StartupAdminElevation.h"
#include "app/SingleInstanceBootstrap.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"
#include "ui/theme/AppTheme.h"

#ifndef SONGBIRD_APP_VERSION
#define SONGBIRD_APP_VERSION "2.2.1"
#endif

namespace {

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

    const QByteArray payload = file.readAll();
    file.close();

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    return normalizeLanguageCode(
        root.value(QStringLiteral("ui")).toObject().value(QStringLiteral("languageCode")).toString());
}

QString loadConfiguredThemeName(const QString& configPath)
{
    QFile file(configPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return AppTheme::lightThemeName();
    }

    const QByteArray payload = file.readAll();
    file.close();

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return AppTheme::lightThemeName();
    }

    const QJsonObject root = document.object();
    return AppTheme::normalizeThemeName(
        root.value(QStringLiteral("ui")).toObject().value(QStringLiteral("themeName")).toString());
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

void installConfiguredTranslator(QApplication& app, QTranslator& translator, const QString& languageCode)
{
    if (languageCode == QStringLiteral("en")) {
        return;
    }

    const QLocale locale = languageCode == QStringLiteral("zh_CN")
        ? QLocale(QLocale::Chinese, QLocale::China)
        : QLocale();

    const QString qmPrefix = QStringLiteral("SongBird");
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
            QLocale::setDefault(locale);
            return;
        }
    }
}

bool installQtTranslator(
    QApplication& app,
    QTranslator& translator,
    const QString& languageCode,
    const QString& qmPrefix)
{
    if (languageCode == QStringLiteral("en")) {
        return false;
    }

    const QLocale locale = languageCode == QStringLiteral("zh_CN")
        ? QLocale(QLocale::Chinese, QLocale::China)
        : QLocale();

    QString translationsPath;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    translationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    translationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif

    const QStringList searchDirs = {
        QStringLiteral(":/translations"),
        translationsPath
    };

    for (const QString& dir : searchDirs) {
        if (!dir.isEmpty() && translator.load(locale, qmPrefix, QStringLiteral("_"), dir)) {
            app.installTranslator(&translator);
            return true;
        }
    }

    return false;
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
    app.setApplicationName(QStringLiteral("SongBird"));
    app.setOrganizationName(QStringLiteral("SongBird"));
    app.setApplicationVersion(QStringLiteral(SONGBIRD_APP_VERSION));
    const QString requestedConfigPath = resolveRequestedConfigPath(QCoreApplication::arguments());
    AppTheme::applyApplicationTheme(app, loadConfiguredThemeName(requestedConfigPath));
    const QIcon appIcon(QStringLiteral(":/app/logo.ico"));
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    QTranslator qtBaseTranslator;
    QTranslator qtTranslator;
    QTranslator translator;
    const QString languageCode = loadConfiguredLanguageCode(requestedConfigPath);
    installQtTranslator(app, qtBaseTranslator, languageCode, QStringLiteral("qtbase"));
    installQtTranslator(app, qtTranslator, languageCode, QStringLiteral("qt"));
    installConfiguredTranslator(app, translator, languageCode);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "SongBird is a Qt/C++ rewrite and improvement of v2rayN."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption configOption(
        QStringList{QStringLiteral("config")},
        QCoreApplication::translate("main", "Use a specific songbird.json file."),
        QStringLiteral("path"));
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
    parser.addOption(startHiddenOption);
    parser.addOption(disableSingleInstanceOption);
    parser.addOption(adminRelaunchOption);
    parser.addOption(restartWaitPidOption);
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
        qputenv("SONGBIRD_NONINTERACTIVE", QByteArrayLiteral("1"));
    }

    const qint64 restartWaitPid = parser.isSet(restartWaitPidOption)
        ? parseRestartWaitPidArgument(parser.value(restartWaitPidOption))
        : 0;
    if (restartWaitPid > 0) {
        waitForProcessExit(restartWaitPid, 10000);
    }

    const bool interactivePromptsEnabled = startupAdminInteractivePromptsEnabled(
        parser.isSet(nonInteractiveOption),
        qEnvironmentVariableIsSet("SONGBIRD_NONINTERACTIVE"));
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
        DialogUtils::showInformation(
            nullptr,
            QStringLiteral("SongBird"),
            QCoreApplication::translate("main", "Another SongBird instance is already running."));
        return 0;
    }

    AppBootstrap bootstrap(
        requestedConfigPath,
        parser.isSet(startHiddenOption),
        parser.isSet(skipCoreOption));
    if (!bootstrap.run()) {
        return 1;
    }

    if (quitAfterMs > 0) {
        QTimer::singleShot(quitAfterMs, &app, &QCoreApplication::quit);
    }

    return app.exec();
}


