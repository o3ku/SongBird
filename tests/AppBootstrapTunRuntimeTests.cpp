#include <QtTest>

#include <QDir>
#include <QTemporaryDir>
#include <QFile>

#include "appcore/CoreStartupCheckpoint.h"
#include "appcore/TunRuntimeState.h"
#include "runtime/CoreConfigPreflight.h"
#include "runtime/TunAdapterNames.h"
#include "TestSupport.h"

class AppBootstrapTunRuntimeTests : public QObject {
    Q_OBJECT

private slots:
    void coreStartupChecklistMarksUseEmoji();
    void coreStartupChecklistItemOmitsDetailText();
    void cleanupRequiredWhenCoreStartedWithTun();
    void cleanupSkippedWhenCoreStartedWithoutTun();
    void tunAdapterCleanupNamesCoverCurrentAndLegacyNames();
    void startAfterTunCleanupRequiresSuccessfulCleanup();
    void postStopActionAfterTunCleanupRequiresSuccessfulCleanup();
    void tunAdapterConflictOutputIsDetected();
    void tunAdapterConflictRetryRequiresTunAndRemainingAttempts();
    void singBoxPreflightUsesCheckCommand();
    void xrayPreflightUsesRunTestCommand();
    void v2rayPreflightUsesRunTestCommand();
    void unsupportedPreflightCommandIsEmpty();
    void preflightFailsForMissingProgram();
    void singBoxGeoFileCheckIsSkipped();
    void xrayGeoFileCheckFailsWhenDatFilesMissing();
    void xrayGeoFileCheckPassesWhenDatFilesExist();
    void xrayGeoDirectoryIsTheCoreDirectory();
    void mihomoGeoFileCheckFailsWhenDataFilesMissing();
    void mihomoGeoFileCheckPassesWhenDataFilesExist();
    void mihomoGeoFileCheckTreatsAnEmptyDatabaseAsMissing();
    void mihomoGeoDirectoryIsItsHomeDirectory();
    void unknownCoreGeoCheckFallsBackToTheExecutableName();
};

using TestSupport::emojiMark;

void AppBootstrapTunRuntimeTests::coreStartupChecklistMarksUseEmoji()
{
    QCOMPARE(coreStartupChecklistMark(CoreStartupCheckpointStatus::Pending), emojiMark(0x26AA));
    QCOMPARE(coreStartupChecklistMark(CoreStartupCheckpointStatus::Started), emojiMark(0x23F3));
    QCOMPARE(coreStartupChecklistMark(CoreStartupCheckpointStatus::Passed), emojiMark(0x2705));
    QCOMPARE(coreStartupChecklistMark(CoreStartupCheckpointStatus::Skipped), emojiMark(0x2796));
    QCOMPARE(coreStartupChecklistMark(CoreStartupCheckpointStatus::Failed), emojiMark(0x274C));
}

void AppBootstrapTunRuntimeTests::coreStartupChecklistItemOmitsDetailText()
{
    QCOMPARE(
        coreStartupChecklistItem(CoreStartupCheckpointStatus::Started, QStringLiteral("Validate core application")),
        QStringLiteral("%1 Validate core application").arg(emojiMark(0x23F3)));

    const OperationResult checkpoint = coreStartupCheckpoint(
        CoreStartupCheckpointStatus::Started,
        QStringLiteral("Validate core application"),
        QStringLiteral("Trying download source: https://example.com/core.zip"));

    QVERIFY(checkpoint.success);
    QVERIFY(checkpoint.message.contains(QStringLiteral("https://example.com/core.zip")));
}

void AppBootstrapTunRuntimeTests::cleanupRequiredWhenCoreStartedWithTun()
{
    QVERIFY(shouldCleanupTunAfterCoreStop(true, true));
    QVERIFY(!shouldCleanupTunAfterCoreStop(false, true));
}

void AppBootstrapTunRuntimeTests::cleanupSkippedWhenCoreStartedWithoutTun()
{
    QVERIFY(!shouldCleanupTunAfterCoreStop(true, false));
    QVERIFY(!shouldCleanupTunAfterCoreStop(false, false));
}

void AppBootstrapTunRuntimeTests::tunAdapterCleanupNamesCoverCurrentAndLegacyNames()
{
    const QStringList cleanupNames = tunAdapterCleanupNames();

    QCOMPARE(songbirdTunAdapterName(), QStringLiteral("songbird_tun"));
    QCOMPARE(legacySingBoxTunAdapterName(), QStringLiteral("singbox_tun"));
    QVERIFY(cleanupNames.contains(songbirdTunAdapterName()));
    QVERIFY(cleanupNames.contains(legacySingBoxTunAdapterName()));
}

void AppBootstrapTunRuntimeTests::startAfterTunCleanupRequiresSuccessfulCleanup()
{
    QVERIFY(shouldResumeCoreStartAfterTunCleanup(true, true, false, false));
    QVERIFY(!shouldResumeCoreStartAfterTunCleanup(false, true, false, false));
    QVERIFY(!shouldResumeCoreStartAfterTunCleanup(true, true, true, false));
    QVERIFY(!shouldResumeCoreStartAfterTunCleanup(true, true, false, true));
}

void AppBootstrapTunRuntimeTests::postStopActionAfterTunCleanupRequiresSuccessfulCleanup()
{
    QVERIFY(shouldRunPostStopActionAfterTunCleanup(true, true, false));
    QVERIFY(!shouldRunPostStopActionAfterTunCleanup(false, true, false));
    QVERIFY(!shouldRunPostStopActionAfterTunCleanup(true, false, false));
    QVERIFY(!shouldRunPostStopActionAfterTunCleanup(true, true, true));
}

void AppBootstrapTunRuntimeTests::tunAdapterConflictOutputIsDetected()
{
    QVERIFY(isTunAdapterConflictOutput(QStringLiteral(
        "FATAL[0015] start service: start inbound/tun[tun-in]: configure tun interface: Cannot create a file when that file already exists.")));
    QVERIFY(!isTunAdapterConflictOutput(QStringLiteral(
        "Core exit detected (code=1). Restarting in 3s...")));
    QVERIFY(!isTunAdapterConflictOutput(QStringLiteral(
        "configure tun interface: permission denied")));
}

void AppBootstrapTunRuntimeTests::tunAdapterConflictRetryRequiresTunAndRemainingAttempts()
{
    QVERIFY(shouldRetryAfterTunAdapterConflict(true, true, true, 0, 1));
    QVERIFY(!shouldRetryAfterTunAdapterConflict(false, true, true, 0, 1));
    QVERIFY(!shouldRetryAfterTunAdapterConflict(true, false, true, 0, 1));
    QVERIFY(!shouldRetryAfterTunAdapterConflict(true, true, false, 0, 1));
    QVERIFY(!shouldRetryAfterTunAdapterConflict(true, true, true, 1, 1));
}

void AppBootstrapTunRuntimeTests::singBoxPreflightUsesCheckCommand()
{
    CoreInfo info;
    info.program = QStringLiteral("C:/cores/sing-box.exe");

    const QStringList arguments = buildCoreConfigPreflightArguments(
        info,
        QStringLiteral("C:/configs/songbird.json"));

    QCOMPARE(arguments, QStringList({
        QStringLiteral("check"),
        QStringLiteral("-c"),
        QDir::toNativeSeparators(QStringLiteral("C:/configs/songbird.json"))
    }));
}

void AppBootstrapTunRuntimeTests::xrayPreflightUsesRunTestCommand()
{
    CoreInfo info;
    info.program = QStringLiteral("C:/cores/xray.exe");

    const QStringList arguments = buildCoreConfigPreflightArguments(
        info,
        QStringLiteral("C:/configs/songbird.json"));

    QCOMPARE(arguments, QStringList({
        QStringLiteral("run"),
        QStringLiteral("-test"),
        QStringLiteral("-config"),
        QDir::toNativeSeparators(QStringLiteral("C:/configs/songbird.json"))
    }));
}

void AppBootstrapTunRuntimeTests::v2rayPreflightUsesRunTestCommand()
{
    CoreInfo info;
    info.program = QStringLiteral("C:/cores/v2ray.exe");

    const QStringList arguments = buildCoreConfigPreflightArguments(
        info,
        QStringLiteral("C:/configs/songbird.json"));

    QCOMPARE(arguments, QStringList({
        QStringLiteral("-test"),
        QStringLiteral("-config"),
        QDir::toNativeSeparators(QStringLiteral("C:/configs/songbird.json"))
    }));
}

void AppBootstrapTunRuntimeTests::unsupportedPreflightCommandIsEmpty()
{
    CoreInfo info;
    info.program = QStringLiteral("C:/cores/custom-core.exe");

    QVERIFY(buildCoreConfigPreflightArguments(
        info,
        QStringLiteral("C:/configs/songbird.json")).isEmpty());
}

void AppBootstrapTunRuntimeTests::preflightFailsForMissingProgram()
{
    CoreInfo info;
    info.program = QDir::temp().filePath(QStringLiteral("missing-SongBird-core.exe"));

    const OperationResult result = validateCoreConfigBeforeStart(
        info,
        QDir::temp().filePath(QStringLiteral("missing-config.json")),
        10);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("core executable was not found")));
}

void AppBootstrapTunRuntimeTests::singBoxGeoFileCheckIsSkipped()
{
    CoreInfo info;
    info.program = QStringLiteral("C:/cores/sing-box.exe");

    const OperationResult result = validateCoreGeoFilesBeforeStart(info);

    QVERIFY(result.success);
    QVERIFY(result.message.contains(QStringLiteral("does not require local geo database files")));
}

void AppBootstrapTunRuntimeTests::xrayGeoFileCheckFailsWhenDatFilesMissing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CoreInfo info;
    info.program = directory.filePath(QStringLiteral("xray.exe"));
    info.workingDirectory = directory.path();

    const OperationResult result = validateCoreGeoFilesBeforeStart(info);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("missing geoip.dat, geosite.dat")));
}

void AppBootstrapTunRuntimeTests::xrayGeoFileCheckPassesWhenDatFilesExist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    for (const QString& fileName : {QStringLiteral("geoip.dat"), QStringLiteral("geosite.dat")}) {
        QFile file(directory.filePath(fileName));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("ok") > 0);
    }

    CoreInfo info;
    info.program = directory.filePath(QStringLiteral("xray.exe"));
    info.workingDirectory = directory.path();

    const OperationResult result = validateCoreGeoFilesBeforeStart(info);

    QVERIFY(result.success);
    QVERIFY(result.message.contains(QStringLiteral("Found geoip.dat, geosite.dat")));
}

void AppBootstrapTunRuntimeTests::xrayGeoDirectoryIsTheCoreDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CoreInfo info;
    info.type = CoreType::Xray;
    info.program = directory.filePath(QStringLiteral("xray.exe"));
    info.workingDirectory = directory.path();

    // Xray declares no data directory, so its geodata sits wherever the core runs from -- not in a
    // sub-directory of the application directory the way mihomo's does.
    QCOMPARE(coreGeoDirectory(info), directory.path());
}

void AppBootstrapTunRuntimeTests::mihomoGeoFileCheckFailsWhenDataFilesMissing()
{
    QTemporaryDir applicationDirectory;
    QVERIFY(applicationDirectory.isValid());

    CoreInfo info;
    info.type = CoreType::Mihomo;
    info.program = applicationDirectory.filePath(QStringLiteral("mihomo.exe"));

    const OperationResult result =
        validateCoreGeoFilesBeforeStart(info, applicationDirectory.path());

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("missing GeoSite.dat, geoip.metadb")));
    QVERIFY(result.message.contains(
        QDir::toNativeSeparators(applicationDirectory.filePath(QStringLiteral("mihomo")))));
}

void AppBootstrapTunRuntimeTests::mihomoGeoFileCheckPassesWhenDataFilesExist()
{
    QTemporaryDir applicationDirectory;
    QVERIFY(applicationDirectory.isValid());
    QVERIFY(QDir().mkpath(applicationDirectory.filePath(QStringLiteral("mihomo"))));
    for (const QString& fileName : {QStringLiteral("GeoSite.dat"), QStringLiteral("geoip.metadb")}) {
        QFile file(applicationDirectory.filePath(QStringLiteral("mihomo/%1").arg(fileName)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("ok") > 0);
    }

    CoreInfo info;
    info.type = CoreType::Mihomo;
    info.program = applicationDirectory.filePath(QStringLiteral("mihomo.exe"));

    const OperationResult result =
        validateCoreGeoFilesBeforeStart(info, applicationDirectory.path());

    QVERIFY2(result.success, qPrintable(result.message));
    QVERIFY(result.message.contains(QStringLiteral("Found GeoSite.dat, geoip.metadb")));
}

void AppBootstrapTunRuntimeTests::mihomoGeoFileCheckTreatsAnEmptyDatabaseAsMissing()
{
    QTemporaryDir applicationDirectory;
    QVERIFY(applicationDirectory.isValid());
    QVERIFY(QDir().mkpath(applicationDirectory.filePath(QStringLiteral("mihomo"))));

    // A download interrupted mid-write leaves a zero-byte file. Reading it as "present" would send
    // mihomo a database it cannot parse instead of refetching it.
    QFile truncated(applicationDirectory.filePath(QStringLiteral("mihomo/GeoSite.dat")));
    QVERIFY(truncated.open(QIODevice::WriteOnly));
    truncated.close();

    CoreInfo info;
    info.type = CoreType::Mihomo;
    info.program = applicationDirectory.filePath(QStringLiteral("mihomo.exe"));

    const OperationResult result =
        validateCoreGeoFilesBeforeStart(info, applicationDirectory.path());

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("empty GeoSite.dat")));
    QVERIFY(!coreGeoFileIsPresent(
        applicationDirectory.filePath(QStringLiteral("mihomo")), QStringLiteral("GeoSite.dat")));
}

void AppBootstrapTunRuntimeTests::mihomoGeoDirectoryIsItsHomeDirectory()
{
    QTemporaryDir applicationDirectory;
    QVERIFY(applicationDirectory.isValid());

    CoreInfo info;
    info.type = CoreType::Mihomo;
    info.program = applicationDirectory.filePath(QStringLiteral("mihomo.exe"));
    // Deliberately set: mihomo's geodata must not land in the directory the core runs from, because
    // the core is told to read its home through -d and would never look here.
    info.workingDirectory = applicationDirectory.path();

    QCOMPARE(
        coreGeoDirectory(info, applicationDirectory.path()),
        applicationDirectory.filePath(QStringLiteral("mihomo")));
}

void AppBootstrapTunRuntimeTests::unknownCoreGeoCheckFallsBackToTheExecutableName()
{
    // A CoreInfo built by hand leaves the type unset, and the TUN sidecar is exactly such a value.
    // Reading the type alone would report "no geodata needed" for an Xray sidecar.
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    CoreInfo info;
    info.program = directory.filePath(QStringLiteral("xray.exe"));
    info.workingDirectory = directory.path();

    QVERIFY(coreNeedsGeoFiles(info));
    QCOMPARE(coreInfoCoreType(info), CoreType::Xray);

    const OperationResult result = validateCoreGeoFilesBeforeStart(info);
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("missing geoip.dat, geosite.dat")));
}

QTEST_MAIN(AppBootstrapTunRuntimeTests)

#include "AppBootstrapTunRuntimeTests.moc"

