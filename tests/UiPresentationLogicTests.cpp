#include <QtTest>

#include <QColor>
#include <QBrush>

#include "ui/dialogs/CoreSettingsPageSupport.h"
#include "ui/models/ServerTableResultStyle.h"
#include "ui/theme/AppTheme.h"

// Covers the two pure decision helpers behind the UI features that have no
// widget of their own: the speed-result colour band, and the theme preference
// resolution. Both are I/O free, so they are tested here rather than through a
// dialog.
class UiPresentationLogicTests : public QObject {
    Q_OBJECT

private slots:
    void resultStyleClassifiesLatencyBands();
    void resultStyleTreatsUnreadableResultsAsFailures();
    void themeResolutionFoldsUnknownNamesToLight();
    void themeResolutionHonoursSystemPreference();
    void themeResolutionFallsBackWhenPlatformReportsNothing();
    void systemThemeNameIsRecognisedCaseInsensitively();
    void coreStatusOffersCancelOnlyWhileUpdating();
    void coreStatusTurnsTheButtonIntoARetryAfterAFailure();
    void coreStatusTooltipCarriesTheByteCounts();
};

void UiPresentationLogicTests::resultStyleClassifiesLatencyBands()
{
    using ServerTableResultStyle::Severity;

    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("0 ms")), Severity::Fast);
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("299 ms")), Severity::Fast);
    // The band edge belongs to the slower band: 300ms is no longer "fast".
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("300 ms")), Severity::Medium);
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("799 ms")), Severity::Medium);
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("800 ms")), Severity::Slow);
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("12345 ms")), Severity::Slow);

    // The services emit "Accessible <n> ms" through UrlProbeLatency, so that form
    // has to land in the same bands.
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("Accessible 150 ms")), Severity::Fast);
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral(" 250 ms ")), Severity::Fast);
}

void UiPresentationLogicTests::resultStyleTreatsUnreadableResultsAsFailures()
{
    using ServerTableResultStyle::Severity;

    // These are the vocabulary the speed test writes when a probe fails, plus a
    // raw error string. None of them is a latency, so none may read as healthy.
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("Timeout")), Severity::Failure);
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("Blocked")), Severity::Failure);
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("Connection refused")), Severity::Failure);

    // An empty result means "never tested", which is not a failure.
    QCOMPARE(ServerTableResultStyle::classify(QString()), Severity::None);
    QCOMPARE(ServerTableResultStyle::classify(QStringLiteral("   ")), Severity::None);
}

void UiPresentationLogicTests::themeResolutionFoldsUnknownNamesToLight()
{
    // normalizeThemeName() is the pre-existing contract: only "Dark" is special.
    QCOMPARE(AppTheme::normalizeThemeName(QStringLiteral("Dark")), AppTheme::darkThemeName());
    QCOMPARE(AppTheme::normalizeThemeName(QStringLiteral("dark")), AppTheme::darkThemeName());
    QCOMPARE(AppTheme::normalizeThemeName(QStringLiteral("Light")), AppTheme::lightThemeName());
    QCOMPARE(AppTheme::normalizeThemeName(QStringLiteral("nonsense")), AppTheme::lightThemeName());
    QCOMPARE(AppTheme::normalizeThemeName(QString()), AppTheme::lightThemeName());

    // A non-System preference resolves to itself, ignoring the platform.
    QCOMPARE(
        AppTheme::resolveThemeName(
            QStringLiteral("Dark"),
            AppTheme::lightThemeName(),
            AppTheme::lightThemeName()),
        AppTheme::darkThemeName());
}

void UiPresentationLogicTests::themeResolutionHonoursSystemPreference()
{
    // "System" is the preference; the OS answer decides the palette.
    QCOMPARE(
        AppTheme::resolveThemeName(
            AppTheme::systemThemeName(),
            AppTheme::darkThemeName(),
            AppTheme::lightThemeName()),
        AppTheme::darkThemeName());
    QCOMPARE(
        AppTheme::resolveThemeName(
            AppTheme::systemThemeName(),
            AppTheme::lightThemeName(),
            AppTheme::darkThemeName()),
        AppTheme::lightThemeName());

    // A platform that reports something odd still lands on a real palette.
    QCOMPARE(
        AppTheme::resolveThemeName(
            AppTheme::systemThemeName(),
            QStringLiteral("banana"),
            AppTheme::darkThemeName()),
        AppTheme::lightThemeName());
}

void UiPresentationLogicTests::themeResolutionFallsBackWhenPlatformReportsNothing()
{
    // No platform preference: the fallback keeps the previous default so a System
    // selection still starts rather than leaving the app unstyled.
    QCOMPARE(
        AppTheme::resolveThemeName(AppTheme::systemThemeName(), QString(), AppTheme::lightThemeName()),
        AppTheme::lightThemeName());
    QCOMPARE(
        AppTheme::resolveThemeName(AppTheme::systemThemeName(), QString(), AppTheme::darkThemeName()),
        AppTheme::darkThemeName());
}

void UiPresentationLogicTests::systemThemeNameIsRecognisedCaseInsensitively()
{
    QVERIFY(AppTheme::isSystemThemeName(AppTheme::systemThemeName()));
    QVERIFY(AppTheme::isSystemThemeName(QStringLiteral("system")));
    QVERIFY(AppTheme::isSystemThemeName(QStringLiteral("  SYSTEM  ")));
    QVERIFY(AppTheme::isSystemThemeName(QStringLiteral("Follow")));
    QVERIFY(AppTheme::isSystemThemeName(QStringLiteral("Auto")));

    // Light/Dark are palettes, not the preference, and an empty value is unset.
    QVERIFY(!AppTheme::isSystemThemeName(AppTheme::lightThemeName()));
    QVERIFY(!AppTheme::isSystemThemeName(AppTheme::darkThemeName()));
    QVERIFY(!AppTheme::isSystemThemeName(QString()));
}

void UiPresentationLogicTests::coreStatusOffersCancelOnlyWhileUpdating()
{
    CoreSettingsPageSupport::CoreStatusInput idle;
    idle.coreType = CoreType::Xray;
    // Nothing is running, so there is nothing to cancel.
    QVERIFY(!CoreSettingsPageSupport::coreStatusPresentation(idle).cancelButtonVisible);

    CoreSettingsPageSupport::CoreStatusInput running;
    running.coreType = CoreType::Xray;
    running.updatingCoreType = CoreType::Xray;
    running.downloadActive = true;
    const CoreSettingsPageSupport::CoreStatusPresentation runningPresentation =
        CoreSettingsPageSupport::coreStatusPresentation(running);
    QVERIFY(runningPresentation.cancelButtonVisible);
    QVERIFY(runningPresentation.cancelButtonEnabled);
    QVERIFY(!runningPresentation.downloadButtonVisible);

    // A cancel already in flight greys the button out rather than removing it, so
    // the user can see the request was taken.
    CoreSettingsPageSupport::CoreStatusInput cancelling = running;
    cancelling.cancelRequested = true;
    const CoreSettingsPageSupport::CoreStatusPresentation cancellingPresentation =
        CoreSettingsPageSupport::coreStatusPresentation(cancelling);
    QVERIFY(cancellingPresentation.cancelButtonVisible);
    QVERIFY(!cancellingPresentation.cancelButtonEnabled);
}

void UiPresentationLogicTests::coreStatusTurnsTheButtonIntoARetryAfterAFailure()
{
    CoreSettingsPageSupport::CoreStatusInput installed;
    installed.coreType = CoreType::Xray;
    installed.existingCoreTypes = {CoreType::Xray};
    QCOMPARE(
        CoreSettingsPageSupport::coreStatusPresentation(installed).downloadButtonText,
        QStringLiteral("Update"));

    CoreSettingsPageSupport::CoreStatusInput failed = installed;
    failed.lastAttemptFailed = true;
    failed.failureReason = QStringLiteral("HTTP 404");
    const CoreSettingsPageSupport::CoreStatusPresentation failedPresentation =
        CoreSettingsPageSupport::coreStatusPresentation(failed);
    QCOMPARE(failedPresentation.downloadButtonText, QStringLiteral("Retry"));
    QVERIFY(failedPresentation.downloadButtonEnabled);
    QVERIFY(failedPresentation.downloadButtonToolTip.contains(QStringLiteral("HTTP 404")));

    // Without a reason the tooltip still explains itself instead of going blank.
    CoreSettingsPageSupport::CoreStatusInput failedSilently = installed;
    failedSilently.lastAttemptFailed = true;
    QVERIFY(!CoreSettingsPageSupport::coreStatusPresentation(failedSilently).downloadButtonToolTip.isEmpty());
}

void UiPresentationLogicTests::coreStatusTooltipCarriesTheByteCounts()
{
    CoreSettingsPageSupport::CoreStatusInput running;
    running.coreType = CoreType::Xray;
    running.updatingCoreType = CoreType::Xray;
    running.downloadActive = true;
    running.downloadReceivedBytes = 1536 * 1024;
    running.downloadTotalBytes = 3072 * 1024;

    const CoreSettingsPageSupport::CoreStatusPresentation withTotal =
        CoreSettingsPageSupport::coreStatusPresentation(running);
    QVERIFY(withTotal.downloadProgressVisible);
    QCOMPARE(withTotal.downloadPercent, 50);
    QVERIFY(withTotal.downloadProgressToolTip.contains(QStringLiteral("1.5 MB")));
    QVERIFY(withTotal.downloadProgressToolTip.contains(QStringLiteral("3.0 MB")));
    QVERIFY(withTotal.downloadProgressToolTip.contains(QStringLiteral("50%")));

    // An unknown total keeps the bar indeterminate but still reports what arrived.
    running.downloadTotalBytes = 0;
    const CoreSettingsPageSupport::CoreStatusPresentation withoutTotal =
        CoreSettingsPageSupport::coreStatusPresentation(running);
    QVERIFY(withoutTotal.downloadIndeterminate);
    QVERIFY(withoutTotal.downloadProgressToolTip.contains(QStringLiteral("1.5 MB")));
}

QTEST_MAIN(UiPresentationLogicTests)

#include "UiPresentationLogicTests.moc"
