#pragma once

#include "platform/ISecretStore.h"

// macOS Keychain secret store.
//
// The encryption key itself -- one random 256-bit key -- lives as a generic
// password item in the user's Keychain; the values in the config file are
// AES-256-CBC ciphertexts with an HMAC-SHA256 tag under subkeys derived from
// that key (encrypt-then-MAC, see SealedValue). Splitting it this way keeps
// the stored form self-contained (the file carries version + iv + ciphertext
// + mac, so a value can be decrypted without a per-value Keychain lookup)
// while no key material ever touches the config file. A config copied to
// another machine does not carry its key with it, which is the point.
class MacSecretStore final : public ISecretStore {
public:
    bool available() const override { return true; }
    QString protect(const QString& plaintext) const override;
    QString unprotect(const QString& stored) const override;
};
