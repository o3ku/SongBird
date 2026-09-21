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
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("Hiddify")), QStringLiteral("Hiddify/2.5.7"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("v2rayn")), QStringLiteral("v2rayN/7.10.4"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("mihomo")), QStringLiteral("mihomo/1.19.28"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("mihomo party")),
             QStringLiteral("mihomo.party/v2.0.0 (clash.meta)"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("clash verge")), QStringLiteral("clash-verge"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("flclash")), QStringLiteral("FLClash"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("clash meta")), QStringLiteral("ClashMeta"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("v2ray")), QStringLiteral("v2ray"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("nekobox android")),
             QStringLiteral("NekoBox/Android/1.4.1 (Prefer ClashMeta Format)"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("hiddifynext")), QStringLiteral("HiddifyNext"));
    QCOMPARE(resolveSubscriptionUserAgent(QStringLiteral("CustomUA/9.9")), QStringLiteral("CustomUA/9.9"));

    // Karing's own default client UA is the first entry of its kUserAgentList
    // (KaringX/karing, setting_manager.dart): mihomo/1.19.28.
    QCOMPARE(fallbackUserAgent(), QStringLiteral("mihomo/1.19.28"));

    const QStringList labels = subscriptionUserAgentPresetLabels();
    QCOMPARE(labels.indexOf(QStringLiteral("clash verge rev")), 1);
    QCOMPARE(labels.indexOf(QStringLiteral("loon")), 7);
    QCOMPARE(labels.indexOf(QStringLiteral("mihomo")), 8);
    QCOMPARE(labels.indexOf(QStringLiteral("clash verge")), 9);
    QCOMPARE(labels.indexOf(QStringLiteral("flclash")), 10);
    QCOMPARE(labels.indexOf(QStringLiteral("mihomo party")), 11);
    QCOMPARE(labels.indexOf(QStringLiteral("clash meta")), 12);
    QCOMPARE(labels.indexOf(QStringLiteral("v2ray")), 13);
    QCOMPARE(labels.indexOf(QStringLiteral("nekobox android")), 14);
    QCOMPARE(labels.indexOf(QStringLiteral("hiddifynext")), 15);
}

QTEST_MAIN(UserAgentTests)
#include "UserAgentTests.moc"
