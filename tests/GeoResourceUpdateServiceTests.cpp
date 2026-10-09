#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>

#include "services/GeoResourceUpdateService.h"

class GeoResourceUpdateServiceTests : public QObject {
    Q_OBJECT

private slots:
    void updateReportsProgressAndWritesGeoFile();
    void updateSingBoxRuleSetWritesRuleSetFile();
    void updateSingBoxRuleSetRejectsUnsupportedTag();
    void updateCoreGeoFileSavesUnderTheNameTheCoreReads();
    void updateCoreGeoFileRejectsAnIncompleteRequirement();
};

void GeoResourceUpdateServiceTests::updateReportsProgressAndWritesGeoFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QStringList progressMessages;
    GeoResourceUpdateService service(
        directory.path(),
        [](const QUrl&, QByteArray* content) {
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
    // A mirror prefix may be prepended to the URL, so only the repository, release and asset name
    // are asserted -- the point is which file is fetched, not which host serves it.
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

QTEST_MAIN(GeoResourceUpdateServiceTests)

#include "GeoResourceUpdateServiceTests.moc"
