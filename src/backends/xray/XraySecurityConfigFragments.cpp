#include "backends/xray/XraySecurityConfigFragments.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include "runtime/ProtocolConfigMapper.h"

namespace {

const QString kPemBeginMarker = QStringLiteral("-----BEGIN CERTIFICATE-----");
const QString kPemEndMarker = QStringLiteral("-----END CERTIFICATE-----");

QStringList splitPemCertificates(const QString& value)
{
    QString text = value;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));

    QStringList certificates;
    int searchFrom = 0;
    while (true) {
        const int begin = text.indexOf(kPemBeginMarker, searchFrom);
        if (begin < 0) {
            break;
        }

        const int end = text.indexOf(kPemEndMarker, begin);
        if (end < 0) {
            break;
        }

        const int afterEnd = end + kPemEndMarker.size();
        const QString certificate = text.mid(begin, afterEnd - begin).trimmed();
        if (!certificate.isEmpty()) {
            certificates.append(certificate);
        }
        searchFrom = afterEnd;
    }

    return certificates;
}

QJsonArray buildLegacyTlsCertificates(const QStringList& certificates)
{
    QJsonArray result;
    for (const QString& certificate : certificates) {
        QJsonArray certificateLines;
        const QStringList lines = certificate.split(QChar('\n'));
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                certificateLines.append(trimmed);
            }
        }
        if (certificateLines.isEmpty()) {
            continue;
        }

        QJsonObject item;
        item.insert(QStringLiteral("certificate"), certificateLines);
        item.insert(QStringLiteral("usage"), QStringLiteral("verify"));
        result.append(item);
    }
    return result;
}

} // namespace

namespace XraySecurityConfigFragments {

void appendFinalmask(QJsonObject& streamSettings, const VmessItem& server)
{
    if (server.finalmask.trimmed().isEmpty()) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument finalmaskDocument = QJsonDocument::fromJson(server.finalmask.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && finalmaskDocument.isObject()) {
        streamSettings.insert(QStringLiteral("finalmask"), finalmaskDocument.object());
    }
}

void appendSecuritySettings(
    QJsonObject& streamSettings,
    const Config& config,
    const VmessItem& server,
    const QString& transportSecurity)
{
    if (ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        QJsonObject realitySettings;
        realitySettings.insert(QStringLiteral("serverName"), ProtocolConfigMapper::resolveServerName(server));
        realitySettings.insert(QStringLiteral("fingerprint"), ProtocolConfigMapper::resolveRealityFingerprint(config, server));
        realitySettings.insert(QStringLiteral("publicKey"), server.publicKey.trimmed());
        realitySettings.insert(QStringLiteral("shortId"), server.shortId.trimmed());
        realitySettings.insert(QStringLiteral("spiderX"), server.spiderX.trimmed());
        if (!server.mldsa65Verify.trimmed().isEmpty()) {
            realitySettings.insert(QStringLiteral("mldsa65Verify"), server.mldsa65Verify.trimmed());
        }
        streamSettings.insert(QStringLiteral("realitySettings"), realitySettings);
        return;
    }

    if (transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) != 0
        && transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) != 0) {
        return;
    }

    QJsonObject tlsSettings;
    const QString fingerprint = ProtocolConfigMapper::resolveFingerprint(config, server);
    const QStringList certificates = splitPemCertificates(server.cert);
    tlsSettings.insert(QStringLiteral("serverName"), ProtocolConfigMapper::resolveServerName(server));
    tlsSettings.insert(
        QStringLiteral("allowInsecure"),
        ProtocolConfigMapper::resolveAllowInsecure(server.allowInsecure, config.dns().defaultAllowInsecure));
    if (!fingerprint.isEmpty()) {
        tlsSettings.insert(QStringLiteral("fingerprint"), fingerprint);
    }
    if (!server.echConfigList.trimmed().isEmpty()) {
        tlsSettings.insert(QStringLiteral("echConfigList"), server.echConfigList.trimmed());
    }
    if (!server.echForceQuery.trimmed().isEmpty()) {
        tlsSettings.insert(QStringLiteral("echForceQuery"), server.echForceQuery.trimmed());
    }
    if (!certificates.isEmpty()) {
        tlsSettings.insert(QStringLiteral("certificates"), buildLegacyTlsCertificates(certificates));
        tlsSettings.insert(QStringLiteral("disableSystemRoot"), true);
        tlsSettings.insert(QStringLiteral("allowInsecure"), false);
    } else if (!server.certSha.trimmed().isEmpty()) {
        tlsSettings.insert(QStringLiteral("pinnedPeerCertSha256"), server.certSha.trimmed());
        tlsSettings.insert(QStringLiteral("allowInsecure"), false);
    }

    if (!server.alpn.isEmpty()) {
        QJsonArray alpnArray;
        for (const QString& item : server.alpn) {
            alpnArray.append(item);
        }
        tlsSettings.insert(QStringLiteral("alpn"), alpnArray);
    }

    if (transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0) {
        streamSettings.insert(QStringLiteral("xtlsSettings"), tlsSettings);
    } else {
        streamSettings.insert(QStringLiteral("tlsSettings"), tlsSettings);
    }
}

} // namespace XraySecurityConfigFragments
