#pragma once

#include <QByteArray>

#include <optional>

namespace MacSecretCipher {

inline constexpr int kKeyLength = 32;
inline constexpr int kNonceLength = 12;
inline constexpr int kTagLength = 16;
inline constexpr int kOverhead = kNonceLength + kTagLength;

QByteArray seal(const QByteArray& key, const QByteArray& nonce, const QByteArray& plaintext);
std::optional<QByteArray> open(const QByteArray& key, const QByteArray& packed);

} // namespace MacSecretCipher
