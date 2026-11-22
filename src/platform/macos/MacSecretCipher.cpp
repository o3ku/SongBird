#include "platform/macos/MacSecretCipher.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <array>
#include <limits>
#include <memory>

namespace MacSecretCipher {
namespace {

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

const unsigned char* bytes(const QByteArray& data)
{
    return reinterpret_cast<const unsigned char*>(data.constData());
}

bool crypt(EVP_CIPHER_CTX* context, const unsigned char* input, int length, QByteArray& output)
{
    int written = 0;
    if (length != 0
        && EVP_CipherUpdate(context, reinterpret_cast<unsigned char*>(output.data()),
            &written, input, length) != 1) {
        return false;
    }

    // GCM writes one byte per input byte and produces no bytes on finalization.
    // A separate buffer gives finalization valid storage even for empty inputs.
    std::array<unsigned char, EVP_MAX_BLOCK_LENGTH> finalOutput;
    int finalLength = 0;
    return EVP_CipherFinal_ex(context, finalOutput.data(), &finalLength) == 1
        && written == length && finalLength == 0;
}

} // namespace

QByteArray seal(const QByteArray& key, const QByteArray& nonce, const QByteArray& plaintext)
{
    if (key.size() != kKeyLength || nonce.size() != kNonceLength
        || plaintext.size() > std::numeric_limits<int>::max() - kOverhead) {
        return {};
    }

    CipherContext context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!context
        || EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, bytes(key), bytes(nonce)) != 1) {
        return {};
    }

    QByteArray ciphertext(plaintext.size(), Qt::Uninitialized);
    QByteArray tag(kTagLength, Qt::Uninitialized);
    if (!crypt(context.get(), bytes(plaintext), static_cast<int>(plaintext.size()), ciphertext)
        || EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_GET_TAG, kTagLength, tag.data()) != 1) {
        return {};
    }
    return nonce + ciphertext + tag;
}

std::optional<QByteArray> open(const QByteArray& key, const QByteArray& packed)
{
    if (key.size() != kKeyLength || packed.size() < kOverhead
        || packed.size() > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }

    CipherContext context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    QByteArray tag = packed.right(kTagLength);
    if (!context
        || EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, bytes(key), bytes(packed)) != 1
        || EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_SET_TAG, kTagLength, tag.data()) != 1) {
        return std::nullopt;
    }

    const int length = static_cast<int>(packed.size()) - kOverhead;
    QByteArray plaintext(length, Qt::Uninitialized);
    if (!crypt(context.get(), bytes(packed) + kNonceLength, length, plaintext)) {
        if (length != 0) {
            OPENSSL_cleanse(plaintext.data(), static_cast<size_t>(length));
        }
        return std::nullopt;
    }
    return plaintext;
}

} // namespace MacSecretCipher
