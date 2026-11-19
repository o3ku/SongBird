#include <QtTest>

#include "common/SealedValue.h"

namespace {

// A deterministic stand-in for HMAC-SHA256: any input change must change the
// tag, and the tag must be exactly 32 bytes. It is a test double, not a MAC.
QByteArray fakeMac(const QByteArray& key, const QByteArray& data)
{
    std::uint64_t state = 1469598103934665603ULL;
    const auto mix = [&state](const QByteArray& bytes) {
        for (const char byte : bytes) {
            state ^= static_cast<std::uint8_t>(byte);
            state *= 1099511628211ULL;
        }
    };
    mix(key);
    mix(data);

    QByteArray tag;
    for (int round = 0; round < 4; ++round) {
        std::uint64_t word = state + static_cast<std::uint64_t>(round) * 0x9E3779B97F4A7C15ULL;
        for (int i = 0; i < 8; ++i) {
            tag.append(static_cast<char>(word & 0xFF));
            word >>= 8;
        }
    }
    return tag;
}

// A reversible stand-in for AES-CBC with PKCS#7 padding: XOR with the key
// and iv, with real padding, so the sealed layout sees the same shape as the
// production cipher. Decryption rejects bad padding instead of stripping it
// blindly, the way CCCryptorFinal does.
QByteArray fakeCipher(const QByteArray& key, const QByteArray& iv, const QByteArray& input, bool encrypt)
{
    if (key.isEmpty() || iv.size() != static_cast<int>(SealedValue::kIvLength)) {
        return {};
    }

    const auto xorByte = [&key, &iv](int index, char byte) {
        return static_cast<char>(
            byte ^ key.at(index % key.size()) ^ iv.at(index % static_cast<int>(SealedValue::kIvLength)));
    };

    if (encrypt) {
        QByteArray padded = input;
        const int padLength = static_cast<int>(SealedValue::kIvLength)
            - (input.size() % static_cast<int>(SealedValue::kIvLength));
        for (int i = 0; i < padLength; ++i) {
            padded.append(static_cast<char>(padLength));
        }
        QByteArray output(padded.size(), 0);
        for (int i = 0; i < padded.size(); ++i) {
            output[i] = xorByte(i, padded.at(i));
        }
        return output;
    }

    if (input.isEmpty() || input.size() % static_cast<int>(SealedValue::kIvLength) != 0) {
        return {};
    }
    QByteArray padded(input.size(), 0);
    for (int i = 0; i < input.size(); ++i) {
        padded[i] = xorByte(i, input.at(i));
    }
    const int padLength = static_cast<std::uint8_t>(padded.at(padded.size() - 1));
    if (padLength <= 0 || padLength > static_cast<int>(SealedValue::kIvLength)
        || padLength > padded.size()) {
        return {};
    }
    for (int i = padded.size() - padLength; i < padded.size(); ++i) {
        if (static_cast<std::uint8_t>(padded.at(i)) != static_cast<std::uint8_t>(padLength)) {
            return {};
        }
    }
    return padded.left(padded.size() - padLength);
}

QByteArray flipByte(const QByteArray& value, int index)
{
    QByteArray flipped = value;
    flipped[index] = static_cast<char>(flipped.at(index) ^ 0x01);
    return flipped;
}

} // namespace

class SealedValueTests : public QObject {
    Q_OBJECT

private slots:
    void sealUnsealRoundTrips();
    void tamperedCiphertextFailsAndDecryptIsNotCalled();
    void tamperedMacFails();
    void tamperedIvFails();
    void wrongVersionFails();
    void truncatedValueFails();
    void unsealRejectsCiphertextThatIsNotABlockMultiple();
    void sealRejectsBadInputs();
    void constantTimeEqualBehaves();

private:
    const QByteArray encryptionKey_ = QByteArrayLiteral("0123456789abcdef0123456789abcdef");
    const QByteArray macKey_ = QByteArrayLiteral("fedcba9876543210fedcba9876543210");
    const QByteArray iv_ = QByteArrayLiteral("0123456789abcdef");
};

void SealedValueTests::sealUnsealRoundTrips()
{
    const QByteArray plaintext = QByteArrayLiteral("11111111-1111-1111-1111-111111111111");
    const QByteArray packed = SealedValue::seal(
        encryptionKey_, macKey_, iv_, plaintext, fakeCipher, fakeMac);
    QVERIFY(!packed.isEmpty());
    QCOMPARE(packed.size(), 1 + 16 + 48 + 32);
    QCOMPARE(SealedValue::unseal(encryptionKey_, macKey_, packed, fakeCipher, fakeMac), plaintext);

    // A one-block plaintext still round-trips: PKCS#7 adds a full block.
    const QByteArray block = QByteArrayLiteral("0123456789abcdef");
    const QByteArray blockPacked = SealedValue::seal(
        encryptionKey_, macKey_, iv_, block, fakeCipher, fakeMac);
    QVERIFY(!blockPacked.isEmpty());
    QCOMPARE(SealedValue::unseal(encryptionKey_, macKey_, blockPacked, fakeCipher, fakeMac), block);
}

void SealedValueTests::tamperedCiphertextFailsAndDecryptIsNotCalled()
{
    const QByteArray packed = SealedValue::seal(
        encryptionKey_, macKey_, iv_, QByteArrayLiteral("secret-password"), fakeCipher, fakeMac);
    QVERIFY(!packed.isEmpty());

    int decryptCalls = 0;
    const SealedValue::BlockCipher countingCipher =
        [&decryptCalls](const QByteArray& key, const QByteArray& iv, const QByteArray& input, bool encrypt) {
            if (!encrypt) {
                ++decryptCalls;
            }
            return fakeCipher(key, iv, input, encrypt);
        };

    // Byte 20 is inside the ciphertext (version + 16-byte iv + ciphertext).
    const QByteArray tampered = flipByte(packed, 20);
    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, tampered, countingCipher, fakeMac).isEmpty());
    QCOMPARE(decryptCalls, 0);
}

void SealedValueTests::tamperedMacFails()
{
    const QByteArray packed = SealedValue::seal(
        encryptionKey_, macKey_, iv_, QByteArrayLiteral("secret-password"), fakeCipher, fakeMac);
    QVERIFY(!packed.isEmpty());

    const QByteArray tampered = flipByte(packed, packed.size() - 1);
    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, tampered, fakeCipher, fakeMac).isEmpty());
}

void SealedValueTests::tamperedIvFails()
{
    const QByteArray packed = SealedValue::seal(
        encryptionKey_, macKey_, iv_, QByteArrayLiteral("secret-password"), fakeCipher, fakeMac);
    QVERIFY(!packed.isEmpty());

    // Byte 5 is inside the iv, which the MAC covers.
    const QByteArray tampered = flipByte(packed, 5);
    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, tampered, fakeCipher, fakeMac).isEmpty());
}

void SealedValueTests::wrongVersionFails()
{
    const QByteArray packed = SealedValue::seal(
        encryptionKey_, macKey_, iv_, QByteArrayLiteral("secret-password"), fakeCipher, fakeMac);
    QVERIFY(!packed.isEmpty());

    QByteArray wrongVersion = packed;
    wrongVersion[0] = static_cast<char>(0x01);
    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, wrongVersion, fakeCipher, fakeMac).isEmpty());

    // A value sealed by the old GCM construction has a random first byte and
    // no version marker: it must fail closed the same way.
    QByteArray gcmShaped = packed;
    gcmShaped[0] = static_cast<char>(0x9A);
    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, gcmShaped, fakeCipher, fakeMac).isEmpty());
}

void SealedValueTests::truncatedValueFails()
{
    const QByteArray packed = SealedValue::seal(
        encryptionKey_, macKey_, iv_, QByteArrayLiteral("secret-password"), fakeCipher, fakeMac);
    QVERIFY(!packed.isEmpty());

    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, packed.left(10), fakeCipher, fakeMac).isEmpty());
    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, packed.left(packed.size() - 1), fakeCipher, fakeMac).isEmpty());
    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, QByteArray(), fakeCipher, fakeMac).isEmpty());
}

void SealedValueTests::unsealRejectsCiphertextThatIsNotABlockMultiple()
{
    // A packed value whose ciphertext is 5 bytes, with a MAC computed over
    // it correctly: the shape check, not the MAC, must refuse it.
    const QByteArray iv = iv_;
    const QByteArray ciphertext = QByteArrayLiteral("12345");
    QByteArray macInput;
    macInput.append(static_cast<char>(SealedValue::kVersion));
    macInput.append(iv);
    macInput.append(ciphertext);
    QByteArray packed;
    packed.append(static_cast<char>(SealedValue::kVersion));
    packed.append(iv);
    packed.append(ciphertext);
    packed.append(fakeMac(macKey_, macInput));

    QVERIFY(SealedValue::unseal(encryptionKey_, macKey_, packed, fakeCipher, fakeMac).isEmpty());
}

void SealedValueTests::sealRejectsBadInputs()
{
    const QByteArray plaintext = QByteArrayLiteral("secret-password");
    QVERIFY(SealedValue::seal(QByteArray(), macKey_, iv_, plaintext, fakeCipher, fakeMac).isEmpty());
    QVERIFY(SealedValue::seal(encryptionKey_, QByteArray(), iv_, plaintext, fakeCipher, fakeMac).isEmpty());
    QVERIFY(SealedValue::seal(encryptionKey_, macKey_, QByteArrayLiteral("short"), plaintext, fakeCipher, fakeMac).isEmpty());
    QVERIFY(SealedValue::seal(encryptionKey_, macKey_, iv_, plaintext, nullptr, fakeMac).isEmpty());
    QVERIFY(SealedValue::seal(encryptionKey_, macKey_, iv_, plaintext, fakeCipher, nullptr).isEmpty());
}

void SealedValueTests::constantTimeEqualBehaves()
{
    QVERIFY(SealedValue::constantTimeEqual(QByteArrayLiteral("abc"), QByteArrayLiteral("abc")));
    QVERIFY(!SealedValue::constantTimeEqual(QByteArrayLiteral("abc"), QByteArrayLiteral("abd")));
    QVERIFY(!SealedValue::constantTimeEqual(QByteArrayLiteral("abc"), QByteArrayLiteral("ab")));
    QVERIFY(!SealedValue::constantTimeEqual(QByteArray(), QByteArrayLiteral("a")));
    QVERIFY(SealedValue::constantTimeEqual(QByteArray(), QByteArray()));
}

QTEST_MAIN(SealedValueTests)

#include "SealedValueTests.moc"
