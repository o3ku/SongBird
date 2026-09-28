#include <QtTest>

#include <QDialogButtonBox>
#include <QPushButton>

#include "common/DialogUtils.h"
#include "common/UiLanguage.h"

class UiLanguageTests : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void explicitEnglishWinsOverChineseSystemLocale();
    void explicitChineseWinsOverEnglishSystemLocale();
    void unsetLanguageFollowsSystemLocale();
    void onlySimplifiedChineseSystemLocaleCounts();
    void configuredLanguageRoundTrips();
    void standardDialogButtonsFollowUiLanguageNotSystemLocale();
};

void UiLanguageTests::init()
{
    UiLanguage::setConfiguredLanguage(QString());
}

void UiLanguageTests::explicitEnglishWinsOverChineseSystemLocale()
{
    // This is the reported defect: an English UI on a Chinese Windows install used to
    // keep localizing its standard dialog buttons, because the gate read the system
    // locale instead of the in-app language setting.
    QVERIFY(!UiLanguage::isChinese(QStringLiteral("en"), QStringLiteral("zh_CN")));

    UiLanguage::setConfiguredLanguage(QStringLiteral("en"));
    QVERIFY(!UiLanguage::isChinese());
}

void UiLanguageTests::explicitChineseWinsOverEnglishSystemLocale()
{
    QVERIFY(UiLanguage::isChinese(QStringLiteral("zh_CN"), QStringLiteral("en_US")));

    UiLanguage::setConfiguredLanguage(QStringLiteral("zh_CN"));
    QVERIFY(UiLanguage::isChinese());
}

void UiLanguageTests::unsetLanguageFollowsSystemLocale()
{
    // An unset code is the settings default ("System Default"), which follows the
    // system locale exactly like installConfiguredTranslator() does.
    QVERIFY(UiLanguage::isChinese(QString(), QStringLiteral("zh_CN")));
    QVERIFY(!UiLanguage::isChinese(QString(), QStringLiteral("en_US")));
    QVERIFY(!UiLanguage::isChinese(QString(), QStringLiteral("ja_JP")));
}

void UiLanguageTests::onlySimplifiedChineseSystemLocaleCounts()
{
    // SongBird ships only SongBird_zh_CN.qm, so a Traditional-Chinese system locale
    // leaves the UI in English and must not be treated as Chinese.
    QVERIFY(!UiLanguage::isChinese(QString(), QStringLiteral("zh_TW")));
    QVERIFY(!UiLanguage::isChinese(QString(), QStringLiteral("zh_HK")));
}

void UiLanguageTests::configuredLanguageRoundTrips()
{
    QCOMPARE(UiLanguage::configuredLanguage(), QString());

    UiLanguage::setConfiguredLanguage(QStringLiteral("zh_CN"));
    QCOMPARE(UiLanguage::configuredLanguage(), QStringLiteral("zh_CN"));
}

void UiLanguageTests::standardDialogButtonsFollowUiLanguageNotSystemLocale()
{
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton* okButton = buttonBox.button(QDialogButtonBox::Ok);
    QPushButton* cancelButton = buttonBox.button(QDialogButtonBox::Cancel);
    QVERIFY(okButton != nullptr);
    QVERIFY(cancelButton != nullptr);

    UiLanguage::setConfiguredLanguage(QStringLiteral("en"));
    DialogUtils::localizeStandardDialogButtonBox(&buttonBox);
    QCOMPARE(okButton->text(), QStringLiteral("OK"));
    QCOMPARE(cancelButton->text(), QStringLiteral("Cancel"));

    // The Chinese path must stay covered: Qt itself does *not* translate these buttons
    // on Windows. QDialogButtonBox reads its text from
    // QPlatformTheme::defaultStandardButtonText(), which uses the "QPlatformTheme"
    // translation context, and qt_zh_CN.qm has no such context. So
    // localizeStandardDialogButtonBox() is load bearing and must not be deleted.
    UiLanguage::setConfiguredLanguage(QStringLiteral("zh_CN"));
    DialogUtils::localizeStandardDialogButtonBox(&buttonBox);
    QCOMPARE(okButton->text(), QStringLiteral("确定"));
    QCOMPARE(cancelButton->text(), QStringLiteral("取消"));
}

QTEST_MAIN(UiLanguageTests)
#include "UiLanguageTests.moc"
