#include "platform/macos/MacSecretStore.h"

#include <QByteArray>

#include <cstring>

#include <CommonCrypto/CommonCrypto.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecRandom.h>

#include "common/SealedValue.h"

namespace {

constexpr size_t kKeyLength = 32;
constexpr size_t kIvLength = SealedValue::kIvLength;

CFStringRef keychainService()
{
    return CFSTR("SongBird");
}

CFStringRef keychainAccount()
{
    return CFSTR("songbird-config-key");
}

// One query dictionary, shared by the fetch and the store: service + account
// identify the single key item, and the caller adds the return/store keys.
CFMutableDictionaryRef keyQuery()
{
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, keychainService());
    CFDictionarySetValue(query, kSecAttrAccount, keychainAccount());
    return query;
}

QByteArray fetchKeychainKey()
{
    CFMutableDictionaryRef query = keyQuery();
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status != errSecSuccess || result == nullptr) {
        return {};
    }

    QByteArray key;
    if (CFGetTypeID(result) == CFDataGetTypeID()) {
        const CFDataRef data = static_cast<CFDataRef>(result);
        key = QByteArray(
            reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
            static_cast<int>(CFDataGetLength(data)));
    }
    CFRelease(result);
    return key.size() == static_cast<int>(kKeyLength) ? key : QByteArray();
}

bool storeKeychainKey(const QByteArray& key)
{
    CFDataRef keyData = CFDataCreate(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(key.constData()),
        static_cast<CFIndex>(key.size()));
    CFMutableDictionaryRef query = keyQuery();
    CFDictionarySetValue(query, kSecValueData, keyData);
    CFRelease(keyData);
    // The key is only usable while the user is logged in; it must not be
    // readable from a locked machine or a backup taken offline.
    CFDictionarySetValue(query, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked);

    OSStatus status = SecItemAdd(query, nullptr);
    CFRelease(query);
    if (status == errSecDuplicateItem) {
        // Two processes racing the first save: the other one won, so the key
        // already there is the key. Re-fetching keeps exactly one item.
        return !fetchKeychainKey().isEmpty();
    }
    return status == errSecSuccess;
}

QByteArray loadOrCreateKey()
{
    QByteArray key = fetchKeychainKey();
    if (!key.isEmpty()) {
        return key;
    }

    key.resize(static_cast<int>(kKeyLength));
    if (SecRandomCopyBytes(
            kSecRandomDefault,
            kKeyLength,
            reinterpret_cast<uint8_t*>(key.data())) != 0) {
        return {};
    }

    if (!storeKeychainKey(key)) {
        return {};
    }

    return fetchKeychainKey();
}

QByteArray randomBytes(size_t count)
{
    QByteArray bytes(static_cast<int>(count), 0);
    if (SecRandomCopyBytes(
            kSecRandomDefault, count, reinterpret_cast<uint8_t*>(bytes.data())) != 0) {
        return {};
    }
    return bytes;
}

// One purpose-bound subkey from the Keychain master key: the encryption key
// and the MAC key must differ, so a weakness in one construction cannot be
// carried into the other. The labels are part of the format -- changing one
// orphans every sealed value, exactly like changing the master key.
QByteArray deriveKey(const QByteArray& masterKey, const char* label)
{
    unsigned char derived[CC_SHA256_DIGEST_LENGTH];
    CCHmac(
        kCCHmacAlgSHA256,
        masterKey.constData(),
        static_cast<size_t>(masterKey.size()),
        label,
        std::strlen(label),
        derived);
    return QByteArray(reinterpret_cast<const char*>(derived), CC_SHA256_DIGEST_LENGTH);
}

// AES-CBC with PKCS#7 padding, via the public CCCryptorCreateWithMode API.
// The GCM mode this file used before is private SPI (kCCModeGCM and the
// CCCryptorGCM* calls are not in the public headers) and does not compile.
QByteArray aesCbcCrypt(const QByteArray& key, const QByteArray& iv, const QByteArray& input, bool encrypt)
{
    CCCryptorRef cryptor = nullptr;
    const CCCryptorStatus createStatus = CCCryptorCreateWithMode(
        encrypt ? kCCEncrypt : kCCDecrypt,
        kCCModeCBC,
        kCCAlgorithmAES,
        ccPKCS7Padding,
        iv.constData(),
        key.constData(),
        static_cast<size_t>(key.size()),
        nullptr,
        0,
        0,
        0,
        &cryptor);
    if (createStatus != kCCSuccess || cryptor == nullptr) {
        return {};
    }

    // CBC with PKCS#7 padding grows the plaintext by at most one block on
    // encrypt; the ciphertext is already a whole number of blocks.
    QByteArray output(input.size() + static_cast<int>(kCCBlockSizeAES128), 0);
    size_t moved = 0;
    CCCryptorStatus status = CCCryptorUpdate(
        cryptor,
        input.constData(),
        static_cast<size_t>(input.size()),
        output.data(),
        static_cast<size_t>(output.size()),
        &moved);
    size_t total = moved;
    if (status == kCCSuccess) {
        moved = 0;
        status = CCCryptorFinal(
            cryptor,
            output.data() + total,
            static_cast<size_t>(output.size() - total),
            &moved);
        total += moved;
    }

    CCCryptorRelease(cryptor);
    if (status != kCCSuccess) {
        return {};
    }
    output.resize(static_cast<int>(total));
    return output;
}

QByteArray hmacSha256(const QByteArray& key, const QByteArray& data)
{
    unsigned char mac[CC_SHA256_DIGEST_LENGTH];
    CCHmac(
        kCCHmacAlgSHA256,
        key.constData(),
        static_cast<size_t>(key.size()),
        data.constData(),
        static_cast<size_t>(data.size()),
        mac);
    return QByteArray(reinterpret_cast<const char*>(mac), CC_SHA256_DIGEST_LENGTH);
}

} // namespace

QString MacSecretStore::protect(const QString& plaintext) const
{
    if (plaintext.isEmpty()) {
        return {};
    }

    const QByteArray masterKey = loadOrCreateKey();
    if (masterKey.isEmpty()) {
        return {};
    }

    const QByteArray iv = randomBytes(kIvLength);
    if (iv.isEmpty()) {
        return {};
    }

    const QByteArray packed = SealedValue::seal(
        deriveKey(masterKey, "songbird-secret-encryption"),
        deriveKey(masterKey, "songbird-secret-authentication"),
        iv,
        plaintext.toUtf8(),
        aesCbcCrypt,
        hmacSha256);
    if (packed.isEmpty()) {
        return {};
    }

    // version || iv || ciphertext || mac: everything the decrypt side needs
    // travels with the value, so no per-value Keychain item is ever created.
    return SecretStoreFormat::protectedPrefix()
        + QString::fromLatin1(packed.toBase64());
}

QString MacSecretStore::unprotect(const QString& stored) const
{
    if (!SecretStoreFormat::isProtectedValue(stored)) {
        return {};
    }

    const QByteArray packed = QByteArray::fromBase64(
        SecretStoreFormat::stripProtectedPrefix(stored).toLatin1());
    if (packed.isEmpty()) {
        return {};
    }

    const QByteArray masterKey = loadOrCreateKey();
    if (masterKey.isEmpty()) {
        return {};
    }

    // The MAC is verified before anything is decrypted (SealedValue::unseal),
    // and a value sealed by the old GCM construction fails the version check
    // closed: the macOS build never shipped with GCM, so no such value can
    // exist in the wild, and a tampered one must never decrypt to garbage.
    const QByteArray plaintext = SealedValue::unseal(
        deriveKey(masterKey, "songbird-secret-encryption"),
        deriveKey(masterKey, "songbird-secret-authentication"),
        packed,
        aesCbcCrypt,
        hmacSha256);
    if (plaintext.isEmpty()) {
        return {};
    }

    return QString::fromUtf8(plaintext);
}
