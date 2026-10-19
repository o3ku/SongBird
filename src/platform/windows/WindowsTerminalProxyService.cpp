#include "platform/windows/WindowsTerminalProxyService.h"

#include <QSettings>
#include <QStringList>

#include <windows.h>

#include "platform/TerminalProxyPolicy.h"

namespace {

constexpr const char* kEnvironmentRegistryPath = "HKEY_CURRENT_USER\\Environment";

const QString kHttpProxy = QStringLiteral("HTTP_PROXY");
const QString kHttpsProxy = QStringLiteral("HTTPS_PROXY");
const QString kAllProxy = QStringLiteral("ALL_PROXY");
const QString kNoProxy = QStringLiteral("NO_PROXY");

// QSettings reports a value it cannot find as an invalid QVariant and one that exists but is empty as
// a valid variant holding "". TerminalProxySnapshot keeps those apart, so this is the only place the
// distinction is read out of the registry.
std::optional<QString> readValue(const QSettings& settings, const QString& name)
{
    const QVariant value = settings.value(name);
    if (!value.isValid()) {
        return std::nullopt;
    }
    return value.toString();
}

// Tells already-running programs to re-read their environment. SendMessageTimeout rather than
// SendMessage: a hung top-level window would otherwise block this call forever. A timeout is treated
// as success -- the message reached the message queues, and a window that did not answer is not ours
// to wait on.
void broadcastEnvironmentChange()
{
    DWORD_PTR result = 0;
    SendMessageTimeoutW(
        HWND_BROADCAST,
        WM_SETTINGCHANGE,
        0,
        reinterpret_cast<LPARAM>(L"Environment"),
        SMTO_ABORTIFHUNG,
        1000,
        &result);
}

// Mirrors the registry value into this process's own environment block.
//
// Windows copies the parent's environment at CreateProcess time, so without this a child process
// launched by SongBird would inherit the block from before the change even though the registry is
// already correct.
void mirrorToProcessEnvironment(const QString& name, const std::optional<QString>& value)
{
    const std::wstring wideName = name.toStdWString();
    if (value.has_value()) {
        const std::wstring wideValue = value->toStdWString();
        SetEnvironmentVariableW(wideName.c_str(), wideValue.c_str());
    } else {
        // A null value is how SetEnvironmentVariableW deletes the variable. Deleting one that is not
        // there is not a failure, so the return value is ignored in both branches: the registry is the
        // source of truth and its status is what the caller is told about.
        SetEnvironmentVariableW(wideName.c_str(), nullptr);
    }
}

} // namespace

WindowsTerminalProxyService::WindowsTerminalProxyService(QString settingsPath)
    : settingsPath_(settingsPath.isEmpty()
            ? QString::fromUtf8(kEnvironmentRegistryPath)
            : std::move(settingsPath))
{
}

bool WindowsTerminalProxyService::enable(const QString& proxyUrl, const QString& noProxy) const
{
    if (proxyUrl.trimmed().isEmpty()) {
        return false;
    }

    // NO_PROXY is only written when the caller supplies one AND the user does not already have one.
    // Overwriting a user's own bypass list would silently route their LAN traffic through the proxy.
    // Decided before the first write so a rejection cannot leave the environment half changed.
    bool shouldWriteNoProxy = false;
    if (!noProxy.trimmed().isEmpty()) {
        QSettings settings(settingsPath_, QSettings::NativeFormat);
        const std::optional<QString> existing = readValue(settings, kNoProxy);
        shouldWriteNoProxy = !existing.has_value() || existing->trimmed().isEmpty();
    }

    QSettings settings(settingsPath_, QSettings::NativeFormat);
    settings.setValue(kHttpProxy, proxyUrl);
    settings.setValue(kHttpsProxy, proxyUrl);
    settings.setValue(kAllProxy, proxyUrl);
    if (shouldWriteNoProxy) {
        settings.setValue(kNoProxy, noProxy);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    // The registry is now correct, so the in-process block is brought in line with it. Done after the
    // status check rather than before, so a refused write does not leave this process pointing at a
    // proxy the registry never recorded.
    mirrorToProcessEnvironment(kHttpProxy, proxyUrl);
    mirrorToProcessEnvironment(kHttpsProxy, proxyUrl);
    mirrorToProcessEnvironment(kAllProxy, proxyUrl);
    if (shouldWriteNoProxy) {
        mirrorToProcessEnvironment(kNoProxy, noProxy);
    }

    broadcastEnvironmentChange();
    return true;
}

bool WindowsTerminalProxyService::disable() const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    // NO_PROXY is deliberately left alone: it may be the user's own list, and this app cannot tell
    // which entries it added. Leaving a bypass list behind is harmless; deleting the user's is not.
    settings.remove(kHttpProxy);
    settings.remove(kHttpsProxy);
    settings.remove(kAllProxy);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    mirrorToProcessEnvironment(kHttpProxy, std::nullopt);
    mirrorToProcessEnvironment(kHttpsProxy, std::nullopt);
    mirrorToProcessEnvironment(kAllProxy, std::nullopt);

    broadcastEnvironmentChange();
    return true;
}

bool WindowsTerminalProxyService::isEnabledFor(const QString& proxyUrl) const
{
    if (proxyUrl.trimmed().isEmpty()) {
        return false;
    }
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    const std::optional<QString> httpValue = readValue(settings, kHttpProxy);
    if (!httpValue.has_value()) {
        return false;
    }
    return httpValue->trimmed().compare(proxyUrl.trimmed(), Qt::CaseInsensitive) == 0;
}

TerminalProxySnapshot WindowsTerminalProxyService::capture() const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    // status() is only meaningful after a read has been attempted, and value() is that read.
    const QVariant probe = settings.value(kHttpProxy);
    if (settings.status() != QSettings::NoError) {
        // No snapshot: the caller must fall back to clearing rather than restoring, because a
        // default-constructed snapshot would delete the user's variables on the way out.
        //
        // KNOWN UNREACHABLE IN TESTS: QSettings reports AccessError on an unwritable path when it
        // *writes*, not when it reads, so this branch cannot be triggered by injecting a scratch key
        // (measured: a bogus path reads back as an invalid QVariant with status NoError). The branch
        // is kept because a real registry read can fail -- a locked or permission-denied key -- and
        // returning "valid but all-absent" there would make the caller delete the user's variables.
        // The cost of keeping an untested branch is lower than the cost of getting it wrong.
        return TerminalProxySnapshot{};
    }
    Q_UNUSED(probe);

    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = readValue(settings, kHttpProxy);
    snapshot.httpsProxy = readValue(settings, kHttpsProxy);
    snapshot.allProxy = readValue(settings, kAllProxy);
    snapshot.noProxy = readValue(settings, kNoProxy);
    return snapshot;
}

bool WindowsTerminalProxyService::restore(const TerminalProxySnapshot& snapshot) const
{
    if (!snapshot.valid) {
        // Nothing was captured, so there is nothing to undo. Reporting success is deliberate: a
        // caller that never captured must not treat this as a failure.
        return true;
    }

    QSettings settings(settingsPath_, QSettings::NativeFormat);
    // Each value is either written back or deleted, decided by whether it existed at capture time.
    // Deleting for the absent case is the only faithful restore: writing an empty string instead
    // would leave behind a variable the user never had, and an empty HTTP_PROXY is read by some tools
    // as "proxy at the empty address" rather than "no proxy".
    const auto apply = [&settings](const QString& name, const std::optional<QString>& value) {
        if (value.has_value()) {
            settings.setValue(name, *value);
        } else {
            settings.remove(name);
        }
    };
    apply(kHttpProxy, snapshot.httpProxy);
    apply(kHttpsProxy, snapshot.httpsProxy);
    apply(kAllProxy, snapshot.allProxy);
    apply(kNoProxy, snapshot.noProxy);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    mirrorToProcessEnvironment(kHttpProxy, snapshot.httpProxy);
    mirrorToProcessEnvironment(kHttpsProxy, snapshot.httpsProxy);
    mirrorToProcessEnvironment(kAllProxy, snapshot.allProxy);
    mirrorToProcessEnvironment(kNoProxy, snapshot.noProxy);

    broadcastEnvironmentChange();
    return true;
}
