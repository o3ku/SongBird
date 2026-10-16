#include "runtime/XrayTlsCompatibility.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>
#include <QVersionNumber>

namespace {

QVersionNumber parseVersion(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QChar('v'), Qt::CaseInsensitive)) {
        version.remove(0, 1);
    }
    static const QRegularExpression pattern(QStringLiteral(R"(^\d+\.\d+\.\d+(?:[-+].*)?$)"));
    return pattern.match(version).hasMatch() ? QVersionNumber::fromString(version) : QVersionNumber();
}

OperationResult unknownVersion()
{
    return OperationResult::fail(QCoreApplication::translate(
        "XrayTlsCompatibility",
        "Cannot determine the Xray version for TLS compatibility. Check the Xray executable or use another core."));
}

OperationResult normalizePins(const QString& value, QStringList& pins)
{
    static const QRegularExpression separators(QStringLiteral("[,~]"));
    static const QRegularExpression hashPattern(QStringLiteral("^[0-9a-fA-F]{64}$"));
    const QStringList values = value.split(separators, Qt::KeepEmptyParts);
    for (QString hash : values) {
        hash = hash.trimmed();
        hash.remove(QChar(':'));
        if (!hashPattern.match(hash).hasMatch()) {
            return OperationResult::fail(QCoreApplication::translate(
                "XrayTlsCompatibility",
                "Each certificate SHA-256 pin must contain 64 hexadecimal digits. Separate multiple pins with commas."));
        }
        pins.append(hash.toLower());
    }
    pins.removeDuplicates();
    return OperationResult::ok();
}

} // namespace

namespace XrayTlsCompatibility {

bool supportsAllowInsecure(const QString& version, const QDateTime& nowUtc)
{
    const QVersionNumber parsed = parseVersion(version);
    if (parsed.isNull()) {
        return false;
    }
    if (parsed < QVersionNumber(26, 1, 31)) {
        return true;
    }
    // 26.1.31、26.2.2 已删除此能力；26.2.4 恢复宽限期，26.6.22 再次完全删除。
    if (parsed < QVersionNumber(26, 2, 4) || parsed >= QVersionNumber(26, 6, 22)) {
        return false;
    }
    const QDateTime deadline(QDate(2026, 6, 1), QTime(0, 0), Qt::UTC);
    return nowUtc.isValid() && nowUtc.toUTC() <= deadline;
}

OperationResult adaptConfig(QJsonObject& root, const QString& version, const QDateTime& nowUtc)
{
    const QVersionNumber parsed = parseVersion(version);
    QJsonArray outbounds = root.value(QStringLiteral("outbounds")).toArray();
    for (int i = 0; i < outbounds.size(); ++i) {
        QJsonObject outbound = outbounds.at(i).toObject();
        QJsonObject stream = outbound.value(QStringLiteral("streamSettings")).toObject();
        const QString security = stream.value(QStringLiteral("security")).toString().trimmed().toLower();
        if (security != QStringLiteral("tls") && security != QStringLiteral("xtls")) {
            continue;
        }
        const QString settingsKey = security == QStringLiteral("xtls")
            ? QStringLiteral("xtlsSettings") : QStringLiteral("tlsSettings");
        QJsonObject tls = stream.value(settingsKey).toObject();
        const QString pinValue = tls.value(QStringLiteral("pinnedPeerCertSha256")).toString().trimmed();
        if (!pinValue.isEmpty()) {
            if (parsed.isNull()) {
                return unknownVersion();
            }
            if (parsed < QVersionNumber(26, 1, 13)) {
                // 旧版的证书链散列并非叶证书散列，不能直接重命名字段来冒充兼容。
                return OperationResult::fail(QCoreApplication::translate(
                    "XrayTlsCompatibility",
                    "Xray %1 does not support certificate SHA-256 pins. Use Xray 26.1.13 or later, or configure a trusted certificate.").arg(version));
            }
            QStringList pins;
            const OperationResult normalized = normalizePins(pinValue, pins);
            if (!normalized.success) {
                return normalized;
            }
            tls.insert(QStringLiteral("pinnedPeerCertSha256"), pins.join(
                parsed < QVersionNumber(26, 1, 31) ? QChar('~') : QChar(',')));
            tls.insert(QStringLiteral("allowInsecure"), false);
        } else if (tls.value(QStringLiteral("allowInsecure")).toBool()) {
            if (parsed.isNull()) {
                return unknownVersion();
            }
            if (!supportsAllowInsecure(version, nowUtc)) {
                return OperationResult::fail(QCoreApplication::translate(
                    "XrayTlsCompatibility",
                    "Xray %1 does not support skipping TLS certificate verification. Configure a trusted certificate or SHA-256 pin, or use sing-box or Mihomo. Explicit node settings override the default in General settings.").arg(version));
            }
        }
        stream.insert(settingsKey, tls);
        outbound.insert(QStringLiteral("streamSettings"), stream);
        outbounds.replace(i, outbound);
    }
    // 全部检查通过后再提交结果，失败时不留下半更新的配置。
    root.insert(QStringLiteral("outbounds"), outbounds);
    return OperationResult::ok();
}

} // namespace XrayTlsCompatibility
