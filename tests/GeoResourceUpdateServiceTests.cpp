#include <QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>

#include "services/GeoResourceUpdateService.h"

// A minimal releases-API payload naming one asset with the SHA-256 of the bytes the fake
// download handler serves for it, so the verification the service now does has something
// true to check against.
QByteArray releaseMetadataJson(const QString& assetName, const QString& digestHex)
{
    QJsonObject asset;
    asset.insert(QStringLiteral("name"), assetName);
    asset.insert(
        QStringLiteral("browser_download_url"),
        QStringLiteral("https://github.com/example.invalid/") + assetName);
    asset.insert(QStringLiteral("digest"), QStringLiteral("sha256:") + digestHex);
    QJsonObject release;
    release.insert(QStringLiteral("tag_name"), QStringLiteral("v1.0.0"));
    release.insert(QStringLiteral("prerelease"), false);
    release.insert(QStringLiteral("draft"), false);
    release.insert(QStringLiteral("assets"), QJsonArray{asset});
    return QJsonDocument(QJsonArray{release}).toJson(QJsonDocument::Compact);
}

class GeoResourceUpdateServiceTests : public QObject {
    Q_OBJECT

private slots:
    void updateReportsProgressAndWritesGeoFile();
    void updateSingBoxRuleSetWritesRuleSetFile();
    void updateSingBoxRuleSetRejectsUnsupportedTag();
    void updateCoreGeoFileSavesUnderTheNameTheCoreReads();
    void updateCoreGeoFileRejectsAnIncompleteRequirement();
    void updateRefusesGeoFileWhenReleaseMetadataIsUnavailable();
    void updateRefusesGeoFileOnChecksumMismatch();
    void updateReportsWarningWhenDigestComesFromAMirror();
};

void GeoResourceUpdateServiceTests::updateReportsProgressAndWritesGeoFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QStringList progressMessages;
    GeoResourceUpdateService service(
        directory.path(),
        [](const QUrl& url, QByteArray* content) {
            // The service fetches the release metadata first to learn the expected checksum,
            // then the file itself; the handler has to answer both.
            if (url.toString().contains(QStringLiteral("api.github.com"))) {
                *content = releaseMetadataJson(
                    QStringLiteral("geoip.dat"),
                    // SHA-256 of "geo-payload".
                    QStringLiteral("a5e18841150b7f40d88caa3dbe6e32eb7c8ad4638b6f6fbfdf01c41c81f9f70e"));
                return OperationResult::ok();
            }
            *content = QByteArray("geo-payload");
            return OperationResult::ok();
        },
        [&progressMessages](const QString& message) {
            progressMessages.append(message);
        });

    const OperationResult result = service.update(QStringLiteral("geoip"));

    QVERIFY(result.success);
    QVERIFY(QFile::exists(directory.filePath(QStringLiteral("geoip.dat"))));

    QFile file(directory.filePath(QStringLiteral("geoip.dat")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("geo-payload"));

    QVERIFY(progressMessages.contains(QStringLiteral("Downloading geoip.dat...")));
    QVERIFY(std::any_of(
        progressMessages.cbegin(),
        progressMessages.cend(),
        [](const QString& message) {
            return message.contains(QStringLiteral("Trying download source:"));
        }));
    QVERIFY(std::any_of(
        progressMessages.cbegin(),
        progressMessages.cend(),
        [](const QString& message) {
            return message.contains(QStringLiteral("Downloaded geoip.dat"));
        }));
    QVERIFY(std::any_of(
        progressMessages.cbegin(),
        progressMessages.cend(),
        [](const QString& message) {
            return message.contains(QStringLiteral("Saving "));
        }));
}

void GeoResourceUpdateServiceTests::updateSingBoxRuleSetWritesRuleSetFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString requestedUrl;
    GeoResourceUpdateService service(
        directory.path(),
        [&requestedUrl](const QUrl& url, QByteArray* content) {
            requestedUrl = url.toString();
            if (url.toString().contains(QStringLiteral("api.github.com"))) {
                *content = releaseMetadataJson(
                    QStringLiteral("geosite-google.srs"),
                    // SHA-256 of "rule-set-payload".
                    QStringLiteral("bd76240a216c52f7643892302821f7c4d4c8e1d82f78f6ef14cda232614668bc"));
                return OperationResult::ok();
            }
            *content = QByteArray("rule-set-payload");
            return OperationResult::ok();
        });

    const OperationResult result = service.updateSingBoxRuleSet(QStringLiteral("geosite-google"));

    QVERIFY(result.success);
    QVERIFY(requestedUrl.contains(QStringLiteral("SagerNet/sing-geosite/rule-set/geosite-google.srs")));

    const QString ruleSetPath = QDir(directory.path()).filePath(QStringLiteral("rule-set/geosite-google.srs"));
    QVERIFY(QFile::exists(ruleSetPath));
    QFile file(ruleSetPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("rule-set-payload"));
}

void GeoResourceUpdateServiceTests::updateSingBoxRuleSetRejectsUnsupportedTag()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    GeoResourceUpdateService service(directory.path());

    const OperationResult result = service.updateSingBoxRuleSet(QStringLiteral("category-ads-all"));

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("Unsupported sing-box rule-set")));
}

void GeoResourceUpdateServiceTests::updateCoreGeoFileSavesUnderTheNameTheCoreReads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString requestedUrl;
    QStringList progressMessages;
    GeoResourceUpdateService service(
        directory.path(),
        [&requestedUrl](const QUrl& url, QByteArray* content) {
            requestedUrl = url.toString();
            if (url.toString().contains(QStringLiteral("api.github.com"))) {
                *content = releaseMetadataJson(
                    QStringLiteral("geosite.dat"),
                    // SHA-256 of "geosite-payload".
                    QStringLiteral("c9af58977a8e31e67943c82547ca16c9d22458facc1b5700350574daa138a5ac"));
                return OperationResult::ok();
            }
            *content = QByteArray("geosite-payload");
            return OperationResult::ok();
        },
        [&progressMessages](const QString& message) {
            progressMessages.append(message);
        });

    // mihomo publishes "geosite.dat" but reads "GeoSite.dat", so the two names have to be kept
    // apart: fetching the published name and saving it under the same name would leave the core
    // without the database it asked for.
    const CoreGeoFileRequirement requirement{
        QStringLiteral("GeoSite.dat"),
        QStringLiteral("geosite.dat"),
        QStringLiteral("MetaCubeX/meta-rules-dat")};

    const OperationResult result = service.updateCoreGeoFile(requirement);

    QVERIFY2(result.success, qPrintable(result.message));
    // Only the repository, release and asset name are asserted -- the point is which file is
    // fetched, not the exact host or query string.
    QVERIFY2(
        requestedUrl.endsWith(
            QStringLiteral("MetaCubeX/meta-rules-dat/releases/download/latest/geosite.dat")),
        qPrintable(requestedUrl));

    // The save path is asserted through the progress text rather than through the filesystem:
    // Windows paths are case-insensitive, so QFile::exists("geosite.dat") would find
    // "GeoSite.dat" and the check would pass even when the file was written under the wrong name.
    //
    // It has to be the *saving* line, not any line: "Downloading %1..." is already built from the
    // name the core reads, so a plain "some message mentions GeoSite.dat" holds even when the file
    // is written under the published name -- a check that could not fail.
    const auto savingMessage = std::find_if(
        progressMessages.cbegin(),
        progressMessages.cend(),
        [](const QString& message) {
            return message.contains(QStringLiteral("Saving "));
        });
    QVERIFY2(savingMessage != progressMessages.cend(), qPrintable(progressMessages.join(QChar('|'))));
    QVERIFY2(savingMessage->contains(QStringLiteral("GeoSite.dat")), qPrintable(*savingMessage));

    const QString savedPath = QDir(directory.path()).filePath(QStringLiteral("GeoSite.dat"));
    QVERIFY(QFile::exists(savedPath));
    QFile file(savedPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("geosite-payload"));
}

void GeoResourceUpdateServiceTests::updateCoreGeoFileRejectsAnIncompleteRequirement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    bool downloadAttempted = false;
    GeoResourceUpdateService service(
        directory.path(),
        [&downloadAttempted](const QUrl&, QByteArray* content) {
            downloadAttempted = true;
            *content = QByteArray("payload");
            return OperationResult::ok();
        });

    const OperationResult result = service.updateCoreGeoFile(
        CoreGeoFileRequirement{QStringLiteral("GeoSite.dat"), {}, {}});

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("No download source for geo file GeoSite.dat")));
    QVERIFY(!downloadAttempted);
    QVERIFY(!QFile::exists(QDir(directory.path()).filePath(QStringLiteral("GeoSite.dat"))));
}

void GeoResourceUpdateServiceTests::updateRefusesGeoFileWhenReleaseMetadataIsUnavailable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    GeoResourceUpdateService service(
        directory.path(),
        [](const QUrl& url, QByteArray* content) {
            if (url.toString().contains(QStringLiteral("api.github.com"))) {
                return OperationResult::fail(QStringLiteral("GitHub API unavailable"));
            }
            *content = QByteArray("geo-payload");
            return OperationResult::ok();
        });

    const OperationResult result = service.update(QStringLiteral("geoip"));

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("cannot be verified")));
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("geoip.dat"))));
}

void GeoResourceUpdateServiceTests::updateRefusesGeoFileOnChecksumMismatch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    GeoResourceUpdateService service(
        directory.path(),
        [](const QUrl& url, QByteArray* content) {
            if (url.toString().contains(QStringLiteral("api.github.com"))) {
                // The digest of "something-else", not of the "geo-payload" the file URL serves.
                *content = releaseMetadataJson(
                    QStringLiteral("geoip.dat"),
                    QStringLiteral("3160fbd511a071c87828ece647a54c6aa5a1e1563b7a89d89875af7a63ad327a"));
                return OperationResult::ok();
            }
            *content = QByteArray("geo-payload");
            return OperationResult::ok();
        });

    const OperationResult result = service.update(QStringLiteral("geoip"));

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("checksum mismatch")));
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("geoip.dat"))));
}

void GeoResourceUpdateServiceTests::updateReportsWarningWhenDigestComesFromAMirror()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QStringList progressMessages;
    GeoResourceUpdateService service(
        directory.path(),
        [](const QUrl& url, QByteArray* content) {
            // The official channel is unreachable: the metadata arrives through a mirror,
            // so the digest it carries is mirror-supplied, not GitHub-anchored.
            if (url.host() == QStringLiteral("api.github.com")) {
                return OperationResult::fail(QStringLiteral("network unreachable"));
            }
            if (url.toString().contains(QStringLiteral("api.github.com"))) {
                *content = releaseMetadataJson(
                    QStringLiteral("geoip.dat"),
                    // SHA-256 of "geo-payload".
                    QStringLiteral("a5e18841150b7f40d88caa3dbe6e32eb7c8ad4638b6f6fbfdf01c41c81f9f70e"));
                return OperationResult::ok();
            }
            *content = QByteArray("geo-payload");
            return OperationResult::ok();
        },
        [&progressMessages](const QString& message) {
            progressMessages.append(message);
        });

    const OperationResult result = service.update(QStringLiteral("geoip"));

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(QFile::exists(directory.filePath(QStringLiteral("geoip.dat"))));
    // The file is saved -- mirrors exist so this works when GitHub is unreachable -- but
    // the warning is loud in both places the user can see it: the log panel and the
    // result message.
    QVERIFY(std::any_of(
        progressMessages.cbegin(),
        progressMessages.cend(),
        [](const QString& message) {
            return message.contains(QStringLiteral("GitHub was unreachable"));
        }));
    QVERIFY(result.message.contains(QStringLiteral("GitHub was unreachable")));
    QVERIFY(result.message.contains(QStringLiteral("mirror")));
}

QTEST_MAIN(GeoResourceUpdateServiceTests)

#include "GeoResourceUpdateServiceTests.moc"
