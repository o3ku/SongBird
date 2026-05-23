#include <QtTest>

#include "services/AppUpdateService.h"

class AppUpdateServiceTests : public QObject {
    Q_OBJECT

private slots:
    void checkFindsNewerStableRelease();
    void checkSkipsPrereleaseWhenDisabled();
    void checkAllowsPrereleaseWhenEnabled();
    void checkReportsUpToDate();
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
    const OperationResult op = service.checkForUpdate(QStringLiteral("1.1.0"), false, &result);

    QVERIFY(op.success);
    QVERIFY(result.updateAvailable);
    QCOMPARE(result.currentVersion, QStringLiteral("1.1.0"));
    QCOMPARE(result.latestVersion, QStringLiteral("v1.2.0"));
    QCOMPARE(result.releaseName, QStringLiteral("SongBird 1.2.0"));
    QCOMPARE(result.releaseUrl.toString(), QStringLiteral("https://github.com/o3ku/songbird/releases/tag/v1.2.0"));
    QVERIFY(requestedUrl.contains(QStringLiteral("api.github.com/repos/o3ku/songbird/releases")));
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
    const OperationResult op = service.checkForUpdate(QStringLiteral("1.8.0"), false, &result);

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
    const OperationResult op = service.checkForUpdate(QStringLiteral("1.8.0"), true, &result);

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
    const OperationResult op = service.checkForUpdate(QStringLiteral("1.2.0"), false, &result);

    QVERIFY(op.success);
    QVERIFY(!result.updateAvailable);
    QCOMPARE(result.latestVersion, QStringLiteral("v1.2.0"));
    QVERIFY(op.message.contains(QStringLiteral("up to date")));
}

QTEST_MAIN(AppUpdateServiceTests)

#include "AppUpdateServiceTests.moc"
