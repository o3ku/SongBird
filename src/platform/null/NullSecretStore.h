#pragma once

#include "platform/ISecretStore.h"

// The stand-in for platforms without an OS secret store (Linux). It reports
// unavailable, so the persistence layer keeps credentials in plaintext and
// records that it did -- see JsonConfigRepository::lastSecretStoreWarning().
class NullSecretStore final : public ISecretStore {
public:
    bool available() const override { return false; }
    QString protect(const QString&) const override { return {}; }
    QString unprotect(const QString&) const override { return {}; }
};
