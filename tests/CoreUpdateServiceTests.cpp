#include <QtTest>

#include <QElapsedTimer>
#include <QTemporaryDir>

#include <atomic>
#include <thread>

#include "domain/models/Config.h"
#include "services/CoreUpdateService.h"

class CoreUpdateServiceTests : public QObject {
    Q_OBJECT

private slots:
    void updateReturnsPromptlyWhenCancellationRequestedDuringDownload();
};

void CoreUpdateServiceTests::updateReturnsPromptlyWhenCancellationRequestedDuringDownload()
{
    std::atomic_bool cancelled = false;
    int downloadAttempts = 0;
    const QByteArray releasePayload =
        R"([{"tag_name":"v1.0.0","prerelease":false,"assets":[{"name":"sing-box-1.0.0-windows-amd64.zip","browser_download_url":"https://github.com/SagerNet/sing-box/releases/download/v1.0.0/sing-box-1.0.0-windows-amd64.zip"}]}])";

    CoreUpdateService service(
        [&](const QUrl& url, QByteArray* content) {
            ++downloadAttempts;
            if (url.host() == QStringLiteral("api.github.com")
                || url.path().contains(QStringLiteral("/repos/"))) {
                *content = releasePayload;
                return OperationResult::ok();
            }

            while (!cancelled.load()) {
                QTest::qSleep(10);
            }

            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Core update was canceled."));
        },
        [](const QString&, const QString&) {
            return OperationResult::ok();
        });

    Config config;
    config.checkPreReleaseUpdate = false;
    config.ignoreGeoUpdateCore = true;

    QTemporaryDir targetDirectory;
    QVERIFY(targetDirectory.isValid());

    QElapsedTimer timer;
    timer.start();

    std::thread cancellationThread([&cancelled]() {
        QTest::qSleep(50);
        cancelled.store(true);
    });

    CoreUpdateService::UpdateOptions options;
    options.cancelCheck = [&cancelled]() {
        return cancelled.load();
    };
    options.skipLocalVersionCheck = true;

    const OperationResult result = service.update(
        CoreType::SingBox,
        config,
        targetDirectory.path(),
        options);
    cancellationThread.join();

    QVERIFY(!result.success);
    QVERIFY(result.cancelled);
    QVERIFY(timer.elapsed() < 1500);
    QVERIFY(downloadAttempts >= 2);
}

QTEST_MAIN(CoreUpdateServiceTests)

#include "CoreUpdateServiceTests.moc"
