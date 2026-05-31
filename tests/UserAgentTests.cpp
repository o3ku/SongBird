#include <QtTest>

#include "common/UserAgent.h"

class UserAgentTests : public QObject
{
    Q_OBJECT

private slots:
    void resolvesSubscriptionPresetAndLegacyValues();
};

void UserAgentTests::resolvesSubscriptionPresetAndLegacyValues()
{
    QCOMPARE(resolveSubscriptionUserAgent(QString()), fallbackUserAgent());
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("nekobox")), fallbackUserAgent());
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("Nekobox")), fallbackUserAgent());
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("ClashVerge")), QStringLiteral("clash-verge/v2.4"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("v2rayn")), QStringLiteral("v2rayN/7.10.4"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("CustomUA/9.9")), QStringLiteral("CustomUA/9.9"));

    const QStringList labels = subscriptionUserAgentPresetLabels();
    QCOMPARE(labels.indexOf(QStringLiteral("clash verge rev")), 1);
    QCOMPARE(labels.indexOf(QStringLiteral("loon")), 7);
}

QTEST_MAIN(UserAgentTests)
#include "UserAgentTests.moc"
