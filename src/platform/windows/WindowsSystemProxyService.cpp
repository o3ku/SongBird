#include "platform/windows/WindowsSystemProxyService.h"

#include <QSettings>
#include <QStringList>

#include <optional>
#include <utility>

#include <windows.h>
#include <wininet.h>

namespace {
constexpr const char* RegistryPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

const QString kProxyEnable = QStringLiteral("ProxyEnable");
const QString kProxyServer = QStringLiteral("ProxyServer");
const QString kProxyOverride = QStringLiteral("ProxyOverride");
const QString kAutoConfigUrl = QStringLiteral("AutoConfigURL");

// QSettings reports a value it cannot find as an invalid QVariant, and a value that exists but is
// empty as a valid one holding "". Those are the two states SystemProxySnapshot keeps apart, so
// this is the only place the distinction is read out of the registry.
std::optional<QString> readCapturedValue(const QSettings& settings, const QString& name)
{
    const QVariant value = settings.value(name);
    if (!value.isValid()) {
        return std::nullopt;
    }
    return value.toString();
}
} // namespace

WindowsSystemProxyService::WindowsSystemProxyService(QString settingsPath)
    : settingsPath_(settingsPath.isEmpty() ? QString::fromUtf8(RegistryPath) : std::move(settingsPath))
{
}

bool WindowsSystemProxyService::update(
    SystemProxyMode mode,
    int httpPort,
    int socksPort,
    const QString& proxyExceptions,
    const QString& advancedProtocol) const
{
    const bool enableProxy = mode == SystemProxyMode::ForcedChange;
    // Validate before writing anything, so a rejected call cannot leave the registry half
    // changed (the PAC removal below is a write too).
    if (enableProxy && httpPort <= 0) {
        return false;
    }

    // A PAC script overrides ProxyEnable/ProxyServer, so it is cleared in both directions --
    // and before setProxy(), because setProxy() is what issues the InternetSetOption refresh.
    // Clearing it afterwards, as the disable path used to, left that refresh reading the stale
    // value and needed a second refresh to take effect.
    if (!clearAutoConfigUrl()) {
        return false;
    }

    if (!enableProxy) {
        // Propagate the registry result rather than returning true unconditionally. The
        // discarded result made a failed write look like a successful cleanup, so callers
        // dropped their managed-proxy flag while the system was still pointed at the core
        // that had just stopped -- i.e. no connectivity and no warning.
        return setProxy(QString(), QString(), false);
    }

    QString proxyServer;
    if (advancedProtocol.trimmed().isEmpty()) {
        proxyServer = QStringLiteral("127.0.0.1:%1").arg(httpPort);
    } else {
        proxyServer = advancedProtocol;
        proxyServer.replace(QStringLiteral("{ip}"), QStringLiteral("127.0.0.1"));
        proxyServer.replace(QStringLiteral("{http_port}"), QString::number(httpPort));
        proxyServer.replace(QStringLiteral("{socks_port}"), QString::number(socksPort));
    }

    return setProxy(proxyServer, proxyExceptions, true);
}

bool WindowsSystemProxyService::isEnabled() const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    return settings.value(kProxyEnable).toInt() == 1;
}

void WindowsSystemProxyService::resetOnShutdown() const
{
    setProxy(QString(), QString(), false);
}

SystemProxySnapshot WindowsSystemProxyService::capture() const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    // status() is only meaningful after a read has been attempted, and value() is that read.
    const QVariant enableValue = settings.value(kProxyEnable);
    if (settings.status() != QSettings::NoError) {
        // No snapshot: the caller must fall back to clearing rather than restoring, because a
        // default-constructed snapshot would delete the user's proxy on the way out.
        return SystemProxySnapshot{};
    }

    SystemProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.enabled = enableValue.isValid() && enableValue.toInt() == 1;
    snapshot.server = readCapturedValue(settings, kProxyServer);
    snapshot.overrideList = readCapturedValue(settings, kProxyOverride);
    snapshot.autoConfigUrl = readCapturedValue(settings, kAutoConfigUrl);
    return snapshot;
}

bool WindowsSystemProxyService::restore(const SystemProxySnapshot& snapshot) const
{
    if (!snapshot.valid) {
        // Nothing was captured, so there is nothing to undo. Reporting success is deliberate: a
        // caller that never captured (every non-Windows test, and a host that does not manage the
        // proxy at all) must not treat this as a failure.
        return true;
    }

    QSettings settings(settingsPath_, QSettings::NativeFormat);

    // PAC first. It takes precedence over ProxyEnable/ProxyServer, and the notification below
    // must observe the final state -- the same ordering update() documents above.
    if (!applyCapturedValue(kAutoConfigUrl, snapshot.autoConfigUrl)) {
        return false;
    }
    if (!applyCapturedValue(kProxyServer, snapshot.server)) {
        return false;
    }
    if (!applyCapturedValue(kProxyOverride, snapshot.overrideList)) {
        return false;
    }

    settings.setValue(kProxyEnable, snapshot.enabled ? 1 : 0);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    return notifyProxyChange();
}

bool WindowsSystemProxyService::applyCapturedValue(
    const QString& name,
    const std::optional<QString>& value) const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    if (value.has_value()) {
        settings.setValue(name, *value);
    } else {
        // The key did not exist before us. Deleting is the only faithful restore: writing an empty
        // string instead would leave behind a key the user never had, and some tools read the
        // presence of ProxyServer rather than its content.
        settings.remove(name);
    }
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool WindowsSystemProxyService::notifyProxyChange() const
{
    const BOOL changed = InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
    const BOOL refreshed = InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
    return changed != 0 && refreshed != 0;
}

bool WindowsSystemProxyService::clearAutoConfigUrl() const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    settings.remove(kAutoConfigUrl);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool WindowsSystemProxyService::setProxy(const QString& proxyServer, const QString& proxyExceptions, bool enabled) const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    settings.setValue(kProxyEnable, enabled ? 1 : 0);
    if (enabled) {
        settings.setValue(kProxyServer, proxyServer);
        settings.setValue(kProxyOverride, proxyExceptions);
    } else {
        settings.remove(kProxyServer);
        settings.remove(kProxyOverride);
    }

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    return notifyProxyChange();
}
