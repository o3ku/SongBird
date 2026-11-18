#include <QtTest>

#include "app/AppUpdateCheckCoordinator.h"
#include "appcore/BackgroundTaskCoordinator.h"
#include "appcore/IUserFeedback.h"

class RecordingFeedback final : public IUserFeedback {
public:
    QObject* uiContext() const override { return nullptr; }
    QWidget* dialogParent() const override { return nullptr; }

    void appendLog(const QString& message) override { logs.append(message); }
    void recordOperationResult(const OperationResult& result) override { results.append(result); }
    void showOperationMessage(const QString& title, const OperationResult& result, QWidget*) override
    {
        operationMessageTitles.append(title);
        operationMessages.append(result.message);
    }
    bool askYesNo(QWidget*, const QString& title, const QString& text, YesNoDefault) override
    {
        prompts.append(title + QStringLiteral("\n") + text);
        return yesNoAnswer;
    }
    void showInformation(QWidget*, const QString& title, const QString& text) override
    {
        informationMessages.append(title + QStringLiteral("\n") + text);
    }
    void showTrayMessage(const QString& title, const QString& message, bool, int) override
    {
        trayMessages.append(title + QStringLiteral("\n") + message);
    }
    void openExternalUrl(const QString& url) override { openedUrls.append(url); }
    bool promptRestartForDownloadedAppUpdate(const QString& newExecutablePath, QWidget*) override
    {
        restartPrompts.append(newExecutablePath);
        return true;
    }

    QStringList logs;
    QList<OperationResult> results;
    QStringList operationMessageTitles;
    QStringList operationMessages;
    QStringList prompts;
    QStringList informationMessages;
    QStringList trayMessages;
    QStringList openedUrls;
    QStringList restartPrompts;
    bool yesNoAnswer = true;
};

class AppUpdateCheckCoordinatorTests : public QObject {
    Q_OBJECT

private slots:
    void checkDoesNotClaimBackgroundTask();
    void manualCheckDownloadsWhenUserAccepts();
    void downloadClaimsAndFinishesAppUpdateTask();
    void macDiskImagePromptsForRestartOnlyWhenEligible_data();
    void macDiskImagePromptsForRestartOnlyWhenEligible();
    void prereleaseBuildChecksPrereleasesEvenWhenTheSettingIsOff();
};

void AppUpdateCheckCoordinatorTests::checkDoesNotClaimBackgroundTask()
{
    BackgroundTaskCoordinator backgroundTasks;
    RecordingFeedback feedback;
    bool checked = false;

    AppUpdateCheckCoordinator coordinator({
        &backgroundTasks,
        &feedback,
        []() { return QStringLiteral("1.0.0"); },
        []() { return false; },
        []() { return QStringLiteral("C:/SongBird"); },
        []() { return false; },
        {},
        [&checked](const QString&, bool, const CoreAssetPlatform&, AppUpdateCheckResult* result) {
            checked = true;
            result->updateAvailable = false;
            result->currentVersion = QStringLiteral("1.0.0");
            result->latestVersion = QStringLiteral("1.0.0");
            return OperationResult::ok(QStringLiteral("SongBird is up to date: 1.0.0."));
        },
        {},
        [](std::function<void()> task) { task(); },
        [](QObject*, std::function<void()> callback) { callback(); }
    });

    coordinator.checkAppUpdates(false);

    QVERIFY(checked);
    QVERIFY(!backgroundTasks.isActive());
    QVERIFY(!coordinator.isCheckRunning());
    QVERIFY(feedback.operationMessages.isEmpty());
}

void AppUpdateCheckCoordinatorTests::manualCheckDownloadsWhenUserAccepts()
{
    BackgroundTaskCoordinator backgroundTasks;
    RecordingFeedback feedback;
    QString downloadedName;
    QString downloadedDirectory;

    AppUpdateCheckCoordinator coordinator({
        &backgroundTasks,
        &feedback,
        []() { return QStringLiteral("1.0.0"); },
        []() { return false; },
        []() { return QStringLiteral("C:/SongBird"); },
        []() { return false; },
        {},
        [](const QString&, bool, const CoreAssetPlatform&, AppUpdateCheckResult* result) {
            result->updateAvailable = true;
            result->currentVersion = QStringLiteral("1.0.0");
            result->latestVersion = QStringLiteral("v1.1.0");
            result->releaseName = QStringLiteral("SongBird 1.1.0");
            result->releaseUrl = QUrl(QStringLiteral("https://example.test/releases/v1.1.0"));
            result->assetName = QStringLiteral("SongBird.exe");
            result->assetDownloadUrl = QUrl(QStringLiteral("https://example.test/SongBird.exe"));
            return OperationResult::ok(QStringLiteral("SongBird v1.1.0 is available."));
        },
        [&downloadedName, &downloadedDirectory](const AppUpdateCheckResult& checkResult, const QString& targetDirectory, QString* savedPath) {
            downloadedName = checkResult.downloadFileName;
            downloadedDirectory = targetDirectory;
            *savedPath = QStringLiteral("C:/SongBird/updates/SongBird.new.exe");
            return OperationResult::ok(QStringLiteral("SongBird update downloaded."));
        },
        [](std::function<void()> task) { task(); },
        [](QObject*, std::function<void()> callback) { callback(); },
        []() { return CoreAssetPlatform{CoreAssetPlatform::Os::Windows}; }
    });

    coordinator.checkAppUpdates(true);

    QCOMPARE(feedback.prompts.size(), 1);
    QCOMPARE(downloadedName, QStringLiteral("SongBird.new.exe"));
    QCOMPARE(downloadedDirectory, QStringLiteral("C:/SongBird/updates"));
    QCOMPARE(feedback.restartPrompts, QStringList{QStringLiteral("C:/SongBird/updates/SongBird.new.exe")});
    QVERIFY(!backgroundTasks.isActive());
}

void AppUpdateCheckCoordinatorTests::downloadClaimsAndFinishesAppUpdateTask()
{
    BackgroundTaskCoordinator backgroundTasks;
    RecordingFeedback feedback;
    QList<BackgroundTaskCoordinator::Kind> activeKindsDuringDownload;

    AppUpdateCheckResult updateResult;
    updateResult.latestVersion = QStringLiteral("v1.1.0");
    updateResult.assetName = QStringLiteral("SongBird.zip");
    updateResult.assetDownloadUrl = QUrl(QStringLiteral("https://example.test/SongBird.zip"));

    AppUpdateCheckCoordinator coordinator({
        &backgroundTasks,
        &feedback,
        []() { return QStringLiteral("1.0.0"); },
        []() { return false; },
        []() { return QStringLiteral("C:/SongBird"); },
        []() { return false; },
        {},
        {},
        [&backgroundTasks, &activeKindsDuringDownload](const AppUpdateCheckResult&, const QString&, QString* savedPath) {
            activeKindsDuringDownload.append(backgroundTasks.active());
            *savedPath = QStringLiteral("C:/SongBird/updates/SongBird.zip");
            return OperationResult::ok(QStringLiteral("SongBird update downloaded."));
        },
        [](std::function<void()> task) { task(); },
        [](QObject*, std::function<void()> callback) { callback(); }
    });

    coordinator.downloadAppUpdate(updateResult);

    QCOMPARE(activeKindsDuringDownload, QList<BackgroundTaskCoordinator::Kind>{BackgroundTaskCoordinator::Kind::AppUpdate});
    QVERIFY(!backgroundTasks.isActive());
    QCOMPARE(feedback.informationMessages.size(), 1);
    QVERIFY(feedback.restartPrompts.isEmpty());
}

void AppUpdateCheckCoordinatorTests::macDiskImagePromptsForRestartOnlyWhenEligible_data()
{
    QTest::addColumn<bool>("eligible");
    QTest::newRow("official checksum") << true;
    QTest::newRow("mirror checksum only") << false;
}

void AppUpdateCheckCoordinatorTests::macDiskImagePromptsForRestartOnlyWhenEligible()
{
    QFETCH(bool, eligible);
    BackgroundTaskCoordinator backgroundTasks;
    RecordingFeedback feedback;
    const QString packagePath = QStringLiteral("/tmp/SongBird Update/SongBird.dmg");

    AppUpdateCheckCoordinator::Dependencies dependencies;
    dependencies.backgroundTasks = &backgroundTasks;
    dependencies.feedback = &feedback;
    dependencies.downloadUpdate = [eligible, &packagePath](const AppUpdateCheckResult&, const QString&, QString* savedPath) {
        *savedPath = packagePath;
        return OperationResult::ok(QStringLiteral("Download result"), eligible);
    };
    dependencies.runInBackground = [](std::function<void()> task) { task(); };
    dependencies.invokeOnUiThread = [](QObject*, std::function<void()> callback) { callback(); };
    dependencies.assetPlatform = []() { return CoreAssetPlatform{CoreAssetPlatform::Os::MacOS}; };
    AppUpdateCheckCoordinator coordinator(dependencies);

    AppUpdateCheckResult release;
    release.assetName = QStringLiteral("SongBird.dmg");
    release.assetDownloadUrl = QUrl(QStringLiteral("https://example.test/SongBird.dmg"));
    coordinator.downloadAppUpdate(release);

    if (eligible) {
        QCOMPARE(feedback.restartPrompts, QStringList{packagePath});
        QVERIFY(feedback.operationMessages.isEmpty());
    } else {
        QVERIFY(feedback.restartPrompts.isEmpty());
        QCOMPARE(feedback.operationMessages, QStringList{QStringLiteral("Download result")});
    }
    QVERIFY(!backgroundTasks.isActive());
}

// The default configuration skips prereleases, and the newest stable release is numerically older
// than an installed beta -- so a beta build has to opt itself in, or 2.5.0-beta1 could never learn
// about 2.5.0-beta2. A stable build must stay unaffected.
void AppUpdateCheckCoordinatorTests::prereleaseBuildChecksPrereleasesEvenWhenTheSettingIsOff()
{
    BackgroundTaskCoordinator backgroundTasks;
    RecordingFeedback feedback;
    QList<bool> observedAllowPrerelease;
    QList<QString> observedVersions;

    AppUpdateCheckCoordinator coordinator({
        &backgroundTasks,
        &feedback,
        []() { return QStringLiteral("2.5.0-beta1"); },
        []() { return false; },
        []() { return QStringLiteral("C:/SongBird"); },
        []() { return false; },
        {},
        [&observedAllowPrerelease, &observedVersions](
            const QString& currentVersion, bool allowPrerelease, const CoreAssetPlatform&, AppUpdateCheckResult* result) {
            observedVersions.append(currentVersion);
            observedAllowPrerelease.append(allowPrerelease);
            result->currentVersion = currentVersion;
            result->latestVersion = QStringLiteral("v2.5.0-beta2");
            result->updateAvailable = allowPrerelease;
            return OperationResult::ok(QStringLiteral("checked"));
        },
        {},
        [](std::function<void()> task) { task(); },
        [](QObject*, std::function<void()> callback) { callback(); }
    });

    coordinator.checkAppUpdates(false);

    QCOMPARE(observedVersions, QList<QString>{QStringLiteral("2.5.0-beta1")});
    QCOMPARE(observedAllowPrerelease, QList<bool>{true});

    // The same coordinator with a stable build keeps honouring the setting.
    AppUpdateCheckCoordinator stableCoordinator({
        &backgroundTasks,
        &feedback,
        []() { return QStringLiteral("2.4.5"); },
        []() { return false; },
        []() { return QStringLiteral("C:/SongBird"); },
        []() { return false; },
        {},
        [&observedAllowPrerelease, &observedVersions](
            const QString& currentVersion, bool allowPrerelease, const CoreAssetPlatform&, AppUpdateCheckResult* result) {
            observedVersions.append(currentVersion);
            observedAllowPrerelease.append(allowPrerelease);
            result->currentVersion = currentVersion;
            result->updateAvailable = false;
            return OperationResult::ok(QStringLiteral("checked"));
        },
        {},
        [](std::function<void()> task) { task(); },
        [](QObject*, std::function<void()> callback) { callback(); }
    });

    stableCoordinator.checkAppUpdates(false);

    QCOMPARE(observedVersions,
             QList<QString>({QStringLiteral("2.5.0-beta1"), QStringLiteral("2.4.5")}));
    QCOMPARE(observedAllowPrerelease, QList<bool>({true, false}));
}

QTEST_MAIN(AppUpdateCheckCoordinatorTests)

#include "AppUpdateCheckCoordinatorTests.moc"
