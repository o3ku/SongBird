#include "platform/windows/WindowsSecretStore.h"

#include <QByteArray>

#include <windows.h>
#include <dpapi.h>

namespace {

QByteArray protectBytes(const QByteArray& plaintext)
{
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
    input.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB output{};
    // UI forbidden: a background save must never pop a dialog, and a prompt
    // here would be answered by whoever happens to be looking at the screen.
    // Local-machine scope is deliberately not set: user scope binds the value
    // to this account, which is the protection the config file needs.
    if (CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output) == 0) {
        return {};
    }

    const QByteArray protectedBytes(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return protectedBytes;
}

QByteArray unprotectBytes(const QByteArray& protectedBytes)
{
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(protectedBytes.data()));
    input.cbData = static_cast<DWORD>(protectedBytes.size());

    DATA_BLOB output{};
    if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output) == 0) {
        return {};
    }

    const QByteArray plaintext(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return plaintext;
}

} // namespace

QString WindowsSecretStore::protect(const QString& plaintext) const
{
    if (plaintext.isEmpty()) {
        return {};
    }

    const QByteArray protectedBytes = protectBytes(plaintext.toUtf8());
    if (protectedBytes.isEmpty()) {
        return {};
    }

    return SecretStoreFormat::protectedPrefix() + QString::fromLatin1(protectedBytes.toBase64());
}

QString WindowsSecretStore::unprotect(const QString& stored) const
{
    if (!SecretStoreFormat::isProtectedValue(stored)) {
        return {};
    }

    const QByteArray protectedBytes = QByteArray::fromBase64(
        SecretStoreFormat::stripProtectedPrefix(stored).toLatin1());
    if (protectedBytes.isEmpty()) {
        return {};
    }

    const QByteArray plaintext = unprotectBytes(protectedBytes);
    if (plaintext.isEmpty()) {
        return {};
    }

    return QString::fromUtf8(plaintext);
}
