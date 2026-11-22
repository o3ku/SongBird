#include <QtTest>

#include "platform/macos/MacSecretCipher.h"

class MacSecretCipherTests : public QObject {
    Q_OBJECT

private slots:
    void matchesAes256GcmVectors_data();
    void matchesAes256GcmVectors();
    void rejectsTamperedPayload_data();
    void rejectsTamperedPayload();
};

void MacSecretCipherTests::matchesAes256GcmVectors_data()
{
    QTest::addColumn<QByteArray>("plaintext");
    QTest::addColumn<QByteArray>("ciphertextAndTag");

    QTest::newRow("one block") << QByteArray(16, '\0')
        << QByteArray::fromHex("cea7403d4d606b6e074ec5d3baf39d18"
                               "d0d1c8a799996bf0265b98b5d48ab919");
    QTest::newRow("empty") << QByteArray()
        << QByteArray::fromHex("530f8afbc74536b9a963b4f1c4cb738b");
}

void MacSecretCipherTests::matchesAes256GcmVectors()
{
    QFETCH(QByteArray, plaintext);
    QFETCH(QByteArray, ciphertextAndTag);

    const QByteArray key(32, '\0');
    const QByteArray nonce(12, '\0');
    const QByteArray packed = nonce + ciphertextAndTag;
    QCOMPARE(MacSecretCipher::seal(key, nonce, plaintext), packed);

    const auto opened = MacSecretCipher::open(key, packed);
    QVERIFY(opened.has_value());
    QCOMPARE(*opened, plaintext);
}

void MacSecretCipherTests::rejectsTamperedPayload_data()
{
    QTest::addColumn<QByteArray>("plaintext");
    QTest::addColumn<int>("offset");

    QTest::newRow("nonce") << QByteArray("secret") << 0;
    QTest::newRow("ciphertext") << QByteArray("secret") << 12;
    QTest::newRow("tag") << QByteArray("secret") << 18;
    QTest::newRow("tag with empty ciphertext") << QByteArray() << 12;
}

void MacSecretCipherTests::rejectsTamperedPayload()
{
    QFETCH(QByteArray, plaintext);
    QFETCH(int, offset);

    const QByteArray key(32, '\0');
    const QByteArray nonce(12, '\0');
    QByteArray packed = MacSecretCipher::seal(key, nonce, plaintext);
    QVERIFY(packed.size() > offset);
    QVERIFY(!MacSecretCipher::open(key, packed.chopped(1)).has_value());
    packed[offset] = static_cast<char>(packed.at(offset) ^ 1);
    QVERIFY(!MacSecretCipher::open(key, packed).has_value());
}

QTEST_APPLESS_MAIN(MacSecretCipherTests)

#include "MacSecretCipherTests.moc"
