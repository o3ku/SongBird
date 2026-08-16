#include <QtTest>

#include <QElapsedTimer>
#include <QTemporaryDir>

#include <algorithm>
#include <atomic>
#include <thread>

#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/CoreCatalog.h"
#include "runtime/core/ICoreBackend.h"
#include "services/CoreUpdatePackageInstallation.h"
#include "services/CoreUpdateService.h"
#include "services/CoreUpdateVersion.h"

class CoreUpdateServiceTests : public QObject {
    Q_OBJECT

private slots:
    void backendMetadataMatchesCatalog();
    void versionComparisonNormalizesTagsAndComparesNumericParts();
    void updateReturnsPromptlyWhenCancellationRequestedDuringDownload();
    void updateFallsBackToBuiltInSingBoxVersionWhenReleaseApiUnavailableAndNoCoreInstalled();
    void updateUsesBuiltInXrayBootstrapVersionWhenNoCoreInstalled();
    void updateGzipInstallUsesInjectedExtractorAndRespectsCancellation();
};

void CoreUpdateServiceTests::backendMetadataMatchesCatalog()
{
    for (const CoreType coreType : catalogCoreTypes()) {
        const ICoreBackend* backend = coreBackend(coreType);
        QVERIFY2(backend != nullptr, qPrintable(QStringLiteral("Missing backend for %1").arg(coreTypeDisplayName(coreType))));
        QCOMPARE(backend->displayName(), catalogCoreDisplayName(coreType));
        QCOMPARE(backend->executableNames(), catalogCoreExecutableNames(coreType));
    }
}

void CoreUpdateServiceTests::versionComparisonNormalizesTagsAndComparesNumericParts()
{
    QCOMPARE(CoreUpdateVersion::normalizeTag(QStringLiteral("Version 1.2.3")), QStringLiteral("v1.2.3"));
    QCOMPARE(CoreUpdateVersion::normalizeTag(QStringLiteral("  V2.0.0  ")), QStringLiteral("v2.0.0"));

    QVERIFY(CoreUpdateVersion::isNewerThan(QStringLiteral("v1.10.0"), QStringLiteral("v1.9.9")));
    QVERIFY(CoreUpdateVersion::isNewerThan(QStringLiteral("v2.0.0"), QStringLiteral("v1.99.99")));
    QVERIFY(CoreUpdateVersion::isNewerThan(QStringLiteral("V2.0.0"), QStringLiteral("version 1.99.99")));
    QVERIFY(!CoreUpdateVersion::isNewerThan(QStringLiteral("v1.2.0"), QStringLiteral("v1.2")));
    QVERIFY(!CoreUpdateVersion::isNewerThan(QStringLiteral("v1.2.3-beta"), QStringLiteral("v1.2.3")));
}

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

    CoreUpdateConfig config;
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

void CoreUpdateServiceTests::updateFallsBackToBuiltInSingBoxVersionWhenReleaseApiUnavailableAndNoCoreInstalled()
{
    QStringList requestedUrls;

    CoreUpdateService service(
        [&](const QUrl& url, QByteArray* content) {
            requestedUrls.append(url.toString(QUrl::FullyEncoded));
            if (url.toString().contains(QStringLiteral("api.github.com/repos/SagerNet/sing-box/releases"))) {
                return OperationResult::fail(QStringLiteral("GitHub API unavailable"));
            }
            if (url.toString().contains(QStringLiteral("/SagerNet/sing-box/releases/download/v1.13.11/sing-box-1.13.11-windows-amd64.zip"))) {
                *content = QByteArray("dummy-package");
                return OperationResult::ok();
            }
            return OperationResult::fail(QStringLiteral("Unexpected URL"));
        },
        [](const QString&, const QString&) {
            return OperationResult::ok();
        });

    CoreUpdateConfig config;
    config.checkPreReleaseUpdate = false;
    config.ignoreGeoUpdateCore = true;

    QTemporaryDir targetDirectory;
    QVERIFY(targetDirectory.isValid());

    CoreUpdateService::UpdateOptions options;
    options.skipLocalVersionCheck = true;

    const OperationResult result = service.update(
        CoreType::SingBox,
        config,
        targetDirectory.path(),
        options);

    QVERIFY(result.success);
    QVERIFY(requestedUrls.contains(QStringLiteral("https://api.github.com/repos/SagerNet/sing-box/releases?per_page=20")));
    QVERIFY(std::any_of(
        requestedUrls.cbegin(),
        requestedUrls.cend(),
        [](const QString& url) {
            return url.contains(QStringLiteral("/SagerNet/sing-box/releases/download/v1.13.11/sing-box-1.13.11-windows-amd64.zip"));
        }));
    QVERIFY(result.message.contains(QStringLiteral("v1.13.11")));
}

void CoreUpdateServiceTests::updateUsesBuiltInXrayBootstrapVersionWhenNoCoreInstalled()
{
    QStringList requestedUrls;

    CoreUpdateService service(
        [&](const QUrl& url, QByteArray* content) {
            requestedUrls.append(url.toString(QUrl::FullyEncoded));
            if (url.toString().contains(QStringLiteral("/XTLS/Xray-core/releases/download/v26.3.27/Xray-windows-64.zip"))) {
                *content = QByteArray("dummy-package");
                return OperationResult::ok();
            }
            return OperationResult::fail(QStringLiteral("Unexpected URL"));
        },
        [](const QString&, const QString&) {
            return OperationResult::ok();
        });

    CoreUpdateConfig config;
    config.checkPreReleaseUpdate = false;
    config.ignoreGeoUpdateCore = true;

    QTemporaryDir targetDirectory;
    QVERIFY(targetDirectory.isValid());

    CoreUpdateService::UpdateOptions options;
    options.skipLocalVersionCheck = true;

    const OperationResult result = service.update(
        CoreType::Xray,
        config,
        targetDirectory.path(),
        options);

    QVERIFY(result.success);
    QVERIFY(std::any_of(
        requestedUrls.cbegin(),
        requestedUrls.cend(),
        [](const QString& url) {
            return url.contains(QStringLiteral("/XTLS/Xray-core/releases/download/v26.3.27/Xray-windows-64.zip"));
        }));
    QVERIFY(result.message.contains(QStringLiteral("v26.3.27")));
    QVERIFY(!std::any_of(
        requestedUrls.cbegin(),
        requestedUrls.cend(),
        [](const QString& url) {
            return url.contains(QStringLiteral("/releases/latest/download/"));
        }));
}

void CoreUpdateServiceTests::updateGzipInstallUsesInjectedExtractorAndRespectsCancellation()
{
    // The .gz install path must route through the injected ArchiveExtractor seam,
    // just like .zip does, instead of always shelling out to PowerShell.
    bool extractorInvoked = false;
    QString extractorSourcePath;
    QString extractorTargetPath;

    CoreUpdateReleaseMetadata::GitHubReleaseAsset asset;
    asset.name = QStringLiteral("mihomo-windows-amd64-v1-v1.19.25.gz");

    QTemporaryDir targetDirectory;
    QVERIFY(targetDirectory.isValid());

    const CoreUpdateService::ArchiveExtractor extractor =
        [&](const QString& sourcePath, const QString& targetPath) {
            extractorInvoked = true;
            extractorSourcePath = sourcePath;
            extractorTargetPath = targetPath;
            return OperationResult::ok();
        };

    const OperationResult result = CoreUpdatePackageInstallation::installPackage(
        targetDirectory.path(),
        asset,
        QByteArray("dummy-gzip-bytes"),
        /*ignoreGeoUpdateCore=*/true,
        extractor,
        /*cancelCheck=*/{},
        /*progressHandler=*/{});

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(extractorInvoked);
    QVERIFY(extractorSourcePath.endsWith(asset.name));
    QVERIFY(extractorTargetPath.contains(QStringLiteral("mihomo.exe")));

    // A cancellation reported by the gzip extraction must propagate as cancelled
    // rather than being flattened into a generic install failure.
    const CoreUpdateService::ArchiveExtractor cancellingExtractor =
        [](const QString&, const QString&) {
            return OperationResult::cancel(QStringLiteral("Gzip extraction was canceled."));
        };

    const OperationResult cancelledResult = CoreUpdatePackageInstallation::installPackage(
        targetDirectory.path(),
        asset,
        QByteArray("dummy-gzip-bytes"),
        /*ignoreGeoUpdateCore=*/true,
        cancellingExtractor,
        /*cancelCheck=*/{},
        /*progressHandler=*/{});

    QVERIFY(!cancelledResult.success);
    QVERIFY(cancelledResult.cancelled);
}

QTEST_MAIN(CoreUpdateServiceTests)

#include "CoreUpdateServiceTests.moc"
