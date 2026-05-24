#include <QtTest>

#include <QDir>
#include <QTemporaryDir>
#include <QFile>

#include "app/CoreStartupCheckpoint.h"
#include "app/TunRuntimeState.h"
#include "runtime/CoreConfigPreflight.h"

class AppBootstrapTunRuntimeTests : public QObject {
    Q_OBJECT

private slots:
    void coreStartupChecklistMarksUseEmoji();
    void coreStartupChecklistItemOmitsDetailText();
    void cleanupRequiredWhenCoreStartedWithTun();
    void cleanupSkippedWhenCoreStartedWithoutTun();
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
};

namespace {

QString emojiMark(ushort codePoint)
{
    return QString(QChar(codePoint));
}

} // namespace

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
    QVERIFY(result.message.contains(QStringLiteral("does not require local geoip.dat/geosite.dat")));
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
    QVERIFY(result.message.contains(QStringLiteral("Found geoip.dat and geosite.dat")));
}

QTEST_MAIN(AppBootstrapTunRuntimeTests)

#include "AppBootstrapTunRuntimeTests.moc"

