#include "backends/singbox/SingBoxOutboundConfigSupport.h"

#include <QJsonValue>

namespace {

const QString kPemBeginMarker = QStringLiteral("-----BEGIN CERTIFICATE-----");
const QString kPemEndMarker = QStringLiteral("-----END CERTIFICATE-----");

} // namespace

namespace SingBoxOutboundConfigSupport {

QStringList splitCsv(const QString& value)
{
    QStringList result;
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }

    return result;
}

QString resolvePrimaryPath(const QString& value)
{
    const QStringList paths = splitCsv(value);
    return paths.isEmpty() ? QStringLiteral("/") : paths.constFirst();
}

bool resolveMuxEnabled(const Config& config, const VmessItem& server)
{
    return server.muxEnabled.value_or(config.muxEnabled);
}

void insertPositiveIntOrString(QJsonObject& object, const QString& key, const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    bool ok = false;
    const int number = trimmed.toInt(&ok);
    object.insert(key, ok && number > 0 ? QJsonValue(number) : QJsonValue(trimmed));
}

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

QStringList splitPinnedCertificateHashes(const QString& value)
{
    QStringList hashes;
    const QStringList parts = value.split(QChar(','), Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        if (part.startsWith(QStringLiteral("sha256/"), Qt::CaseInsensitive)) {
            part = part.mid(QStringLiteral("sha256/").size());
        }
        if (!part.isEmpty()) {
            hashes.append(part);
        }
    }
    return hashes;
}

} // namespace SingBoxOutboundConfigSupport
