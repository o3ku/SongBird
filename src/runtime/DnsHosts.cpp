#include "runtime/DnsHosts.h"

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QRegularExpression>

namespace {

const QMap<QString, QStringList> kPredefinedHosts{
    {QStringLiteral("dns.google"), QStringList{
                                       QStringLiteral("8.8.8.8"),
                                       QStringLiteral("8.8.4.4"),
                                       QStringLiteral("2001:4860:4860::8888"),
                                       QStringLiteral("2001:4860:4860::8844")}},
    {QStringLiteral("dns.alidns.com"), QStringList{
                                           QStringLiteral("223.5.5.5"),
                                           QStringLiteral("223.6.6.6"),
                                           QStringLiteral("2400:3200::1"),
                                           QStringLiteral("2400:3200:baba::1")}},
    {QStringLiteral("one.one.one.one"), QStringList{
                                            QStringLiteral("1.1.1.1"),
                                            QStringLiteral("1.0.0.1"),
                                            QStringLiteral("2606:4700:4700::1111"),
                                            QStringLiteral("2606:4700:4700::1001")}},
    {QStringLiteral("1dot1dot1dot1.cloudflare-dns.com"), QStringList{
                                                              QStringLiteral("1.1.1.1"),
                                                              QStringLiteral("1.0.0.1"),
                                                              QStringLiteral("2606:4700:4700::1111"),
                                                              QStringLiteral("2606:4700:4700::1001")}},
    {QStringLiteral("cloudflare-dns.com"), QStringList{
                                               QStringLiteral("104.16.249.249"),
                                               QStringLiteral("104.16.248.249"),
                                               QStringLiteral("2606:4700::6810:f8f9"),
                                               QStringLiteral("2606:4700::6810:f9f9")}},
    {QStringLiteral("dns.cloudflare.com"), QStringList{
                                               QStringLiteral("104.16.132.229"),
                                               QStringLiteral("104.16.133.229"),
                                               QStringLiteral("2606:4700::6810:84e5"),
                                               QStringLiteral("2606:4700::6810:85e5")}},
    {QStringLiteral("dot.pub"), QStringList{
                                    QStringLiteral("1.12.12.12"),
                                    QStringLiteral("120.53.53.53")}},
    {QStringLiteral("doh.pub"), QStringList{
                                    QStringLiteral("1.12.12.12"),
                                    QStringLiteral("120.53.53.53")}},
    {QStringLiteral("dns.quad9.net"), QStringList{
                                          QStringLiteral("9.9.9.9"),
                                          QStringLiteral("149.112.112.112"),
                                          QStringLiteral("2620:fe::fe"),
                                          QStringLiteral("2620:fe::9")}},
    {QStringLiteral("dns.yandex.net"), QStringList{
                                           QStringLiteral("77.88.8.8"),
                                           QStringLiteral("77.88.8.1"),
                                           QStringLiteral("2a02:6b8::feed:0ff"),
                                           QStringLiteral("2a02:6b8:0:1::feed:0ff")}},
    {QStringLiteral("dns.sb"), QStringList{
                                   QStringLiteral("185.222.222.222"),
                                   QStringLiteral("2a09::")}},
    {QStringLiteral("dns.umbrella.com"), QStringList{
                                             QStringLiteral("208.67.220.220"),
                                             QStringLiteral("208.67.222.222"),
                                             QStringLiteral("2620:119:35::35"),
                                             QStringLiteral("2620:119:53::53")}},
    {QStringLiteral("dns.sse.cisco.com"), QStringList{
                                              QStringLiteral("208.67.220.220"),
                                              QStringLiteral("208.67.222.222"),
                                              QStringLiteral("2620:119:35::35"),
                                              QStringLiteral("2620:119:53::53")}},
    {QStringLiteral("engage.cloudflareclient.com"), QStringList{
                                                        QStringLiteral("162.159.192.1")}},
};

QMap<QString, QString> parseSystemHostsText(const QString& hosts)
{
    QMap<QString, QString> result;
    const QString normalized = QString(hosts).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QChar('\r'), QChar('\n'));
    const QStringList lines = normalized.split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QChar('#'))) {
            continue;
        }

        const int commentIndex = line.indexOf(QChar('#'));
        if (commentIndex >= 0) {
            line = line.left(commentIndex).trimmed();
        }
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(QRegularExpression(QStringLiteral("[ \\t]+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            continue;
        }

        const QString ipAddress = parts.constFirst().trimmed();
        if (!DnsHosts::isIpAddress(ipAddress)) {
            continue;
        }

        const QString domain = parts.at(1).trimmed();
        if (domain.isEmpty() || domain.size() > 255) {
            continue;
        }

        result.insert(domain, ipAddress);
    }

    return result;
}

void mergeSystemHostsFromFile(const QString& path, QMap<QString, QString>& hosts)
{
    if (path.trimmed().isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QByteArray content = file.readAll();
    const auto readError = file.error();
    file.close();
    if (content.isEmpty() && readError != QFileDevice::NoError) {
        return;
    }

    const QMap<QString, QString> parsedHosts = parseSystemHostsText(QString::fromUtf8(content));
    for (auto it = parsedHosts.constBegin(); it != parsedHosts.constEnd(); ++it) {
        hosts.insert(it.key(), it.value());
    }
}

} // namespace

namespace DnsHosts {

const QMap<QString, QStringList>& predefined()
{
    return kPredefinedHosts;
}

bool isIpAddress(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QStringList octets = trimmed.split(QChar('.'));
    if (octets.size() == 4) {
        for (const QString& octet : octets) {
            bool ok = false;
            const int segment = octet.toInt(&ok);
            if (!ok || segment < 0 || segment > 255) {
                return false;
            }
        }
        return true;
    }

    return trimmed.contains(QChar(':'));
}

bool isDomainName(const QString& value)
{
    const QString trimmed = value.trimmed();
    return !trimmed.isEmpty()
        && !isIpAddress(trimmed)
        && trimmed.contains(QChar('.'))
        && !trimmed.contains(QChar(' '));
}

QMap<QString, QStringList> parseConfigured(const QString& hosts)
{
    QMap<QString, QStringList> result;
    const QString normalized = QString(hosts).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QChar('\r'), QChar('\n'));
    const QStringList lines = normalized.split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QChar('#'))) {
            continue;
        }

        const int commentIndex = line.indexOf(QChar('#'));
        if (commentIndex >= 0) {
            line = line.left(commentIndex).trimmed();
        }
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(QRegularExpression(QStringLiteral("[ \\t]+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            continue;
        }

        const QString key = parts.constFirst();
        QStringList values = result.value(key);
        for (int index = 1; index < parts.size(); ++index) {
            values.append(parts.at(index));
        }
        result.insert(key, values);
    }

    return result;
}

QMap<QString, QString> loadSystem()
{
    QMap<QString, QString> hosts;

    const QString overridePath = qEnvironmentVariable("SONGBIRD_SYSTEM_HOSTS_PATH").trimmed();
    if (!overridePath.isEmpty()) {
        mergeSystemHostsFromFile(overridePath, hosts);
        return hosts;
    }

#ifdef Q_OS_WIN
    const QString systemRoot = qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"));
    const QString hostsDirectory = QDir::fromNativeSeparators(systemRoot) + QStringLiteral("/System32/drivers/etc/");
    mergeSystemHostsFromFile(hostsDirectory + QStringLiteral("hosts"), hosts);
    mergeSystemHostsFromFile(hostsDirectory + QStringLiteral("hosts.ics"), hosts);
#else
    mergeSystemHostsFromFile(QStringLiteral("/etc/hosts"), hosts);
#endif

    return hosts;
}

} // namespace DnsHosts
