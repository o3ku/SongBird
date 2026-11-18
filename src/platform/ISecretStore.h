#pragma once

#include <QString>

// Protects short credential strings at rest.
//
// The configuration file holds proxy credentials (node UUIDs and passwords,
// WireGuard private keys, the local inbound password). Writing them verbatim
// makes a config backup a credential backup too, so the persistence layer
// stores them through this interface instead: protect() turns a plaintext
// credential into an opaque stored form, unprotect() reverses it. The stored
// form is only meaningful on the machine and user account that created it --
// moving the file to another machine does not move the credentials with it,
// which is the point.
//
// The interface is deliberately narrow: the repository never asks how a value
// is protected, only whether a store exists and whether a round trip works.
// A platform without a store reports available() == false, and the caller
// keeps the plaintext value while recording that it did so (see
// JsonConfigRepository::lastSecretStoreWarning()); a store must never fail
// silently into plaintext.
class ISecretStore {
public:
    virtual ~ISecretStore() = default;

    // True when protect()/unprotect() can work on this machine.
    virtual bool available() const = 0;

    // Returns the stored form of `plaintext`, or an empty string when it
    // cannot be protected. An empty plaintext is not protected at all and
    // returns empty, so callers must not treat an empty result as a failure
    // for an empty input.
    virtual QString protect(const QString& plaintext) const = 0;

    // Reverses protect(). Returns an empty string when `stored` is not a
    // protected value or cannot be decrypted; a protected value never
    // decrypts to empty, so an empty result always means failure here.
    virtual QString unprotect(const QString& stored) const = 0;
};

namespace SecretStoreFormat {

// The stored form is `enc:v1:<platform payload>`. The version lets a future
// format change tell old values apart instead of feeding them to the wrong
// decoder; the payload itself is opaque to every reader but the store.
inline QString protectedPrefix()
{
    return QStringLiteral("enc:v1:");
}

inline bool isProtectedValue(const QString& value)
{
    return value.startsWith(protectedPrefix());
}

inline QString stripProtectedPrefix(const QString& value)
{
    return value.mid(protectedPrefix().size());
}

} // namespace SecretStoreFormat
