#pragma once

#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <functional>

// Encrypt-then-MAC sealing for short secrets, with the layout and the
// verification order factored out of any one platform crypto library so the
// security-critical decisions can be unit-tested without it.
//
// The platform store supplies the primitives (AES-CBC and HMAC-SHA256 on
// macOS via CommonCrypto); this header owns only the format and the order of
// operations, so a wrong primitive still cannot turn a tampered value into a
// plaintext: the MAC is verified before anything is decrypted, and the
// comparison runs in constant time.
//
// Wire format, after the store's own prefix and base64:
//   version (1 byte, 0x02) || iv (16 bytes) || ciphertext || mac (32 bytes)
// The MAC covers version || iv || ciphertext. A value sealed by the old GCM
// construction has no version byte and fails the check closed.
namespace SealedValue {

inline constexpr std::uint8_t kVersion = 0x02;
inline constexpr std::size_t kIvLength = 16;
inline constexpr std::size_t kMacLength = 32;

// AES-CBC with PKCS#7 padding under `key`; `encrypt` selects the direction.
// Returns an empty array on failure.
using BlockCipher = std::function<QByteArray(const QByteArray& key, const QByteArray& iv, const QByteArray& input, bool encrypt)>;

// HMAC-SHA256 under `key`. Returns an empty array on failure.
using MacFunction = std::function<QByteArray(const QByteArray& key, const QByteArray& data)>;

// Compares two digests without early exit, so the time taken does not reveal
// how many leading bytes matched. Different lengths compare unequal.
inline bool constantTimeEqual(const QByteArray& expected, const QByteArray& actual)
{
    if (expected.size() != actual.size()) {
        return false;
    }
    std::uint8_t diff = 0;
    for (int i = 0; i < expected.size(); ++i) {
        diff |= static_cast<std::uint8_t>(expected.at(i) ^ actual.at(i));
    }
    return diff == 0;
}

namespace detail {

// The exact bytes the MAC covers: the version binds the format, the iv
// binds the ciphertext to its randomiser.
inline QByteArray macInput(const QByteArray& iv, const QByteArray& ciphertext)
{
    QByteArray input;
    input.append(static_cast<char>(kVersion));
    input.append(iv);
    input.append(ciphertext);
    return input;
}

} // namespace detail

// Seals `plaintext`: encrypt, then MAC the sealed bytes, then pack.
// Returns an empty array when any input is unusable or a primitive fails.
inline QByteArray seal(
    const QByteArray& encryptionKey,
    const QByteArray& macKey,
    const QByteArray& iv,
    const QByteArray& plaintext,
    const BlockCipher& cipher,
    const MacFunction& mac)
{
    if (encryptionKey.isEmpty() || macKey.isEmpty()
        || iv.size() != static_cast<int>(kIvLength) || !cipher || !mac) {
        return {};
    }

    const QByteArray ciphertext = cipher(encryptionKey, iv, plaintext, true);
    if (ciphertext.isEmpty() || ciphertext.size() % static_cast<int>(kIvLength) != 0) {
        return {};
    }

    const QByteArray tag = mac(macKey, detail::macInput(iv, ciphertext));
    if (tag.size() != static_cast<int>(kMacLength)) {
        return {};
    }

    QByteArray packed;
    packed.append(static_cast<char>(kVersion));
    packed.append(iv);
    packed.append(ciphertext);
    packed.append(tag);
    return packed;
}

// Opens a value sealed by seal(): checks the version and shape, verifies the
// MAC, and only then decrypts. Returns an empty array on any failure; a
// failed MAC means the cipher is never invoked. An empty result always means
// failure here because protect() never seals an empty plaintext.
inline QByteArray unseal(
    const QByteArray& encryptionKey,
    const QByteArray& macKey,
    const QByteArray& packed,
    const BlockCipher& cipher,
    const MacFunction& mac)
{
    const int minimumSize = static_cast<int>(1 + kIvLength + kIvLength + kMacLength);
    if (encryptionKey.isEmpty() || macKey.isEmpty() || packed.size() < minimumSize || !cipher || !mac) {
        return {};
    }
    if (static_cast<std::uint8_t>(packed.at(0)) != kVersion) {
        return {};
    }

    const QByteArray iv = packed.mid(1, static_cast<int>(kIvLength));
    const QByteArray ciphertext = packed.mid(
        1 + static_cast<int>(kIvLength),
        packed.size() - static_cast<int>(1 + kIvLength + kMacLength));
    const QByteArray tag = packed.right(static_cast<int>(kMacLength));
    if (ciphertext.size() % static_cast<int>(kIvLength) != 0) {
        return {};
    }

    const QByteArray expectedTag = mac(macKey, detail::macInput(iv, ciphertext));
    if (expectedTag.size() != static_cast<int>(kMacLength) || !constantTimeEqual(expectedTag, tag)) {
        return {};
    }

    return cipher(encryptionKey, iv, ciphertext, false);
}

} // namespace SealedValue
