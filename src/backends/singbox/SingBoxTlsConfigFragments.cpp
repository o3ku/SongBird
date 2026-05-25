#include "backends/singbox/SingBoxTlsConfigFragments.h"

#include <QJsonArray>

#include "backends/singbox/SingBoxOutboundConfigSupport.h"
#include "runtime/ProtocolConfigMapper.h"

namespace OutboundSupport = SingBoxOutboundConfigSupport;

namespace SingBoxTlsConfigFragments {

QJsonObject buildTls(const Config& config, const VmessItem& server)
{
    const QString transportSecurity = server.streamSecurity.trimmed();
    if (transportSecurity.isEmpty()
        || (transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) != 0
            && transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) != 0
            && !ProtocolConfigMapper::isRealityTransport(transportSecurity))) {
        return {};
    }

    QJsonObject tls;
    tls.insert(QStringLiteral("enabled"), true);
    tls.insert(
        QStringLiteral("insecure"),
        ProtocolConfigMapper::resolveAllowInsecure(server.allowInsecure, config.dns().defaultAllowInsecure));

    const QString serverName = ProtocolConfigMapper::resolveServerName(server).trimmed();
    if (!serverName.isEmpty()) {
        tls.insert(QStringLiteral("server_name"), serverName);
    }

    if (!server.alpn.isEmpty()) {
        QJsonArray alpnArray;
        for (const QString& item : server.alpn) {
            if (!item.trimmed().isEmpty()) {
                alpnArray.append(item.trimmed());
            }
        }
        if (!alpnArray.isEmpty()) {
            tls.insert(QStringLiteral("alpn"), alpnArray);
        }
    }

    const QStringList certificates = OutboundSupport::splitPemCertificates(server.cert);
    if (!certificates.isEmpty() && !ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        QJsonArray certificateArray;
        for (const QString& certificate : certificates) {
            certificateArray.append(certificate);
        }
        tls.insert(QStringLiteral("certificate"), certificateArray);
        tls.insert(QStringLiteral("insecure"), false);
    } else if (!ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        const QStringList pinnedHashes = OutboundSupport::splitPinnedCertificateHashes(server.certSha);
        if (!pinnedHashes.isEmpty()) {
            QJsonArray pinnedHashArray;
            for (const QString& hash : pinnedHashes) {
                pinnedHashArray.append(hash);
            }
            tls.insert(QStringLiteral("certificate_public_key_sha256"), pinnedHashArray);
            tls.insert(QStringLiteral("insecure"), false);
        }
    }

    if (ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        QJsonObject utls;
        utls.insert(QStringLiteral("enabled"), true);
        utls.insert(QStringLiteral("fingerprint"), ProtocolConfigMapper::resolveRealityFingerprint(config, server));
        tls.insert(QStringLiteral("utls"), utls);

        QJsonObject reality;
        reality.insert(QStringLiteral("enabled"), true);
        reality.insert(QStringLiteral("public_key"), server.publicKey.trimmed());
        reality.insert(QStringLiteral("short_id"), server.shortId.trimmed());
        tls.insert(QStringLiteral("reality"), reality);
        tls.insert(QStringLiteral("insecure"), false);
    } else {
        const QString fingerprint = ProtocolConfigMapper::resolveFingerprint(config, server);
        if (!fingerprint.isEmpty()) {
            QJsonObject utls;
            utls.insert(QStringLiteral("enabled"), true);
            utls.insert(QStringLiteral("fingerprint"), fingerprint);
            tls.insert(QStringLiteral("utls"), utls);
        }
    }

    return tls;
}

} // namespace SingBoxTlsConfigFragments
