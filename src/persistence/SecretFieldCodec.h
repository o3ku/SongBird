#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "platform/ISecretStore.h"

// Translates the stored form of credential fields in a raw config document.
//
// The JSON serializers (JsonConfigServerSerialization, JsonConfigRootSerialization)
// stay the authority on which fields exist and what they are called; this codec
// only rewrites the values of the credential fields, in place, on the document
// the repository already holds. Keeping the translation here -- rather than in
// the serializers -- means the field list is consulted in exactly two places
// (this file and the serializer) and the migration report has one producer.
//
// Which fields are credentials: `id` is the VMess/VLESS UUID and the
// Trojan/Shadowsocks/SOCKS/HTTP password; `obfsPassword` is the Hysteria2
// obfuscation password; `privateKey` is the WireGuard private key. `security`
// is a cipher/method name, `publicKey`/`peerPublicKey`/`cert` are public, and
// `extra` is free-form user text the dialogs display, so none of those are
// translated here.
namespace SecretFieldCodec {

inline QStringList serverSecretKeys()
{
    return {
        QStringLiteral("id"),
        QStringLiteral("obfsPassword"),
        QStringLiteral("privateKey"),
    };
}

// Key of the local inbound password in the root "inbound" object.
inline QString inboundPasswordKey()
{
    return QStringLiteral("pass");
}

struct LoadReport {
    // At least one credential was stored as legacy plaintext while a store
    // is available. The value loads as-is; the repository re-saves it
    // protected (migration) instead of warning about it.
    bool legacyPlaintextSecretsFound = false;
    // At least one protected value could not be decrypted. Its in-memory
    // value is empty, and the repository must not re-save over it: the stored
    // form stays in the file so the credential is not destroyed by a load.
    bool secretDecryptFailed = false;
    // At least one credential stayed plaintext because no store is available
    // on this platform. The value loads as-is.
    bool storeUnavailable = false;
};

struct SaveReport {
    // At least one credential stayed plaintext because no store is available
    // on this platform.
    bool storeUnavailable = false;
    // At least one credential stayed plaintext because protect() failed.
    bool secretProtectFailed = false;
};

namespace detail {

inline QString decodeSecretValue(const QString& stored, const ISecretStore* store, LoadReport& report)
{
    if (stored.isEmpty()) {
        return {};
    }

    if (SecretStoreFormat::isProtectedValue(stored)) {
        if (store == nullptr || !store->available()) {
            report.secretDecryptFailed = true;
            return {};
        }
        const QString plaintext = store->unprotect(stored);
        if (plaintext.isEmpty()) {
            report.secretDecryptFailed = true;
            return {};
        }
        return plaintext;
    }

    if (store == nullptr || !store->available()) {
        report.storeUnavailable = true;
        return stored;
    }

    report.legacyPlaintextSecretsFound = true;
    return stored;
}

inline void encodeSecretValue(QString& stored, const ISecretStore* store, SaveReport& report)
{
    if (stored.isEmpty() || SecretStoreFormat::isProtectedValue(stored)) {
        return;
    }

    if (store == nullptr || !store->available()) {
        report.storeUnavailable = true;
        return;
    }

    const QString protectedValue = store->protect(stored);
    if (protectedValue.isEmpty()) {
        report.secretProtectFailed = true;
        return;
    }
    stored = protectedValue;
}

} // namespace detail

// Rewrites credential fields in `root` to their in-memory form: protected
// values are decrypted, legacy plaintext is left as-is and reported.
inline LoadReport decodeSecrets(QJsonObject& root, const ISecretStore* store)
{
    LoadReport report;

    QJsonArray servers = root.value(QStringLiteral("servers")).toArray();
    for (int index = 0; index < servers.size(); ++index) {
        QJsonObject server = servers.at(index).toObject();
        for (const QString& key : serverSecretKeys()) {
            if (!server.contains(key)) {
                continue;
            }
            server.insert(key, detail::decodeSecretValue(server.value(key).toString(), store, report));
        }
        servers[index] = server;
    }
    if (!servers.isEmpty()) {
        root.insert(QStringLiteral("servers"), servers);
    }

    QJsonArray inbounds = root.value(QStringLiteral("inbound")).toArray();
    if (!inbounds.isEmpty()) {
        QJsonObject inbound = inbounds.at(0).toObject();
        if (inbound.contains(inboundPasswordKey())) {
            inbound.insert(
                inboundPasswordKey(),
                detail::decodeSecretValue(inbound.value(inboundPasswordKey()).toString(), store, report));
            inbounds[0] = inbound;
            root.insert(QStringLiteral("inbound"), inbounds);
        }
    }

    return report;
}

// Rewrites credential fields in `root` to their stored form: plaintext values
// are protected when a store is available. A value that cannot be protected
// stays plaintext and is reported -- it is never dropped.
inline SaveReport encodeSecrets(QJsonObject& root, const ISecretStore* store)
{
    SaveReport report;

    QJsonArray servers = root.value(QStringLiteral("servers")).toArray();
    for (int index = 0; index < servers.size(); ++index) {
        QJsonObject server = servers.at(index).toObject();
        for (const QString& key : serverSecretKeys()) {
            if (!server.contains(key)) {
                continue;
            }
            QString value = server.value(key).toString();
            detail::encodeSecretValue(value, store, report);
            server.insert(key, value);
        }
        servers[index] = server;
    }
    if (!servers.isEmpty()) {
        root.insert(QStringLiteral("servers"), servers);
    }

    QJsonArray inbounds = root.value(QStringLiteral("inbound")).toArray();
    if (!inbounds.isEmpty()) {
        QJsonObject inbound = inbounds.at(0).toObject();
        if (inbound.contains(inboundPasswordKey())) {
            QString value = inbound.value(inboundPasswordKey()).toString();
            detail::encodeSecretValue(value, store, report);
            inbound.insert(inboundPasswordKey(), value);
            inbounds[0] = inbound;
            root.insert(QStringLiteral("inbound"), inbounds);
        }
    }

    return report;
}

} // namespace SecretFieldCodec
