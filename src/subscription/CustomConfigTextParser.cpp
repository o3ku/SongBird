#include "subscription/CustomConfigTextParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

VmessItem CustomConfigTextParser::parse(const QString& text, QString* fileExtension, bool* ok)
{
    if (fileExtension != nullptr) {
        *fileExtension = QStringLiteral("json");
    }
    if (ok != nullptr) {
        *ok = false;
    }

    const QString normalized = text.trimmed();
    const QJsonDocument document = QJsonDocument::fromJson(normalized.toUtf8());
    if (!document.isObject()) {
        VmessItem item;
        item.configType = ConfigType::Custom;

        if (normalized.contains(QStringLiteral("port"), Qt::CaseInsensitive)
            && normalized.contains(QStringLiteral("socks-port"), Qt::CaseInsensitive)
            && normalized.contains(QStringLiteral("proxies"), Qt::CaseInsensitive)) {
            item.coreType = CoreType::Clash;
            item.remarks = QStringLiteral("clash_custom");
            if (fileExtension != nullptr) {
                *fileExtension = QStringLiteral("yaml");
            }
            if (ok != nullptr) {
                *ok = true;
            }
            return item;
        }

        return {};
    }

    const QJsonObject root = document.object();
    const QJsonArray inbounds = root.value(QStringLiteral("inbounds")).toArray();
    const QJsonArray outbounds = root.value(QStringLiteral("outbounds")).toArray();
    VmessItem item;
    item.configType = ConfigType::Custom;

    if (!inbounds.isEmpty() && !outbounds.isEmpty() && outbounds.at(0).isObject()
        && outbounds.at(0).toObject().contains(QStringLiteral("protocol"))) {
        item.coreType = CoreType::Xray;
        item.remarks = QStringLiteral("xray_custom");
    } else if (!inbounds.isEmpty() && !outbounds.isEmpty() && outbounds.at(0).isObject()
        && outbounds.at(0).toObject().contains(QStringLiteral("type"))) {
        item.coreType = CoreType::SingBox;
        item.remarks = QStringLiteral("singbox_custom");
    } else if (normalized.contains(QStringLiteral("server"), Qt::CaseInsensitive)
        && normalized.contains(QStringLiteral("up"), Qt::CaseInsensitive)
        && normalized.contains(QStringLiteral("down"), Qt::CaseInsensitive)
        && normalized.contains(QStringLiteral("listen"), Qt::CaseInsensitive)
        && !normalized.contains(QStringLiteral("<html>"), Qt::CaseInsensitive)
        && !normalized.contains(QStringLiteral("<body>"), Qt::CaseInsensitive)) {
        item.coreType = CoreType::Hysteria;
        item.remarks = QStringLiteral("hysteria_custom");
    } else if (normalized.contains(QStringLiteral("listen"), Qt::CaseInsensitive)
        && normalized.contains(QStringLiteral("proxy"), Qt::CaseInsensitive)
        && !normalized.contains(QStringLiteral("<html>"), Qt::CaseInsensitive)
        && !normalized.contains(QStringLiteral("<body>"), Qt::CaseInsensitive)) {
        item.coreType = CoreType::NaiveProxy;
        item.remarks = QStringLiteral("naiveproxy_custom");
    } else {
        return {};
    }

    if (ok != nullptr) {
        *ok = true;
    }

    return item;
}
