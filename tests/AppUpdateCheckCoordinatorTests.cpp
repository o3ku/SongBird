#include <QtTest>

#include "app/AppUpdateCheckCoordinator.h"
#include "app/BackgroundTaskCoordinator.h"
#include "app/IUserFeedback.h"

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
        [&checked](const QString&, bool, AppUpdateCheckResult* result) {
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
        [](const QString&, bool, AppUpdateCheckResult* result) {
            result->updateAvailable = true;
            result->currentVersion = QStringLiteral("1.0.0");
            result->latestVersion = QStringLiteral("v1.1.0");
            result->releaseName = QStringLiteral("SongBird 1.1.0");
            result->releaseUrl = QUrl(QStringLiteral("https://example.test/releases/v1.1.0"));
            result->assetName = QStringLiteral("SongBird.exe");
            result->assetDownloadUrl = QUrl(QStringLiteral("https://example.test/SongBird.exe"));
            return OperationResult::ok(QStringLiteral("SongBird v1.1.0 is available."));
        },
        [&downloadedName, &downloadedDirectory](const QUrl&, const QString& assetName, const QString& targetDirectory, QString* savedPath) {
            downloadedName = assetName;
            downloadedDirectory = targetDirectory;
            *savedPath = QStringLiteral("C:/SongBird/updates/SongBird.new.exe");
            return OperationResult::ok(QStringLiteral("SongBird update downloaded."));
        },
        [](std::function<void()> task) { task(); },
        [](QObject*, std::function<void()> callback) { callback(); }
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
        [&backgroundTasks, &activeKindsDuringDownload](const QUrl&, const QString&, const QString&, QString* savedPath) {
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

QTEST_MAIN(AppUpdateCheckCoordinatorTests)

#include "AppUpdateCheckCoordinatorTests.moc"
