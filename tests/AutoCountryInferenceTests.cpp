#include <QtTest/QtTest>

#include "auto/AutoCountryInference.h"

class AutoCountryInferenceTests : public QObject
{
    Q_OBJECT

private slots:
    void infersFromFlagEmoji();
    void infersFromAliases();
    void infersFromSeparatedCountryCode();
    void unrecognizedNamesBecomeUnknown();
    void unsupportedCountriesBecomeUnknown();
};

void AutoCountryInferenceTests::infersFromFlagEmoji()
{
    QCOMPARE(inferAutoCountryCodeFromNodeName(QString::fromUcs4(U"\U0001F1FA\U0001F1F8 Netflix")), QStringLiteral("US"));
    QCOMPARE(inferAutoCountryCodeFromNodeName(QString::fromUcs4(U"\U0001F1ED\U0001F1F0 Hong Kong")), QStringLiteral("HK"));
}

void AutoCountryInferenceTests::infersFromAliases()
{
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("香港 01")), QStringLiteral("HK"));
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("United States Premium")), QStringLiteral("US"));
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("日本 Tokyo")), QStringLiteral("JP"));
}

void AutoCountryInferenceTests::infersFromSeparatedCountryCode()
{
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("node-US-01")), QStringLiteral("US"));
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("USA premium")), QStringLiteral("US"));
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("UK media")), QStringLiteral("GB"));
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("[SG] low latency")), QStringLiteral("SG"));
}

void AutoCountryInferenceTests::unrecognizedNamesBecomeUnknown()
{
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("github.com/example_a2a74db6")), QStringLiteral("UNKNOWN"));
}

void AutoCountryInferenceTests::unsupportedCountriesBecomeUnknown()
{
    QCOMPARE(inferAutoCountryCodeFromNodeName(QString::fromUcs4(U"\U0001F1E8\U0001F1E6 Canada")), QStringLiteral("UNKNOWN"));
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("法国 Paris")), QStringLiteral("UNKNOWN"));
    QCOMPARE(inferAutoCountryCodeFromNodeName(QStringLiteral("node-CN-01")), QStringLiteral("UNKNOWN"));
}

QTEST_MAIN(AutoCountryInferenceTests)

#include "AutoCountryInferenceTests.moc"
