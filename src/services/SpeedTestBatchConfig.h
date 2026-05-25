#pragma once

#include <optional>
#include <utility>

#include <QJsonObject>
#include <QList>
#include <QString>

namespace SpeedTestBatchConfig {

struct ProbeEntry {
    QString indexId;
    QString serverName;
    int socksPort = 0;
    QString outboundTag;
    QString inboundTag;
    QJsonObject outbound;
    QString url;
};

enum class BatchFlavor {
    Legacy,
    SingBox,
};

std::optional<std::pair<BatchFlavor, QJsonObject>> extractPrimaryOutbound(const QJsonObject& root);
QJsonObject assembleLegacyConfig(const QList<ProbeEntry>& entries);
QJsonObject assembleSingBoxConfig(const QList<ProbeEntry>& entries);

} // namespace SpeedTestBatchConfig
