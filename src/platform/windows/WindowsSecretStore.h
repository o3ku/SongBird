#pragma once

#include "platform/ISecretStore.h"

// DPAPI (user scope) secret store.
//
// CryptProtectData with CRYPTPROTECT_UI_FORBIDDEN binds the ciphertext to
// this Windows user account: the same user on the same machine decrypts it,
// nobody else does, and no key material is ever written to disk by this
// class. No extra entropy is supplied -- a constant baked into the binary
// would not be a key, it would be a comment -- so the protection is exactly
// what user-scope DPAPI provides.
class WindowsSecretStore final : public ISecretStore {
public:
    bool available() const override { return true; }
    QString protect(const QString& plaintext) const override;
    QString unprotect(const QString& stored) const override;
};
