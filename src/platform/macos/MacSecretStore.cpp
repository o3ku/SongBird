#include "platform/macos/MacSecretStore.h"

#include <QByteArray>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecRandom.h>

#include "platform/macos/MacSecretCipher.h"

namespace {

using MacSecretCipher::kKeyLength;
using MacSecretCipher::kNonceLength;

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
    return key.size() == kKeyLength ? key : QByteArray();
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

    key.resize(kKeyLength);
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

    const QByteArray packed = MacSecretCipher::seal(key, nonce, plaintext.toUtf8());
    if (packed.isEmpty()) {
        return {};
    }

    // nonce || ciphertext || tag: everything the decrypt side needs travels
    // with the value, so no per-value Keychain item is ever created.
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
    if (packed.size() < MacSecretCipher::kOverhead) {
        return {};
    }

    const QByteArray key = loadOrCreateKey();
    if (key.isEmpty()) {
        return {};
    }

    const auto plaintext = MacSecretCipher::open(key, packed);
    return plaintext ? QString::fromUtf8(*plaintext) : QString();
}
