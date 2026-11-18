#pragma once

#include "platform/ISecretStore.h"

// macOS Keychain secret store.
//
// The encryption key itself -- one random 256-bit key -- lives as a generic
// password item in the user's Keychain; the values in the config file are
// AES-256-GCM ciphertexts under that key. Splitting it this way keeps the
// stored form self-contained (the file carries nonce + ciphertext + tag, so a
// value can be decrypted without a per-value Keychain lookup) while no key
// material ever touches the config file. A config copied to another machine
// does not carry its key with it, which is the point.
class MacSecretStore final : public ISecretStore {
public:
    bool available() const override { return true; }
    QString protect(const QString& plaintext) const override;
    QString unprotect(const QString& stored) const override;
};
