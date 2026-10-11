#include <QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QTemporaryDir>

#include <algorithm>
#include <atomic>
#include <thread>

#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/CoreCatalog.h"
#include "runtime/core/ICoreBackend.h"
#include "services/CoreUpdateOperations.h"
#include "services/CoreUpdatePackageInstallation.h"
#include "services/CoreUpdateReleaseMetadata.h"
#include "services/CoreUpdateService.h"
#include "services/CoreUpdateVersion.h"

namespace {

// The four platforms the release-asset rules have to answer for. Spelled out here rather than
// derived from the host, because these tests exist precisely to cover the platforms the host is not:
// no CI job runs this suite on macOS, so a macOS naming rule can only ever be verified by asking for
// it explicitly.
CoreAssetPlatform windowsX64Platform()
{
    return CoreAssetPlatform{CoreAssetPlatform::Os::Windows, /*appleSilicon=*/false, /*sixtyFourBit=*/true};
}

CoreAssetPlatform windowsX86Platform()
{
    return CoreAssetPlatform{CoreAssetPlatform::Os::Windows, /*appleSilicon=*/false, /*sixtyFourBit=*/false};
}

CoreAssetPlatform macosArm64Platform()
{
    return CoreAssetPlatform{CoreAssetPlatform::Os::MacOS, /*appleSilicon=*/true, /*sixtyFourBit=*/true};
}

CoreAssetPlatform macosIntelPlatform()
{
    return CoreAssetPlatform{CoreAssetPlatform::Os::MacOS, /*appleSilicon=*/false, /*sixtyFourBit=*/true};
}

// Every asset a real release publishes for the platforms this app ships on, so the selection under
// test is the one a live release payload would produce rather than a list of one.
QList<CoreUpdateReleaseMetadata::GitHubReleaseAsset> releaseAssetsFor(CoreType coreType)
{
    QStringList names;
    switch (coreType) {
    case CoreType::Xray:
        names = QStringList{
            QStringLiteral("Xray-windows-64.zip"),
            QStringLiteral("Xray-windows-32.zip"),
            QStringLiteral("Xray-windows-arm64-v8a.zip"),
            QStringLiteral("Xray-macos-64.zip"),
            QStringLiteral("Xray-macos-64.zip.dgst"),
            QStringLiteral("Xray-macos-arm64-v8a.zip"),
        };
        break;
    case CoreType::SingBox:
        names = QStringList{
            QStringLiteral("sing-box-1.14.2-windows-amd64.zip"),
            QStringLiteral("sing-box-1.14.2-windows-386.zip"),
            QStringLiteral("sing-box-1.14.2-windows-arm64.zip"),
            QStringLiteral("sing-box-1.14.2-windows-amd64-legacy-windows-7.zip"),
            QStringLiteral("sing-box-1.14.2-darwin-amd64.tar.gz"),
            QStringLiteral("sing-box-1.14.2-darwin-amd64-legacy-macos-10.13.tar.gz"),
            QStringLiteral("sing-box-1.14.2-darwin-arm64.tar.gz"),
        };
        break;
    case CoreType::Mihomo:
        names = QStringList{
            QStringLiteral("mihomo-windows-amd64-v1-v1.19.25.zip"),
            QStringLiteral("mihomo-windows-amd64-compatible-v1.19.25.zip"),
            QStringLiteral("mihomo-windows-386-v1.19.25.zip"),
            QStringLiteral("mihomo-windows-arm64-v1.19.25.zip"),
            QStringLiteral("mihomo-darwin-amd64-v1.19.25.gz"),
            QStringLiteral("mihomo-darwin-amd64-v1-go122-v1.19.25.gz"),
            QStringLiteral("mihomo-darwin-amd64-compatible-v1.19.25.gz"),
            QStringLiteral("mihomo-darwin-arm64-v1.19.25.gz"),
        };
        break;
    default:
        break;
    }

    QList<CoreUpdateReleaseMetadata::GitHubReleaseAsset> assets;
    assets.reserve(names.size());
    for (const QString& name : names) {
        assets.append(CoreUpdateReleaseMetadata::GitHubReleaseAsset{
            name,
            QUrl(QStringLiteral("https://example.invalid/") + name)});
    }
    return assets;
}

// A GitHub releases payload carrying every asset a real release publishes, so a case that needs the
// update to get as far as downloading an asset does not also depend on which platform the suite runs
// on. The single-asset payload this replaces named the Windows zip, which scored only on macOS while
// the platform detection was broken; once detection was fixed the case failed there with no asset
// selected, which is the update stopping before the download it exists to interrupt.
QByteArray releasePayloadFor(CoreType coreType, const QString& tagName)
{
    QJsonArray assets;
    for (const CoreUpdateReleaseMetadata::GitHubReleaseAsset& asset : releaseAssetsFor(coreType)) {
        QJsonObject entry;
        entry.insert(QStringLiteral("name"), asset.name);
        entry.insert(QStringLiteral("browser_download_url"), asset.downloadUrl.toString());
        assets.append(entry);
    }

    QJsonObject release;
    release.insert(QStringLiteral("tag_name"), tagName);
    release.insert(QStringLiteral("prerelease"), false);
    release.insert(QStringLiteral("assets"), assets);

    QJsonArray releases;
    releases.append(release);
    return QJsonDocument(releases).toJson(QJsonDocument::Compact);
}

} // namespace

class CoreUpdateServiceTests : public QObject {
    Q_OBJECT

private slots:
    void backendMetadataMatchesCatalog();
    void currentAssetPlatformMatchesTheBuildHost();
    void versionComparisonNormalizesTagsAndComparesNumericParts();
    void updateReturnsPromptlyWhenCancellationRequestedDuringDownload();
    void updateFallsBackToBuiltInSingBoxVersionWhenReleaseApiUnavailableAndNoCoreInstalled();
    void updateUsesBuiltInXrayBootstrapVersionWhenNoCoreInstalled();
    void updateGzipInstallUsesInjectedExtractorAndRespectsCancellation();
    void tarGzInstallExtractsIntoTheTargetDirectory();
    void assetNamesFollowTheTargetPlatform();
    void extractionCommandsFollowTheTargetPlatform();
    void helperProcessReportsAFailedLaunch();
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

void CoreUpdateServiceTests::currentAssetPlatformMatchesTheBuildHost()
{
    // The one case here that covers detection itself rather than a fixture. Every other test passes a
    // platform in, which is what makes the naming rules verifiable from any host -- and is also why a
    // detection that reports the wrong operating system used to break nothing visible. That is the bug
    // this guards: the function's translation unit pulled in no Qt header, so Q_OS_WIN and Q_OS_MACOS
    // were both undefined, os stayed Os::Other, and Other falls through to the Windows branch of every
    // backend. Windows hid the mistake by accident; macOS downloaded and installed the Windows core.
    //
    // Comparing against *this* file's compile-time facts is what gives the case teeth: the expectation
    // does not come from the function under test, and the two live in different translation units.
    const CoreAssetPlatform platform = currentAssetPlatform();

#if defined(Q_OS_WIN)
    QVERIFY2(platform.isWindows(), "A Windows build has to resolve Windows release assets");
    QVERIFY(!platform.isMacOS());
#elif defined(Q_OS_MACOS)
    QVERIFY2(platform.isMacOS(), "A macOS build has to resolve macOS release assets");
    QVERIFY(!platform.isWindows());
    // The slice the process runs as decides the architecture, so appleSilicon is deliberately not
    // asserted here -- an Intel build under Rosetta is a legitimate x86_64 process on ARM hardware.
    QVERIFY(platform.sixtyFourBit);
#else
    QSKIP("This host publishes no release assets, so detection has nothing to agree with");
#endif
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
    // Every platform's asset, so the case reaches the download on whichever platform it runs on. The
    // Windows-only payload it used to carry scored no asset at all on macOS once detection was fixed.
    const QByteArray releasePayload = releasePayloadFor(CoreType::SingBox, QStringLiteral("v1.14.2"));

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

            // Report it the way the real downloader does. Returning a plain failure instead made this
            // case depend on the mirror list: only a github.com asset gets four candidate URLs, so
            // what used to produce the cancelled result was the download loop's own cancellation check
            // on the *next* iteration. An asset on any other host yields one candidate, the loop
            // exhausts, and the cancellation comes back as an ordinary download failure.
            return CoreUpdateOperations::cancelledResult();
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
    QVERIFY2(result.cancelled,
             qPrintable(QStringLiteral("message=%1 downloadAttempts=%2")
                            .arg(result.message)
                            .arg(downloadAttempts)));
    QVERIFY(timer.elapsed() < 1500);
    QVERIFY(downloadAttempts >= 2);
}

void CoreUpdateServiceTests::updateFallsBackToBuiltInSingBoxVersionWhenReleaseApiUnavailableAndNoCoreInstalled()
{
    const ICoreBackend* backend = coreBackend(CoreType::SingBox);
    QVERIFY(backend != nullptr);

    // The asset this run expects is the one the running platform's own policy names, so the case
    // says the same thing everywhere instead of only on the platform that publishes .exe files.
    const CoreUpdateReleaseMetadata::GitHubRelease builtIn =
        CoreUpdateReleaseMetadata::buildBuiltInFallbackRelease(*backend, currentAssetPlatform());
    QVERIFY(!builtIn.tagName.isEmpty());
    QCOMPARE(builtIn.assets.size(), 1);
    const QString builtInAssetUrl = builtIn.assets.constFirst().downloadUrl.toString(QUrl::FullyEncoded);

    QStringList requestedUrls;

    CoreUpdateService service(
        [&](const QUrl& url, QByteArray* content) {
            requestedUrls.append(url.toString(QUrl::FullyEncoded));
            if (url.toString().contains(QStringLiteral("api.github.com/repos/SagerNet/sing-box/releases"))) {
                return OperationResult::fail(QStringLiteral("GitHub API unavailable"));
            }
            if (url.toString(QUrl::FullyEncoded) == builtInAssetUrl) {
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

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(requestedUrls.contains(QStringLiteral("https://api.github.com/repos/SagerNet/sing-box/releases?per_page=20")));
    QVERIFY(std::any_of(
        requestedUrls.cbegin(),
        requestedUrls.cend(),
        [&builtInAssetUrl](const QString& url) {
            return url == builtInAssetUrl;
        }));
    QVERIFY(result.message.contains(builtIn.tagName));
}

void CoreUpdateServiceTests::updateUsesBuiltInXrayBootstrapVersionWhenNoCoreInstalled()
{
    const ICoreBackend* backend = coreBackend(CoreType::Xray);
    QVERIFY(backend != nullptr);

    const CoreUpdateReleaseMetadata::GitHubRelease builtIn =
        CoreUpdateReleaseMetadata::buildBuiltInFallbackRelease(*backend, currentAssetPlatform());
    QVERIFY(!builtIn.tagName.isEmpty());
    QCOMPARE(builtIn.assets.size(), 1);
    const QString builtInAssetUrl = builtIn.assets.constFirst().downloadUrl.toString(QUrl::FullyEncoded);

    QStringList requestedUrls;

    CoreUpdateService service(
        [&](const QUrl& url, QByteArray* content) {
            requestedUrls.append(url.toString(QUrl::FullyEncoded));
            if (url.toString(QUrl::FullyEncoded) == builtInAssetUrl) {
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

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(std::any_of(
        requestedUrls.cbegin(),
        requestedUrls.cend(),
        [&builtInAssetUrl](const QString& url) {
            return url == builtInAssetUrl;
        }));
    QVERIFY(result.message.contains(builtIn.tagName));
    QVERIFY(!std::any_of(
        requestedUrls.cbegin(),
        requestedUrls.cend(),
        [](const QString& url) {
            return url.contains(QStringLiteral("/releases/latest/download/"));
        }));
}

void CoreUpdateServiceTests::updateGzipInstallUsesInjectedExtractorAndRespectsCancellation()
{
    // The .gz install path must route through the injected ArchiveExtractor seam, just like .zip
    // does, instead of always shelling out to the platform's own tool.
    //
    // The mihomo gzip names carry the platform and microarchitecture rather than the executable
    // name, so the installed file name is a mapping and not a suffix strip; both platform spellings
    // are covered because the mapping has a branch for each.
    const QList<QPair<QString, QString>> assetNames{
        {QStringLiteral("mihomo-windows-amd64-v1-v1.19.25.gz"), QStringLiteral("mihomo.exe")},
        {QStringLiteral("mihomo-darwin-arm64-v1.19.25.gz"), QStringLiteral("mihomo")},
    };

    for (const QPair<QString, QString>& expected : assetNames) {
        bool extractorInvoked = false;
        QString extractorSourcePath;
        QString extractorTargetPath;

        CoreUpdateReleaseMetadata::GitHubReleaseAsset asset;
        asset.name = expected.first;

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
        QVERIFY2(
            extractorTargetPath.endsWith(expected.second),
            qPrintable(extractorTargetPath));

        // A cancellation reported by the gzip extraction must propagate as cancelled rather than
        // being flattened into a generic install failure.
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
}

void CoreUpdateServiceTests::tarGzInstallExtractsIntoTheTargetDirectory()
{
    // sing-box publishes .tar.gz on macOS, and ".tar.gz" ends with ".gz" -- so the tarball branch
    // has to be reached before the bare-gzip one, or the archive is decompressed into a single file
    // named after itself instead of being unpacked.
    QTemporaryDir targetDirectory;
    QVERIFY(targetDirectory.isValid());

    CoreUpdateReleaseMetadata::GitHubReleaseAsset asset;
    asset.name = QStringLiteral("sing-box-1.14.2-darwin-arm64.tar.gz");

    bool extractorInvoked = false;
    const CoreUpdateService::ArchiveExtractor extractor =
        [&](const QString&, const QString& extractionDirectory) {
            extractorInvoked = true;

            // What tar -xzf produces for this release: one top-level directory holding the core.
            const QString nested = QDir(extractionDirectory).filePath(QStringLiteral("sing-box-1.14.2-darwin-arm64"));
            if (!QDir().mkpath(nested)) {
                return OperationResult::fail(QStringLiteral("could not stage the archive contents"));
            }

            QFile staged(QDir(nested).filePath(QStringLiteral("sing-box")));
            if (!staged.open(QIODevice::WriteOnly)) {
                return OperationResult::fail(QStringLiteral("could not stage the core"));
            }
            staged.write(QByteArray("dummy-core"));
            staged.close();
            QFile::setPermissions(
                staged.fileName(),
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            return OperationResult::ok();
        };

    const OperationResult result = CoreUpdatePackageInstallation::installPackage(
        targetDirectory.path(),
        asset,
        QByteArray("dummy-tar-gz-bytes"),
        /*ignoreGeoUpdateCore=*/true,
        extractor,
        /*cancelCheck=*/{},
        /*progressHandler=*/{});

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(extractorInvoked);

    // The archive's single top-level directory is flattened away.
    const QString installedCore = QDir(targetDirectory.path()).filePath(QStringLiteral("sing-box"));
    QVERIFY2(QFileInfo::exists(installedCore), qPrintable(installedCore));

#if !defined(Q_OS_WIN)
    // And the executable bit survives the copy. Without it macOS refuses to start the core at all,
    // which is the whole reason the copy preserves it. Windows has no such bit to preserve, so
    // there is nothing to assert there.
    QVERIFY(QFile(installedCore).permissions().testFlag(QFileDevice::ExeOwner));
#endif
}

void CoreUpdateServiceTests::assetNamesFollowTheTargetPlatform()
{
    // The asset naming rules are the one part of the update path a test cannot reach by running on
    // the machine under test -- no CI job runs this suite on macOS -- so every platform the app
    // ships on is asked for explicitly, from whichever host is running.
    struct Case {
        CoreType coreType;
        CoreAssetPlatform platform;
        QString expectedAssetName;
    };

    const QList<Case> cases{
        {CoreType::Xray, windowsX64Platform(), QStringLiteral("Xray-windows-64.zip")},
        {CoreType::Xray, windowsX86Platform(), QStringLiteral("Xray-windows-32.zip")},
        {CoreType::Xray, macosArm64Platform(), QStringLiteral("Xray-macos-arm64-v8a.zip")},
        {CoreType::Xray, macosIntelPlatform(), QStringLiteral("Xray-macos-64.zip")},

        {CoreType::SingBox, windowsX64Platform(), QStringLiteral("sing-box-1.14.2-windows-amd64.zip")},
        {CoreType::SingBox, windowsX86Platform(), QStringLiteral("sing-box-1.14.2-windows-386.zip")},
        {CoreType::SingBox, macosArm64Platform(), QStringLiteral("sing-box-1.14.2-darwin-arm64.tar.gz")},
        {CoreType::SingBox, macosIntelPlatform(), QStringLiteral("sing-box-1.14.2-darwin-amd64.tar.gz")},

        {CoreType::Mihomo, windowsX64Platform(), QStringLiteral("mihomo-windows-amd64-v1-v1.19.25.zip")},
        {CoreType::Mihomo, windowsX86Platform(), QStringLiteral("mihomo-windows-386-v1.19.25.zip")},
        {CoreType::Mihomo, macosArm64Platform(), QStringLiteral("mihomo-darwin-arm64-v1.19.25.gz")},
        {CoreType::Mihomo, macosIntelPlatform(), QStringLiteral("mihomo-darwin-amd64-v1.19.25.gz")},
    };

    for (const Case& testCase : cases) {
        const ICoreBackend* backend = coreBackend(testCase.coreType);
        QVERIFY2(backend != nullptr, qPrintable(catalogCoreDisplayName(testCase.coreType)));

        const CoreUpdateAssetPolicy policy = backend->updateAssetPolicy(testCase.platform);
        const QString policyAssetName = testCase.platform.sixtyFourBit
            ? policy.builtInFallbackAssetName64
            : policy.builtInFallbackAssetName32;
        QCOMPARE(policyAssetName, testCase.expectedAssetName);

        // The bootstrap release the policy builds has to point at that same asset, since the URL it
        // carries is what the download then uses when GitHub's release lookup is unavailable.
        const CoreUpdateReleaseMetadata::GitHubRelease fallback =
            CoreUpdateReleaseMetadata::buildBuiltInFallbackRelease(*backend, testCase.platform);
        QCOMPARE(fallback.assets.size(), 1);
        QCOMPARE(fallback.assets.constFirst().name, testCase.expectedAssetName);
        QVERIFY(fallback.assets.constFirst().downloadUrl.toString().endsWith(testCase.expectedAssetName));

        // And the scoring has to pick it out of a list that also holds every other platform's
        // asset, which is the decision a real release payload drives.
        const QList<CoreUpdateReleaseMetadata::GitHubReleaseAsset> assets = releaseAssetsFor(testCase.coreType);
        const CoreUpdateReleaseMetadata::GitHubReleaseAsset* selected =
            CoreUpdateReleaseMetadata::selectBestReleaseAsset(*backend, assets, testCase.platform);
        QVERIFY2(selected != nullptr, qPrintable(testCase.expectedAssetName));
        QCOMPARE(selected->name, testCase.expectedAssetName);
    }

    // The scoring has to reject a release that carries only the other platforms' assets rather than
    // silently falling back to one of them.
    for (const CoreType coreType : {CoreType::Xray, CoreType::SingBox, CoreType::Mihomo}) {
        const ICoreBackend* backend = coreBackend(coreType);
        QVERIFY(backend != nullptr);

        QList<CoreUpdateReleaseMetadata::GitHubReleaseAsset> windowsOnly;
        QList<CoreUpdateReleaseMetadata::GitHubReleaseAsset> macosOnly;
        for (const CoreUpdateReleaseMetadata::GitHubReleaseAsset& asset : releaseAssetsFor(coreType)) {
            const QString normalized = asset.name.toLower();
            if (normalized.contains(QStringLiteral("windows"))) {
                windowsOnly.append(asset);
            } else if (normalized.contains(QStringLiteral("macos")) || normalized.contains(QStringLiteral("darwin"))) {
                macosOnly.append(asset);
            }
        }

        QVERIFY(!windowsOnly.isEmpty());
        QVERIFY(!macosOnly.isEmpty());
        QVERIFY(CoreUpdateReleaseMetadata::selectBestReleaseAsset(*backend, windowsOnly, macosArm64Platform()) == nullptr);
        QVERIFY(CoreUpdateReleaseMetadata::selectBestReleaseAsset(*backend, macosOnly, windowsX64Platform()) == nullptr);
    }
}

void CoreUpdateServiceTests::extractionCommandsFollowTheTargetPlatform()
{
    // The commands are data so that the ones a platform other than the running one would use can be
    // asserted here. That is the only coverage the macOS extraction path can get while the suite
    // runs on Windows.
    const QString archivePath = QStringLiteral("/tmp/pkg.zip");
    const QString directory = QStringLiteral("/tmp/out");

    const CoreUpdateOperations::HelperCommand windowsZip =
        CoreUpdateOperations::archiveExtractionCommand(windowsX64Platform(), archivePath, directory);
    QCOMPARE(windowsZip.program, QStringLiteral("powershell"));
    QVERIFY(windowsZip.arguments.contains(QStringLiteral("-Command")));
    QVERIFY(windowsZip.standardOutputFile.isEmpty());

    // ditto rather than unzip: it is the one that restores the permission bits inside the tree, and
    // the core's executable bit is exactly what the extraction has to preserve.
    const CoreUpdateOperations::HelperCommand macosZip =
        CoreUpdateOperations::archiveExtractionCommand(macosArm64Platform(), archivePath, directory);
    QCOMPARE(macosZip.program, QStringLiteral("/usr/bin/ditto"));
    QCOMPARE(
        macosZip.arguments,
        QStringList({QStringLiteral("-x"), QStringLiteral("-k"), archivePath, directory}));
    QVERIFY(macosZip.standardOutputFile.isEmpty());

    const QString tarPath = QStringLiteral("/tmp/pkg.tar.gz");
    const CoreUpdateOperations::HelperCommand macosTar =
        CoreUpdateOperations::tarGzExtractionCommand(macosArm64Platform(), tarPath, directory);
    QCOMPARE(macosTar.program, QStringLiteral("/usr/bin/tar"));
    QCOMPARE(
        macosTar.arguments,
        QStringList({QStringLiteral("-xzf"), tarPath, QStringLiteral("-C"), directory}));

    // The gzip payload is the core itself, so stdout has to be redirected into a file rather than
    // left to land in the parent's own stream.
    const QString gzipPath = QStringLiteral("/tmp/core.gz");
    const QString outputPath = QStringLiteral("/tmp/core");
    const CoreUpdateOperations::HelperCommand macosGzip =
        CoreUpdateOperations::gzipDecompressionCommand(macosArm64Platform(), gzipPath, outputPath);
    QCOMPARE(macosGzip.program, QStringLiteral("/usr/bin/gzip"));
    QCOMPARE(macosGzip.arguments, QStringList({QStringLiteral("-dc"), gzipPath}));
    QCOMPARE(macosGzip.standardOutputFile, outputPath);

    // The Windows form writes its own output file, so the staging path is embedded in the command
    // and there is nothing to redirect.
    const CoreUpdateOperations::HelperCommand windowsGzip =
        CoreUpdateOperations::gzipDecompressionCommand(windowsX64Platform(), gzipPath, outputPath);
    QCOMPARE(windowsGzip.program, QStringLiteral("powershell"));
    QVERIFY(windowsGzip.standardOutputFile.isEmpty());
}

void CoreUpdateServiceTests::helperProcessReportsAFailedLaunch()
{
    // The runner's launch-failure branch is what turns "the platform has no such tool" into a
    // message instead of a silent success, so it is worth pinning independently of the extraction
    // paths that use it.
    const CoreUpdateOperations::HelperProcessOutcome outcome = CoreUpdateOperations::runHelperProcess(
        CoreUpdateOperations::HelperCommand{QStringLiteral("songbird-no-such-helper-program"), {}, {}},
        5000,
        /*cancelCheck=*/{});

    QVERIFY(outcome.status == CoreUpdateOperations::HelperProcessOutcome::Status::StartFailed);
    QVERIFY(!outcome.errorText.isEmpty());
}

QTEST_MAIN(CoreUpdateServiceTests)

#include "CoreUpdateServiceTests.moc"
