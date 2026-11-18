#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "services/AppUpdateService.h"

namespace {

CoreAssetPlatform windowsPlatform()
{
    CoreAssetPlatform platform;
    platform.os = CoreAssetPlatform::Os::Windows;
    platform.sixtyFourBit = true;
    return platform;
}

CoreAssetPlatform macosPlatform()
{
    CoreAssetPlatform platform;
    platform.os = CoreAssetPlatform::Os::MacOS;
    platform.appleSilicon = true;
    platform.sixtyFourBit = true;
    return platform;
}

} // namespace

class AppUpdateServiceTests : public QObject {
    Q_OBJECT

private slots:
    void checkFindsNewerStableRelease();
    void checkSkipsPrereleaseWhenDisabled();
    void checkAllowsPrereleaseWhenEnabled();
    void checkReportsUpToDate();
    void checkFindsStableReleaseNewerThanTheInstalledPrerelease();
    void checkFindsNextPrereleaseForTheInstalledPrerelease();
    void assetSelectionFollowsTheTargetPlatform();
    void checkCarriesTheAssetDigestAndChecksumAsset();
    void checkRecordsOfficialMetadataProvenance();
    void checkRecordsMirrorMetadataProvenance();
    void downloadSavesAVerifiedUpdate();
    void downloadSavesAMirrorVerifiedUpdateWithWarning();
    void downloadRefusesAnUnverifiedUpdate();
    void downloadRefusesAnUpdateOnChecksumMismatch();
    void downloadRefusesAMirrorMismatch();
};

void AppUpdateServiceTests::checkFindsNewerStableRelease()
{
    QString requestedUrl;
    AppUpdateService service([&requestedUrl](const QUrl& url, QByteArray* content) {
        requestedUrl = url.toString(QUrl::FullyEncoded);
        *content = R"([{"tag_name":"v1.2.0","name":"SongBird 1.2.0","html_url":"https://github.com/o3ku/songbird/releases/tag/v1.2.0","draft":false,"prerelease":false}])";
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    const OperationResult op =
        service.checkForUpdate(QStringLiteral("1.1.0"), false, windowsPlatform(), &result);

    QVERIFY(op.success);
    QVERIFY(result.updateAvailable);
    QCOMPARE(result.currentVersion, QStringLiteral("1.1.0"));
    QCOMPARE(result.latestVersion, QStringLiteral("v1.2.0"));
    QCOMPARE(result.releaseName, QStringLiteral("SongBird 1.2.0"));
    QCOMPARE(result.releaseUrl.toString(), QStringLiteral("https://github.com/o3ku/songbird/releases/tag/v1.2.0"));
    QVERIFY(requestedUrl.contains(QStringLiteral("api.github.com/repos/o3ku/SongBird/releases")));
}

void AppUpdateServiceTests::checkSkipsPrereleaseWhenDisabled()
{
    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content =
            R"([)"
            R"({"tag_name":"v2.0.0-beta.1","draft":false,"prerelease":true},)"
            R"({"tag_name":"v1.9.0","draft":false,"prerelease":false})"
            R"(])";
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    const OperationResult op =
        service.checkForUpdate(QStringLiteral("1.8.0"), false, windowsPlatform(), &result);

    QVERIFY(op.success);
    QVERIFY(result.updateAvailable);
    QCOMPARE(result.latestVersion, QStringLiteral("v1.9.0"));
}

void AppUpdateServiceTests::checkAllowsPrereleaseWhenEnabled()
{
    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content =
            R"([)"
            R"({"tag_name":"v2.0.0-beta.1","draft":false,"prerelease":true},)"
            R"({"tag_name":"v1.9.0","draft":false,"prerelease":false})"
            R"(])";
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    const OperationResult op =
        service.checkForUpdate(QStringLiteral("1.8.0"), true, windowsPlatform(), &result);

    QVERIFY(op.success);
    QVERIFY(result.updateAvailable);
    QCOMPARE(result.latestVersion, QStringLiteral("v2.0.0-beta.1"));
}

void AppUpdateServiceTests::checkReportsUpToDate()
{
    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content = R"([{"tag_name":"v1.2.0","draft":false,"prerelease":false}])";
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    const OperationResult op =
        service.checkForUpdate(QStringLiteral("1.2.0"), false, windowsPlatform(), &result);

    QVERIFY(op.success);
    QVERIFY(!result.updateAvailable);
    QCOMPARE(result.latestVersion, QStringLiteral("v1.2.0"));
    QVERIFY(op.message.contains(QStringLiteral("up to date")));
}

// A beta has to be able to see the release it exists to test. Comparing the tags while ignoring
// the prerelease suffix made `2.5.0` look equal to `2.5.0-beta1`, so every beta user would have
// been told "up to date" on the day 2.5.0 shipped.
void AppUpdateServiceTests::checkFindsStableReleaseNewerThanTheInstalledPrerelease()
{
    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content =
            R"([)"
            R"({"tag_name":"v2.5.0","draft":false,"prerelease":false},)"
            R"({"tag_name":"v2.5.0-beta2","draft":false,"prerelease":true},)"
            R"({"tag_name":"v2.4.5","draft":false,"prerelease":false})"
            R"(])";
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    const OperationResult op =
        service.checkForUpdate(QStringLiteral("2.5.0-beta1"), false, windowsPlatform(), &result);

    QVERIFY(op.success);
    QVERIFY(result.updateAvailable);
    QCOMPARE(result.latestVersion, QStringLiteral("v2.5.0"));
}

// Same triple, different prerelease: `2.5.0-beta2` is newer than `2.5.0-beta1`.
void AppUpdateServiceTests::checkFindsNextPrereleaseForTheInstalledPrerelease()
{
    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content =
            R"([)"
            R"({"tag_name":"v2.5.0-beta2","draft":false,"prerelease":true,"assets":[)"
            R"({"name":"songbird.exe","browser_download_url":"https://example.invalid/songbird.exe"}]},)"
            R"({"tag_name":"v2.5.0-beta1","draft":false,"prerelease":true},)"
            R"({"tag_name":"v2.4.5","draft":false,"prerelease":false})"
            R"(])";
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    const OperationResult op =
        service.checkForUpdate(QStringLiteral("2.5.0-beta1"), true, windowsPlatform(), &result);

    QVERIFY(op.success);
    QVERIFY(result.updateAvailable);
    QCOMPARE(result.latestVersion, QStringLiteral("v2.5.0-beta2"));
    QCOMPARE(result.assetName, QStringLiteral("songbird.exe"));
}

// Scoring only `.exe` handed a macOS user the Windows build of the very same release, because
// nothing about `songbird.exe` says "Windows" to the old name heuristics.
void AppUpdateServiceTests::assetSelectionFollowsTheTargetPlatform()
{
    const QByteArray payload =
        R"([{"tag_name":"v2.5.0","draft":false,"prerelease":false,"assets":[)"
        R"({"name":"SongBird-macos-universal.dmg","browser_download_url":"https://example.invalid/SongBird-macos-universal.dmg"},)"
        R"({"name":"songbird.exe","browser_download_url":"https://example.invalid/songbird.exe"}]}])";
    AppUpdateService service([&payload](const QUrl&, QByteArray* content) {
        *content = payload;
        return OperationResult::ok();
    });

    AppUpdateCheckResult macResult;
    QVERIFY(service.checkForUpdate(QStringLiteral("2.4.5"), false, macosPlatform(), &macResult).success);
    QVERIFY(macResult.updateAvailable);
    QCOMPARE(macResult.assetName, QStringLiteral("SongBird-macos-universal.dmg"));
    QCOMPARE(macResult.assetDownloadUrl.toString(),
             QStringLiteral("https://example.invalid/SongBird-macos-universal.dmg"));

    AppUpdateCheckResult windowsResult;
    QVERIFY(service.checkForUpdate(QStringLiteral("2.4.5"), false, windowsPlatform(), &windowsResult).success);
    QVERIFY(windowsResult.updateAvailable);
    QCOMPARE(windowsResult.assetName, QStringLiteral("songbird.exe"));

    // A Windows-only release leaves a macOS user with no asset; the coordinator then offers the
    // release page instead of downloading an executable this platform cannot run.
    const QByteArray windowsOnlyPayload =
        R"([{"tag_name":"v2.5.0","draft":false,"prerelease":false,"assets":[)"
        R"({"name":"songbird.exe","browser_download_url":"https://example.invalid/songbird.exe"}]}])";
    AppUpdateService windowsOnlyService([&windowsOnlyPayload](const QUrl&, QByteArray* content) {
        *content = windowsOnlyPayload;
        return OperationResult::ok();
    });

    AppUpdateCheckResult noAssetResult;
    QVERIFY(windowsOnlyService.checkForUpdate(QStringLiteral("2.4.5"), false, macosPlatform(), &noAssetResult).success);
    QVERIFY(noAssetResult.updateAvailable);
    QVERIFY(noAssetResult.assetName.isEmpty());
    QVERIFY(!noAssetResult.assetDownloadUrl.isValid());
}

void AppUpdateServiceTests::checkCarriesTheAssetDigestAndChecksumAsset()
{
    // SHA-256 of "update-bytes".
    const QString updateDigest = QStringLiteral("92439dd4e7652a0414d2cd9d220e8d2767ed71f9005327223fc5d12b7a5065c3");
    const QByteArray payload =
        R"([{"tag_name":"v2.5.0","draft":false,"prerelease":false,"assets":[)"
        R"({"name":"songbird.exe","browser_download_url":"https://example.invalid/songbird.exe","digest":"sha256:)" + updateDigest.toUtf8() + R"("},)"
        R"({"name":"songbird.exe.sha256","browser_download_url":"https://example.invalid/songbird.exe.sha256"}]}])";
    AppUpdateService service([&payload](const QUrl&, QByteArray* content) {
        *content = payload;
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    QVERIFY(service.checkForUpdate(QStringLiteral("2.4.5"), false, windowsPlatform(), &result).success);
    QCOMPARE(result.assetName, QStringLiteral("songbird.exe"));
    QCOMPARE(result.assetDigest, updateDigest);
    QCOMPARE(result.checksumAssetName, QStringLiteral("songbird.exe.sha256"));
    QCOMPARE(result.checksumAssetDownloadUrl.toString(),
             QStringLiteral("https://example.invalid/songbird.exe.sha256"));
    // The metadata arrived on the first attempt, from the official channel.
    QVERIFY(result.metadataFromOfficial);
}

void AppUpdateServiceTests::checkRecordsOfficialMetadataProvenance()
{
    const QByteArray payload =
        R"([{"tag_name":"v2.5.0","draft":false,"prerelease":false,"assets":[)"
        R"({"name":"songbird.exe","browser_download_url":"https://example.invalid/songbird.exe"}]}])";
    QList<QUrl> requestedUrls;
    AppUpdateService service([&payload, &requestedUrls](const QUrl& url, QByteArray* content) {
        requestedUrls.append(url);
        *content = payload;
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    QVERIFY(service.checkForUpdate(QStringLiteral("2.4.5"), false, windowsPlatform(), &result).success);
    QVERIFY(result.metadataFromOfficial);
    // Official first: the API is tried on api.github.com before any mirror is reached.
    QVERIFY(!requestedUrls.isEmpty());
    QCOMPARE(requestedUrls.first().host(), QStringLiteral("api.github.com"));
}

void AppUpdateServiceTests::checkRecordsMirrorMetadataProvenance()
{
    const QByteArray payload =
        R"([{"tag_name":"v2.5.0","draft":false,"prerelease":false,"assets":[)"
        R"({"name":"songbird.exe","browser_download_url":"https://example.invalid/songbird.exe"}]}])";
    QList<QUrl> requestedUrls;
    AppUpdateService service([&payload, &requestedUrls](const QUrl& url, QByteArray* content) {
        requestedUrls.append(url);
        if (url.host() == QStringLiteral("api.github.com")) {
            return OperationResult::fail(QStringLiteral("network unreachable"));
        }
        *content = payload;
        return OperationResult::ok();
    });

    AppUpdateCheckResult result;
    QVERIFY(service.checkForUpdate(QStringLiteral("2.4.5"), false, windowsPlatform(), &result).success);
    QVERIFY(result.updateAvailable);
    QVERIFY(!result.metadataFromOfficial);
    // The official channel was tried first and failed; a mirror answered instead.
    QVERIFY(requestedUrls.size() >= 2);
    QCOMPARE(requestedUrls.first().host(), QStringLiteral("api.github.com"));
    QVERIFY(requestedUrls.at(1).host() != QStringLiteral("api.github.com"));
}

void AppUpdateServiceTests::downloadSavesAVerifiedUpdate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content = QByteArray("update-bytes");
        return OperationResult::ok();
    });

    AppUpdateCheckResult checkResult;
    checkResult.assetName = QStringLiteral("songbird.exe");
    checkResult.assetDownloadUrl = QUrl(QStringLiteral("https://example.invalid/songbird.exe"));
    // SHA-256 of "update-bytes".
    checkResult.assetDigest =
        QStringLiteral("92439dd4e7652a0414d2cd9d220e8d2767ed71f9005327223fc5d12b7a5065c3");
    // The digest arrived with official metadata, so it anchors the official pass.
    checkResult.metadataFromOfficial = true;

    QString savedPath;
    const OperationResult result = service.downloadUpdate(checkResult, directory.path(), &savedPath);

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(QFile::exists(savedPath));
    QFile file(savedPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("update-bytes"));
}

void AppUpdateServiceTests::downloadSavesAMirrorVerifiedUpdateWithWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content = QByteArray("update-bytes");
        return OperationResult::ok();
    });

    // The digest is right, but the metadata arrived through a mirror: the official pass
    // cannot use it, so the degraded mirror pass verifies it instead -- and the saved
    // message says so loudly, so the user knows this install is mirror-verified only.
    AppUpdateCheckResult checkResult;
    checkResult.assetName = QStringLiteral("songbird.exe");
    checkResult.assetDownloadUrl = QUrl(QStringLiteral("https://example.invalid/songbird.exe"));
    // SHA-256 of "update-bytes".
    checkResult.assetDigest =
        QStringLiteral("92439dd4e7652a0414d2cd9d220e8d2767ed71f9005327223fc5d12b7a5065c3");
    checkResult.metadataFromOfficial = false;

    QString savedPath;
    const OperationResult result = service.downloadUpdate(checkResult, directory.path(), &savedPath);

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(QFile::exists(savedPath));
    QVERIFY(result.message.contains(QStringLiteral("mirror")));
    QVERIFY(result.message.contains(QStringLiteral("GitHub was unreachable")));
}

void AppUpdateServiceTests::downloadRefusesAnUnverifiedUpdate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content = QByteArray("update-bytes");
        return OperationResult::ok();
    });

    // No digest and no checksum asset: the release cannot be verified, so nothing is saved.
    // Official metadata, so the failure is the official pass reporting unverifiable.
    AppUpdateCheckResult checkResult;
    checkResult.assetName = QStringLiteral("songbird.exe");
    checkResult.assetDownloadUrl = QUrl(QStringLiteral("https://example.invalid/songbird.exe"));
    checkResult.metadataFromOfficial = true;

    QString savedPath;
    const OperationResult result = service.downloadUpdate(checkResult, directory.path(), &savedPath);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("cannot be verified")));
    QVERIFY(savedPath.isEmpty());
    QVERIFY(!QFile::exists(QDir(directory.path()).filePath(QStringLiteral("songbird.exe"))));
}

void AppUpdateServiceTests::downloadRefusesAnUpdateOnChecksumMismatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content = QByteArray("update-bytes");
        return OperationResult::ok();
    });

    AppUpdateCheckResult checkResult;
    checkResult.assetName = QStringLiteral("songbird.exe");
    checkResult.assetDownloadUrl = QUrl(QStringLiteral("https://example.invalid/songbird.exe"));
    // SHA-256 of "something-else", not of the "update-bytes" the download serves.
    checkResult.assetDigest =
        QStringLiteral("3160fbd511a071c87828ece647a54c6aa5a1e1563b7a89d89875af7a63ad327a");
    // Official metadata, so the official pass is what reports the mismatch.
    checkResult.metadataFromOfficial = true;

    QString savedPath;
    const OperationResult result = service.downloadUpdate(checkResult, directory.path(), &savedPath);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("checksum mismatch")));
    QVERIFY(savedPath.isEmpty());
    QVERIFY(!QFile::exists(QDir(directory.path()).filePath(QStringLiteral("songbird.exe"))));
}

void AppUpdateServiceTests::downloadRefusesAMirrorMismatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppUpdateService service([](const QUrl&, QByteArray* content) {
        *content = QByteArray("update-bytes");
        return OperationResult::ok();
    });

    // A mirror-supplied digest that does not match the bytes fails closed in the degraded
    // pass too: the mirror path only relaxes where the anchor comes from, never whether
    // the bytes match it.
    AppUpdateCheckResult checkResult;
    checkResult.assetName = QStringLiteral("songbird.exe");
    checkResult.assetDownloadUrl = QUrl(QStringLiteral("https://example.invalid/songbird.exe"));
    // SHA-256 of "something-else", not of the "update-bytes" the download serves.
    checkResult.assetDigest =
        QStringLiteral("3160fbd511a071c87828ece647a54c6aa5a1e1563b7a89d89875af7a63ad327a");
    checkResult.metadataFromOfficial = false;

    QString savedPath;
    const OperationResult result = service.downloadUpdate(checkResult, directory.path(), &savedPath);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("checksum mismatch")));
    QVERIFY(savedPath.isEmpty());
    QVERIFY(!QFile::exists(QDir(directory.path()).filePath(QStringLiteral("songbird.exe"))));
}

QTEST_MAIN(AppUpdateServiceTests)

#include "AppUpdateServiceTests.moc"
