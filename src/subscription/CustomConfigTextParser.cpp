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
    } else {
        return {};
    }

    if (ok != nullptr) {
        *ok = true;
    }

    return item;
}
