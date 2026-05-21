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

QTEST_MAIN(GeoResourceUpdateServiceTests)

#include "GeoResourceUpdateServiceTests.moc"
