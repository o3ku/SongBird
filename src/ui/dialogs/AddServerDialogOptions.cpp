#include "ui/dialogs/AddServerDialogOptions.h"

#include <QCoreApplication>

namespace {

QString trDialog(const char* text)
{
    return QCoreApplication::translate("AddServerDialog", text);
}

} // namespace

QList<AddServerDialogOptions::ComboOption> AddServerDialogOptions::protocolTypeOptions()
{
    return {
        { trDialog("VMess"), static_cast<int>(ConfigType::VMess) },
        { trDialog("VLESS"), static_cast<int>(ConfigType::VLESS) },
        { trDialog("Trojan"), static_cast<int>(ConfigType::Trojan) },
        { trDialog("Shadowsocks"), static_cast<int>(ConfigType::Shadowsocks) },
        { trDialog("Socks"), static_cast<int>(ConfigType::Socks) },
        { trDialog("HTTP"), static_cast<int>(ConfigType::HTTP) },
        { trDialog("Hysteria2"), static_cast<int>(ConfigType::Hysteria2) },
        { trDialog("TUIC"), static_cast<int>(ConfigType::TUIC) },
        { trDialog("WireGuard"), static_cast<int>(ConfigType::WireGuard) },
        { trDialog("AnyTLS"), static_cast<int>(ConfigType::AnyTLS) },
        { trDialog("Naive"), static_cast<int>(ConfigType::Naive) }
    };
}

AddServerDialogOptions::ProtocolFieldState AddServerDialogOptions::protocolFieldState(ConfigType type)
{
    ProtocolFieldState state;
    state.credentialLabelText = trDialog("Credential");
    state.securityLabelText = trDialog("Security");

    switch (type) {
    case ConfigType::VMess:
        state.credentialLabelText = trDialog("UUID");
        state.securityLabelText = trDialog("Encryption");
        state.defaultSecurity = QStringLiteral("auto");
        state.showAlterId = true;
        break;
    case ConfigType::VLESS:
        state.credentialLabelText = trDialog("UUID");
        state.securityLabelText = trDialog("Encryption");
        state.defaultSecurity = QStringLiteral("none");
        state.showFlow = true;
        break;
    case ConfigType::Trojan:
        state.credentialLabelText = trDialog("Password");
        state.showFlow = true;
        break;
    case ConfigType::Shadowsocks:
        state.credentialLabelText = trDialog("Password");
        state.securityLabelText = trDialog("Method");
        state.defaultSecurity = QStringLiteral("aes-128-gcm");
        break;
    case ConfigType::Socks:
    case ConfigType::HTTP:
        state.credentialLabelText = trDialog("Username");
        state.securityLabelText = trDialog("Password");
        break;
    case ConfigType::Hysteria2:
        state.credentialLabelText = trDialog("Password");
        break;
    case ConfigType::TUIC:
        state.credentialLabelText = trDialog("UUID");
        state.securityLabelText = trDialog("Password");
        break;
    case ConfigType::WireGuard:
        state.credentialLabelText = trDialog("Private Key");
        break;
    case ConfigType::AnyTLS:
        state.credentialLabelText = trDialog("Password");
        break;
    case ConfigType::Naive:
        state.credentialLabelText = trDialog("Username");
        break;
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        break;
    }

    return state;
}

AddServerDialogOptions::TransportFieldState AddServerDialogOptions::transportFieldState(
    const QString& network,
    const QString& transportSecurity)
{
    const QString normalizedSecurity = transportSecurity.trimmed();
    const QString normalizedNetwork = network.trimmed().toLower();

    TransportFieldState state;
    state.headerLabelText = trDialog("Header Type");
    state.hostLabelText = trDialog("Host");
    state.pathLabelText = trDialog("Path");
    state.hostPlaceholder = trDialog("host.example.com");
    state.pathPlaceholder = trDialog("/");
    state.secureTransport = normalizedSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0
        || normalizedSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0
        || normalizedSecurity.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0;
    state.tlsTransport = normalizedSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0
        || normalizedSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0;
    state.realityTransport = normalizedSecurity.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0;
    state.httpupgradeTransport = normalizedNetwork == QStringLiteral("httpupgrade");
    state.xhttpTransport = normalizedNetwork == QStringLiteral("xhttp");

    if (normalizedNetwork == QStringLiteral("grpc")) {
        state.headerLabelText = trDialog("Mode");
        state.pathLabelText = trDialog("Service Name");
        state.pathPlaceholder = trDialog("service-name");
        state.showHost = false;
    } else if (state.xhttpTransport) {
        state.headerLabelText = trDialog("Mode");
    } else if (normalizedNetwork == QStringLiteral("kcp")) {
        state.pathLabelText = trDialog("Seed");
        state.pathPlaceholder = trDialog("mkcp-seed");
        state.showHost = false;
    } else if (normalizedNetwork == QStringLiteral("quic")) {
        state.hostLabelText = trDialog("QUIC Security");
        state.pathLabelText = trDialog("Key");
        state.hostPlaceholder = trDialog("none");
        state.pathPlaceholder = trDialog("quic-key");
    }

    return state;
}

QStringList AddServerDialogOptions::securityOptions(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
        return {
            QStringLiteral("auto"),
            QStringLiteral("aes-128-gcm"),
            QStringLiteral("chacha20-poly1305"),
            QStringLiteral("none")
        };
    case ConfigType::VLESS:
        return {
            QStringLiteral("none")
        };
    case ConfigType::Shadowsocks:
        return {
            QStringLiteral("aes-256-gcm"),
            QStringLiteral("aes-128-gcm"),
            QStringLiteral("chacha20-poly1305"),
            QStringLiteral("chacha20-ietf-poly1305"),
            QStringLiteral("xchacha20-poly1305"),
            QStringLiteral("2022-blake3-aes-128-gcm"),
            QStringLiteral("2022-blake3-aes-256-gcm")
        };
    case ConfigType::Trojan:
    case ConfigType::Socks:
    case ConfigType::HTTP:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::WireGuard:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return {};
    }
}

QStringList AddServerDialogOptions::networkOptions()
{
    return {
        QStringLiteral("tcp"),
        QStringLiteral("kcp"),
        QStringLiteral("quic"),
        QStringLiteral("ws"),
        QStringLiteral("httpupgrade"),
        QStringLiteral("xhttp"),
        QStringLiteral("grpc"),
        QStringLiteral("h2")
    };
}

QStringList AddServerDialogOptions::headerTypeOptions()
{
    return {
        QStringLiteral("none"),
        QStringLiteral("http"),
        QStringLiteral("srtp"),
        QStringLiteral("utp"),
        QStringLiteral("wechat-video"),
        QStringLiteral("dtls"),
        QStringLiteral("wireguard"),
        QStringLiteral("gun"),
        QStringLiteral("multi")
    };
}

QStringList AddServerDialogOptions::streamSecurityOptions()
{
    return {
        QString(),
        QStringLiteral("tls"),
        QStringLiteral("xtls"),
        QStringLiteral("reality")
    };
}

QStringList AddServerDialogOptions::echForceQueryOptions()
{
    return {
        QString(),
        QStringLiteral("none"),
        QStringLiteral("half"),
        QStringLiteral("full")
    };
}

QStringList AddServerDialogOptions::userAgentOptions()
{
    return {
        QString(),
        QStringLiteral("chrome"),
        QStringLiteral("firefox"),
        QStringLiteral("edge"),
        QStringLiteral("golang")
    };
}

QStringList AddServerDialogOptions::allowInsecureOptions()
{
    return {
        QString(),
        QStringLiteral("false"),
        QStringLiteral("true")
    };
}

QStringList AddServerDialogOptions::congestionControlOptions()
{
    return {
        QStringLiteral("cubic"),
        QStringLiteral("bbr"),
        QStringLiteral("new_reno")
    };
}

QStringList AddServerDialogOptions::udpRelayModeOptions()
{
    return {
        QStringLiteral("native"),
        QStringLiteral("quic")
    };
}
