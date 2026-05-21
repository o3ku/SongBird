#include "platform/windows/WindowsSystemProxyService.h"

#include <QSettings>

#include <windows.h>
#include <wininet.h>

namespace {
constexpr const char* RegistryPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
}

bool WindowsSystemProxyService::update(
    SystemProxyMode mode,
    int httpPort,
    int socksPort,
    const QString& proxyExceptions,
    const QString& advancedProtocol) const
{
    if (mode == SystemProxyMode::ForcedClear) {
        setProxy(QString(), QString(), false);
        QSettings settings(QString::fromUtf8(RegistryPath), QSettings::NativeFormat);
        settings.remove(QStringLiteral("AutoConfigURL"));
        settings.sync();
        InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
        InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
        return true;
    }

    if (httpPort <= 0) {
        return false;
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

    QSettings settings(QString::fromUtf8(RegistryPath), QSettings::NativeFormat);
    settings.remove(QStringLiteral("AutoConfigURL"));
    settings.sync();

    return setProxy(proxyServer, proxyExceptions, true);
}

bool WindowsSystemProxyService::enable(int httpPort, int socksPort, const QString& proxyExceptions, const QString& advancedProtocol) const
{
    return update(SystemProxyMode::ForcedChange, httpPort, socksPort, proxyExceptions, advancedProtocol);
}

bool WindowsSystemProxyService::disable() const
{
    return update(SystemProxyMode::ForcedClear, 0, 0, QString(), QString());
}

bool WindowsSystemProxyService::isEnabled() const
{
    QSettings settings(QString::fromUtf8(RegistryPath), QSettings::NativeFormat);
    return settings.value(QStringLiteral("ProxyEnable")).toInt() == 1;
}

void WindowsSystemProxyService::resetOnShutdown() const
{
    setProxy(QString(), QString(), false);
}

bool WindowsSystemProxyService::setProxy(const QString& proxyServer, const QString& proxyExceptions, bool enabled) const
{
    QSettings settings(QString::fromUtf8(RegistryPath), QSettings::NativeFormat);
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
