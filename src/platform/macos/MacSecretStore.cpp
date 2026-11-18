#include "platform/macos/MacSecretStore.h"

#include <QByteArray>

#include <CommonCrypto/CommonCrypto.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecRandom.h>

namespace {

constexpr size_t kKeyLength = 32;
constexpr size_t kNonceLength = 12;
constexpr size_t kTagLength = 16;

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

QByteArray gcmCrypt(const QByteArray& key, const QByteArray& nonce, const QByteArray& input, bool encrypt, QByteArray* tag)
{
    CCCryptorRef cryptor = nullptr;
    const CCCryptorStatus createStatus = CCCryptorCreateWithMode(
        encrypt ? kCCEncrypt : kCCDecrypt,
        kCCModeGCM,
        kCCAlgorithmAES,
        ccNoPadding,
        nullptr,
        key.constData(),
        key.size(),
        nullptr,
        0,
        0,
        0,
        &cryptor);
    if (createStatus != kCCSuccess || cryptor == nullptr) {
        return {};
    }

    QByteArray output(input.size(), 0);
    CCCryptorStatus status = CCCryptorGCMAddIV(
        cryptor, nonce.constData(), static_cast<size_t>(nonce.size()));
    if (status == kCCSuccess) {
        status = encrypt
            ? CCCryptorGCMEncrypt(cryptor, input.constData(), static_cast<size_t>(input.size()), output.data())
            : CCCryptorGCMDecrypt(cryptor, input.constData(), static_cast<size_t>(input.size()), output.data());
    }

    // Finalising produces the tag on encrypt and verifies it on decrypt: a
    // tampered value fails here instead of decrypting to garbage.
    if (status == kCCSuccess && tag != nullptr) {
        size_t tagLength = kTagLength;
        if (encrypt) {
            tag->resize(static_cast<int>(kTagLength));
        }
        status = CCCryptorGCMFinal(cryptor, tag->data(), &tagLength);
    }

    CCCryptorRelease(cryptor);
    return status == kCCSuccess ? output : QByteArray();
}

} // namespace

QString MacSecretStore::protect(const QString& plaintext) const
{
    if (plaintext.isEmpty()) {
        return {};
    }

    const QByteArray key = loadOrCreateKey();
    if (key.isEmpty()) {
        return {};
    }

    const QByteArray nonce = randomBytes(kNonceLength);
    if (nonce.isEmpty()) {
        return {};
    }

    QByteArray tag;
    const QByteArray ciphertext = gcmCrypt(key, nonce, plaintext.toUtf8(), true, &tag);
    if (ciphertext.isEmpty() || tag.size() != static_cast<int>(kTagLength)) {
        return {};
    }

    // nonce || ciphertext || tag: everything the decrypt side needs travels
    // with the value, so no per-value Keychain item is ever created.
    return SecretStoreFormat::protectedPrefix()
        + QString::fromLatin1((nonce + ciphertext + tag).toBase64());
}

QString MacSecretStore::unprotect(const QString& stored) const
{
    if (!SecretStoreFormat::isProtectedValue(stored)) {
        return {};
    }

    const QByteArray packed = QByteArray::fromBase64(
        SecretStoreFormat::stripProtectedPrefix(stored).toLatin1());
    if (packed.size() < static_cast<int>(kNonceLength + kTagLength)) {
        return {};
    }

    const QByteArray key = loadOrCreateKey();
    if (key.isEmpty()) {
        return {};
    }

    const QByteArray nonce = packed.left(static_cast<int>(kNonceLength));
    const QByteArray tag = packed.right(static_cast<int>(kTagLength));
    const QByteArray ciphertext = packed.mid(
        static_cast<int>(kNonceLength),
        packed.size() - static_cast<int>(kNonceLength + kTagLength));

    QByteArray expectedTag = tag;
    const QByteArray plaintext = gcmCrypt(key, nonce, ciphertext, false, &expectedTag);
    if (plaintext.isEmpty()) {
        return {};
    }

    return QString::fromUtf8(plaintext);
}
