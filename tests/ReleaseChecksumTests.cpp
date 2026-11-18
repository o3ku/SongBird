#include <QtTest>

#include "common/ReleaseChecksum.h"

namespace {

using ReleaseChecksum::NamedAsset;
using ReleaseChecksum::Verification;
using ReleaseChecksum::VerificationOutcome;

const QString kHelloSha256 = QStringLiteral("2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
const QString kDummyPackageSha256 = QStringLiteral("73f9efcc02e48d777d943d6098d728697b846ddf3a637b04f4b6b10d553eca36");

QUrl releaseUrl(const QString& fileName)
{
    return QUrl(QStringLiteral("https://github.com/XTLS/Xray-core/releases/download/v26.3.27/") + fileName);
}

} // namespace

class ReleaseChecksumTests : public QObject {
    Q_OBJECT

private slots:
    void sha256HexMatchesKnownVector();
    void digestToHexNormalisesApiDigests();
    void matchesSha256AcceptsBothDigestSpellings();
    void findChecksumAssetPrefersPerAssetFiles();
    void expectedSha256ParsesCoreutilsChecksums();
    void expectedSha256ParsesSingleBareHash();
    void expectedSha256ParsesOpensslDigestFiles();
    void derivedChecksumUrlsFollowThePackageUrl();
    void verifyPrefersTheApiDigestAndNeverFetches();
    void verifyIgnoresADigestFromMirrorMetadata();
    void verifyFallsBackToTheChecksumAsset();
    void verifyIgnoresAMirrorHostedChecksumAsset();
    void verifyFallsBackToADerivedChecksumFile();
    void verifyDerivedChecksumNeedsOfficialMetadataOrACodeUrl();
    void verifyIsUnverifiableWhenNoChecksumExists();
    void verifyViaMirrorAcceptsAMirrorSuppliedDigest();
    void verifyViaMirrorAcceptsAMirrorHostedChecksumDocument();
    void verifyViaMirrorRefusesAMirrorMismatch();
    void verifyViaMirrorIsUnverifiableWhenNoChecksumExists();
};

void ReleaseChecksumTests::sha256HexMatchesKnownVector()
{
    QCOMPARE(ReleaseChecksum::sha256Hex(QByteArray("abc")),
             QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

void ReleaseChecksumTests::digestToHexNormalisesApiDigests()
{
    QCOMPARE(ReleaseChecksum::digestToHex(QStringLiteral("sha256:") + kHelloSha256), kHelloSha256);
    QCOMPARE(ReleaseChecksum::digestToHex(kHelloSha256), kHelloSha256);
    QCOMPARE(ReleaseChecksum::digestToHex(QStringLiteral("SHA256:") + kHelloSha256.toUpper()), kHelloSha256);
    QVERIFY(ReleaseChecksum::digestToHex(QStringLiteral("md5:0123456789abcdef")).isEmpty());
    QVERIFY(ReleaseChecksum::digestToHex(QStringLiteral("not a hash")).isEmpty());
    QVERIFY(ReleaseChecksum::digestToHex(QString()).isEmpty());
}

void ReleaseChecksumTests::matchesSha256AcceptsBothDigestSpellings()
{
    QVERIFY(ReleaseChecksum::matchesSha256(QByteArray("hello"), QStringLiteral("sha256:") + kHelloSha256));
    QVERIFY(ReleaseChecksum::matchesSha256(QByteArray("hello"), kHelloSha256));
    QVERIFY(!ReleaseChecksum::matchesSha256(QByteArray("hello"), kDummyPackageSha256));
    QVERIFY(!ReleaseChecksum::matchesSha256(QByteArray("hello"), QString()));
}

void ReleaseChecksumTests::findChecksumAssetPrefersPerAssetFiles()
{
    const QList<NamedAsset> assets{
        NamedAsset{QStringLiteral("Xray-windows-64.zip"), releaseUrl(QStringLiteral("Xray-windows-64.zip"))},
        NamedAsset{QStringLiteral("checksums.txt"), releaseUrl(QStringLiteral("checksums.txt"))},
        NamedAsset{QStringLiteral("Xray-windows-64.zip.dgst"), releaseUrl(QStringLiteral("Xray-windows-64.zip.dgst"))},
        NamedAsset{QStringLiteral("Xray-windows-64.zip.sha256"), releaseUrl(QStringLiteral("Xray-windows-64.zip.sha256"))},
    };
    // A per-asset file wins over the shared checksums.txt, and .sha256 wins over .dgst.
    const std::optional<NamedAsset> found =
        ReleaseChecksum::findChecksumAsset(assets, QStringLiteral("Xray-windows-64.zip"));
    QVERIFY(found.has_value());
    QCOMPARE(found->name, QStringLiteral("Xray-windows-64.zip.sha256"));

    const QList<NamedAsset> dgstOnly{
        NamedAsset{QStringLiteral("Xray-windows-64.zip"), releaseUrl(QStringLiteral("Xray-windows-64.zip"))},
        NamedAsset{QStringLiteral("checksums.txt"), releaseUrl(QStringLiteral("checksums.txt"))},
        NamedAsset{QStringLiteral("Xray-windows-64.zip.dgst"), releaseUrl(QStringLiteral("Xray-windows-64.zip.dgst"))},
    };
    const std::optional<NamedAsset> dgstFound =
        ReleaseChecksum::findChecksumAsset(dgstOnly, QStringLiteral("Xray-windows-64.zip"));
    QVERIFY(dgstFound.has_value());
    QCOMPARE(dgstFound->name, QStringLiteral("Xray-windows-64.zip.dgst"));

    const QList<NamedAsset> sharedOnly{
        NamedAsset{QStringLiteral("Xray-windows-64.zip"), releaseUrl(QStringLiteral("Xray-windows-64.zip"))},
        NamedAsset{QStringLiteral("checksums.txt"), releaseUrl(QStringLiteral("checksums.txt"))},
    };
    const std::optional<NamedAsset> sharedFound =
        ReleaseChecksum::findChecksumAsset(sharedOnly, QStringLiteral("Xray-windows-64.zip"));
    QVERIFY(sharedFound.has_value());
    QCOMPARE(sharedFound->name, QStringLiteral("checksums.txt"));

    const QList<NamedAsset> none{
        NamedAsset{QStringLiteral("Xray-windows-64.zip"), releaseUrl(QStringLiteral("Xray-windows-64.zip"))},
    };
    QVERIFY(!ReleaseChecksum::findChecksumAsset(none, QStringLiteral("Xray-windows-64.zip")).has_value());
}

void ReleaseChecksumTests::expectedSha256ParsesCoreutilsChecksums()
{
    // goreleaser's checksums.txt: one "<hex>  <filename>" line per asset.
    const QByteArray document =
        (kHelloSha256 + QStringLiteral("  hello.txt\n")
         + kDummyPackageSha256 + QStringLiteral("  dummy-package\n")).toUtf8();
    { const std::optional<QString> found =
             ReleaseChecksum::expectedSha256ForAsset(document, QStringLiteral("dummy-package"));
             QVERIFY(found.has_value());
             QCOMPARE(*found, kDummyPackageSha256); }
    { const std::optional<QString> found =
             ReleaseChecksum::expectedSha256ForAsset(document, QStringLiteral("hello.txt"));
             QVERIFY(found.has_value());
             QCOMPARE(*found, kHelloSha256); }
    QVERIFY(!ReleaseChecksum::expectedSha256ForAsset(document, QStringLiteral("missing.zip")).has_value());

    // Binary mode marks the filename with '*'; a path in the document still matches on basename.
    const QByteArray binaryMode =
        (kHelloSha256 + QStringLiteral(" *subdir/hello.txt\n")).toUtf8();
    { const std::optional<QString> found =
             ReleaseChecksum::expectedSha256ForAsset(binaryMode, QStringLiteral("hello.txt"));
             QVERIFY(found.has_value());
             QCOMPARE(*found, kHelloSha256); }
}

void ReleaseChecksumTests::expectedSha256ParsesSingleBareHash()
{
    const QByteArray document = (kHelloSha256 + QStringLiteral("\n")).toUtf8();
    { const std::optional<QString> found =
             ReleaseChecksum::expectedSha256ForAsset(document, QStringLiteral("SongBird.exe"));
             QVERIFY(found.has_value());
             QCOMPARE(*found, kHelloSha256); }

    // Two bare hashes name nothing: the document is ambiguous, not a match.
    const QByteArray ambiguous =
        (kHelloSha256 + QStringLiteral("\n") + kDummyPackageSha256 + QStringLiteral("\n")).toUtf8();
    QVERIFY(!ReleaseChecksum::expectedSha256ForAsset(ambiguous, QStringLiteral("SongBird.exe")).has_value());
}

void ReleaseChecksumTests::expectedSha256ParsesOpensslDigestFiles()
{
    // Xray publishes "<archive>.zip.dgst" in openssl dgst format.
    const QByteArray dgst =
        (QStringLiteral("SHA2-256(Xray-windows-64.zip)= ") + kDummyPackageSha256 + QStringLiteral("\n")).toUtf8();
    { const std::optional<QString> found =
             ReleaseChecksum::expectedSha256ForAsset(dgst, QStringLiteral("Xray-windows-64.zip"));
             QVERIFY(found.has_value());
             QCOMPARE(*found, kDummyPackageSha256); }
    QVERIFY(!ReleaseChecksum::expectedSha256ForAsset(dgst, QStringLiteral("Xray-linux-64.zip")).has_value());

    const QByteArray dgstSpaced =
        (QStringLiteral("SHA256 (Xray-windows-64.zip) = ") + kDummyPackageSha256 + QStringLiteral("\n")).toUtf8();
    { const std::optional<QString> found =
             ReleaseChecksum::expectedSha256ForAsset(dgstSpaced, QStringLiteral("Xray-windows-64.zip"));
             QVERIFY(found.has_value());
             QCOMPARE(*found, kDummyPackageSha256); }

    // Xray's workflow strips the filename with sed, leaving "SHA256= <hex>".
    const QByteArray dgstBare =
        (QStringLiteral("SHA256= ") + kDummyPackageSha256 + QStringLiteral("\n")).toUtf8();
    { const std::optional<QString> found =
             ReleaseChecksum::expectedSha256ForAsset(dgstBare, QStringLiteral("Xray-windows-64.zip"));
             QVERIFY(found.has_value());
             QCOMPARE(*found, kDummyPackageSha256); }
}

void ReleaseChecksumTests::derivedChecksumUrlsFollowThePackageUrl()
{
    const QList<QUrl> urls =
        ReleaseChecksum::derivedChecksumUrls(releaseUrl(QStringLiteral("Xray-windows-64.zip")));
    QCOMPARE(urls.size(), 3);
    QCOMPARE(urls.at(0).toString(),
             QStringLiteral("https://github.com/XTLS/Xray-core/releases/download/v26.3.27/Xray-windows-64.zip.sha256"));
    QCOMPARE(urls.at(1).toString(),
             QStringLiteral("https://github.com/XTLS/Xray-core/releases/download/v26.3.27/Xray-windows-64.zip.dgst"));
    QCOMPARE(urls.at(2).toString(),
             QStringLiteral("https://github.com/XTLS/Xray-core/releases/download/v26.3.27/checksums.txt"));
    QVERIFY(ReleaseChecksum::derivedChecksumUrls(QUrl()).isEmpty());
}

void ReleaseChecksumTests::verifyPrefersTheApiDigestAndNeverFetches()
{
    int fetchCalls = 0;
    const auto fetch = [&fetchCalls](const QUrl&, QByteArray*) {
        ++fetchCalls;
        return OperationResult::fail(QStringLiteral("must not be called"));
    };

    // The digest only anchors the bytes when the metadata that carried it came from the
    // official channel.
    ReleaseChecksum::VerificationRequest official;
    official.packageBytes = QByteArray("hello");
    official.assetName = QStringLiteral("hello.txt");
    official.packageDownloadUrl = releaseUrl(QStringLiteral("hello.txt"));
    official.apiDigest = QStringLiteral("sha256:") + kHelloSha256;
    official.metadataFromOfficial = true;

    const VerificationOutcome verified =
        ReleaseChecksum::verifyDownloadedBytes(official, fetch);
    QCOMPARE(verified.status, Verification::Verified);
    QCOMPARE(verified.expectedHex, kHelloSha256);
    QCOMPARE(verified.actualHex, kHelloSha256);
    QCOMPARE(fetchCalls, 0);

    ReleaseChecksum::VerificationRequest mismatch = official;
    mismatch.apiDigest = QStringLiteral("sha256:") + kDummyPackageSha256;
    const VerificationOutcome mismatched =
        ReleaseChecksum::verifyDownloadedBytes(mismatch, fetch);
    QCOMPARE(mismatched.status, Verification::Mismatched);
    QCOMPARE(mismatched.expectedHex, kDummyPackageSha256);
    QCOMPARE(mismatched.actualHex, kHelloSha256);
    QCOMPARE(fetchCalls, 0);
}

void ReleaseChecksumTests::verifyIgnoresADigestFromMirrorMetadata()
{
    int fetchCalls = 0;
    const auto fetch = [&fetchCalls](const QUrl&, QByteArray*) {
        ++fetchCalls;
        return OperationResult::fail(QStringLiteral("not found"));
    };

    // The same digest, but the metadata arrived through a mirror: a mirror can report any
    // digest it likes for the bytes it serves, so the official pass must not use it. The
    // derived checksum URLs are github.com URLs, but they are only trusted when the
    // metadata was official (or the package URL comes from code), so with neither the
    // result is Unverifiable -- and no checksum fetch happens at all.
    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("hello");
    request.assetName = QStringLiteral("hello.txt");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("hello.txt"));
    request.apiDigest = QStringLiteral("sha256:") + kHelloSha256;
    request.metadataFromOfficial = false;

    const VerificationOutcome outcome =
        ReleaseChecksum::verifyDownloadedBytes(request, fetch);
    QCOMPARE(outcome.status, Verification::Unverifiable);
    QVERIFY(outcome.expectedHex.isEmpty());
    QCOMPARE(fetchCalls, 0);
}

void ReleaseChecksumTests::verifyFallsBackToTheChecksumAsset()
{
    const auto fetch = [](const QUrl& url, QByteArray* content) {
        if (url.toString().endsWith(QStringLiteral("hello.txt.sha256"))) {
            *content = (kHelloSha256 + QStringLiteral("  hello.txt\n")).toUtf8();
            return OperationResult::ok();
        }
        return OperationResult::fail(QStringLiteral("not found"));
    };

    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("hello");
    request.assetName = QStringLiteral("hello.txt");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("hello.txt"));
    request.checksumAsset =
        NamedAsset{QStringLiteral("hello.txt.sha256"), releaseUrl(QStringLiteral("hello.txt.sha256"))};
    request.metadataFromOfficial = true;

    const VerificationOutcome verified =
        ReleaseChecksum::verifyDownloadedBytes(request, fetch);
    QCOMPARE(verified.status, Verification::Verified);

    request.packageBytes = QByteArray("tampered");
    const VerificationOutcome mismatched =
        ReleaseChecksum::verifyDownloadedBytes(request, fetch);
    QCOMPARE(mismatched.status, Verification::Mismatched);
}

void ReleaseChecksumTests::verifyIgnoresAMirrorHostedChecksumAsset()
{
    const auto fetch = [](const QUrl& url, QByteArray* content) {
        // Only the mirror-hosted document answers: the derived official URLs 404, so the
        // only checksum the official pass could use is the mirror's own document.
        if (url.host() == QStringLiteral("ghfast.top")
            && url.toString().endsWith(QStringLiteral("hello.txt.sha256"))) {
            *content = (kHelloSha256 + QStringLiteral("  hello.txt\n")).toUtf8();
            return OperationResult::ok();
        }
        return OperationResult::fail(QStringLiteral("not found"));
    };

    // The metadata is official, but the checksum asset it names lives on a mirror: the
    // document is fetched from the mirror's host, so it cannot anchor the bytes. The
    // derived URLs (github.com, metadata official) are tried instead and 404, leaving
    // the package Unverifiable rather than Verified-against-a-mirror.
    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("hello");
    request.assetName = QStringLiteral("hello.txt");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("hello.txt"));
    request.checksumAsset = NamedAsset{
        QStringLiteral("hello.txt.sha256"),
        QUrl(QStringLiteral("https://ghfast.top/https://github.com/XTLS/Xray-core/releases/download/v26.3.27/hello.txt.sha256"))};
    request.metadataFromOfficial = true;

    const VerificationOutcome outcome =
        ReleaseChecksum::verifyDownloadedBytes(request, fetch);
    QCOMPARE(outcome.status, Verification::Unverifiable);
}

void ReleaseChecksumTests::verifyFallsBackToADerivedChecksumFile()
{
    // No digest, no checksum asset in the release: the .dgst beside the package still verifies
    // it. This is the Xray fallback path, where the release object is built locally.
    const auto fetch = [](const QUrl& url, QByteArray* content) {
        if (url.toString().endsWith(QStringLiteral("dummy-package.dgst"))) {
            *content = (QStringLiteral("SHA2-256(dummy-package)= ") + kDummyPackageSha256 + QStringLiteral("\n")).toUtf8();
            return OperationResult::ok();
        }
        return OperationResult::fail(QStringLiteral("not found"));
    };

    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("dummy-package");
    request.assetName = QStringLiteral("dummy-package");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("dummy-package"));
    request.metadataFromOfficial = true;

    const VerificationOutcome verified =
        ReleaseChecksum::verifyDownloadedBytes(request, fetch);
    QCOMPARE(verified.status, Verification::Verified);
}

void ReleaseChecksumTests::verifyDerivedChecksumNeedsOfficialMetadataOrACodeUrl()
{
    const auto fetch = [](const QUrl& url, QByteArray* content) {
        if (url.toString().endsWith(QStringLiteral("dummy-package.dgst"))) {
            *content = (QStringLiteral("SHA2-256(dummy-package)= ") + kDummyPackageSha256 + QStringLiteral("\n")).toUtf8();
            return OperationResult::ok();
        }
        return OperationResult::fail(QStringLiteral("not found"));
    };

    // The bootstrap case: no metadata at all, but the package URL comes from the backend's
    // code, so the derived checksum URLs are anchored to that URL's own (github.com) host.
    ReleaseChecksum::VerificationRequest fromCode;
    fromCode.packageBytes = QByteArray("dummy-package");
    fromCode.assetName = QStringLiteral("dummy-package");
    fromCode.packageDownloadUrl = releaseUrl(QStringLiteral("dummy-package"));
    fromCode.packageUrlFromCode = true;

    const VerificationOutcome verified =
        ReleaseChecksum::verifyDownloadedBytes(fromCode, fetch);
    QCOMPARE(verified.status, Verification::Verified);

    // The same package URL, but named by mirror-supplied metadata: the derived document
    // is not an official anchor without official metadata behind it.
    ReleaseChecksum::VerificationRequest fromMirror = fromCode;
    fromMirror.packageUrlFromCode = false;
    const VerificationOutcome unverifiable =
        ReleaseChecksum::verifyDownloadedBytes(fromMirror, fetch);
    QCOMPARE(unverifiable.status, Verification::Unverifiable);
}

void ReleaseChecksumTests::verifyIsUnverifiableWhenNoChecksumExists()
{
    int fetchCalls = 0;
    const auto fetch = [&fetchCalls](const QUrl&, QByteArray*) {
        ++fetchCalls;
        return OperationResult::fail(QStringLiteral("not found"));
    };

    // No digest, no checksum asset, and every derived URL 404s: unverifiable, not verified.
    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("dummy-package");
    request.assetName = QStringLiteral("dummy-package");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("dummy-package"));
    request.metadataFromOfficial = true;

    const VerificationOutcome outcome =
        ReleaseChecksum::verifyDownloadedBytes(request, fetch);
    QCOMPARE(outcome.status, Verification::Unverifiable);
    QVERIFY(outcome.expectedHex.isEmpty());
    QVERIFY(outcome.actualHex.isEmpty());
    // The checksum asset slot is empty, so all three derived URLs are tried.
    QCOMPARE(fetchCalls, 3);
}

void ReleaseChecksumTests::verifyViaMirrorAcceptsAMirrorSuppliedDigest()
{
    const auto fetch = [](const QUrl&, QByteArray*) {
        return OperationResult::fail(QStringLiteral("must not be called"));
    };

    // GitHub unreachable, metadata through a mirror: the digest is mirror-supplied, so
    // the degraded pass accepts it as MirrorVerified -- self-consistent with the mirror,
    // not anchored to GitHub -- and never fetches.
    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("hello");
    request.assetName = QStringLiteral("hello.txt");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("hello.txt"));
    request.apiDigest = QStringLiteral("sha256:") + kHelloSha256;
    request.metadataFromOfficial = false;

    const VerificationOutcome outcome =
        ReleaseChecksum::verifyDownloadedBytesViaMirror(request, fetch);
    QCOMPARE(outcome.status, Verification::MirrorVerified);
    QCOMPARE(outcome.expectedHex, kHelloSha256);
    QCOMPARE(outcome.actualHex, kHelloSha256);
}

void ReleaseChecksumTests::verifyViaMirrorAcceptsAMirrorHostedChecksumDocument()
{
    const auto fetch = [](const QUrl& url, QByteArray* content) {
        // The mirror pass expands each checksum URL through the mirror candidates, so the
        // document is fetched from a mirror host here, not from github.com.
        if (url.host() == QStringLiteral("ghfast.top")
            && url.toString().endsWith(QStringLiteral("hello.txt.sha256"))) {
            *content = (kHelloSha256 + QStringLiteral("  hello.txt\n")).toUtf8();
            return OperationResult::ok();
        }
        return OperationResult::fail(QStringLiteral("not found"));
    };

    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("hello");
    request.assetName = QStringLiteral("hello.txt");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("hello.txt"));
    request.checksumAsset =
        NamedAsset{QStringLiteral("hello.txt.sha256"), releaseUrl(QStringLiteral("hello.txt.sha256"))};
    request.metadataFromOfficial = false;

    const VerificationOutcome outcome =
        ReleaseChecksum::verifyDownloadedBytesViaMirror(request, fetch);
    QCOMPARE(outcome.status, Verification::MirrorVerified);
}

void ReleaseChecksumTests::verifyViaMirrorRefusesAMirrorMismatch()
{
    const auto fetch = [](const QUrl&, QByteArray*) {
        return OperationResult::fail(QStringLiteral("must not be called"));
    };

    // A mirror-supplied digest that does not match the bytes is a mismatch in the
    // degraded pass too: the degraded path only ever relaxes where the anchor comes
    // from, never whether the bytes match it.
    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("hello");
    request.assetName = QStringLiteral("hello.txt");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("hello.txt"));
    request.apiDigest = QStringLiteral("sha256:") + kDummyPackageSha256;
    request.metadataFromOfficial = false;

    const VerificationOutcome outcome =
        ReleaseChecksum::verifyDownloadedBytesViaMirror(request, fetch);
    QCOMPARE(outcome.status, Verification::Mismatched);
    QCOMPARE(outcome.expectedHex, kDummyPackageSha256);
    QCOMPARE(outcome.actualHex, kHelloSha256);
}

void ReleaseChecksumTests::verifyViaMirrorIsUnverifiableWhenNoChecksumExists()
{
    int fetchCalls = 0;
    const auto fetch = [&fetchCalls](const QUrl&, QByteArray*) {
        ++fetchCalls;
        return OperationResult::fail(QStringLiteral("not found"));
    };

    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = QByteArray("dummy-package");
    request.assetName = QStringLiteral("dummy-package");
    request.packageDownloadUrl = releaseUrl(QStringLiteral("dummy-package"));
    request.metadataFromOfficial = false;

    const VerificationOutcome outcome =
        ReleaseChecksum::verifyDownloadedBytesViaMirror(request, fetch);
    QCOMPARE(outcome.status, Verification::Unverifiable);
    QVERIFY(outcome.expectedHex.isEmpty());
    // No checksum asset: three derived URLs, each tried through three mirrors plus the
    // official URL.
    QCOMPARE(fetchCalls, 12);
}

QTEST_MAIN(ReleaseChecksumTests)

#include "ReleaseChecksumTests.moc"
