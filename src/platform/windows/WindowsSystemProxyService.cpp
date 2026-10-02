#include "platform/windows/WindowsSystemProxyService.h"

#include <QSettings>

#include <utility>

#include <windows.h>
#include <wininet.h>

namespace {
constexpr const char* RegistryPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
}

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
    return settings.value(QStringLiteral("ProxyEnable")).toInt() == 1;
}

void WindowsSystemProxyService::resetOnShutdown() const
{
    setProxy(QString(), QString(), false);
}

bool WindowsSystemProxyService::clearAutoConfigUrl() const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    settings.remove(QStringLiteral("AutoConfigURL"));
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool WindowsSystemProxyService::setProxy(const QString& proxyServer, const QString& proxyExceptions, bool enabled) const
{
    QSettings settings(settingsPath_, QSettings::NativeFormat);
    settings.setValue(QStringLiteral("ProxyEnable"), enabled ? 1 : 0);
    if (enabled) {
        settings.setValue(QStringLiteral("ProxyServer"), proxyServer);
        settings.setValue(QStringLiteral("ProxyOverride"), proxyExceptions);
    } else {
        settings.remove(QStringLiteral("ProxyServer"));
        settings.remove(QStringLiteral("ProxyOverride"));
    }

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        return false;
    }

    InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
    InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
    return true;
}
