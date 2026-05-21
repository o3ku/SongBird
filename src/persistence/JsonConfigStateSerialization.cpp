#include "persistence/JsonConfigStateSerialization.h"

#include <QJsonArray>

namespace {

void insertIfNotEmpty(QJsonObject& object, const QString& key, const QString& value)
{
    if (!value.trimmed().isEmpty()) {
        object.insert(key, value);
    }
}

void insertIfTrue(QJsonObject& object, const QString& key, bool value)
{
    if (value) {
        object.insert(key, true);
    }
}

void insertIfNonZero(QJsonObject& object, const QString& key, int value)
{
    if (value != 0) {
        object.insert(key, value);
    }
}

QJsonObject toUiStateObject(const Config& config)
{
    QJsonObject ui;
    insertIfNonZero(ui, QStringLiteral("mainLocationX"), config.ui().mainLocationX);
    insertIfNonZero(ui, QStringLiteral("mainLocationY"), config.ui().mainLocationY);
    insertIfNonZero(ui, QStringLiteral("mainSizeWidth"), config.ui().mainSizeWidth);
    insertIfNonZero(ui, QStringLiteral("mainSizeHeight"), config.ui().mainSizeHeight);
    if (config.ui().mainServerSortColumn >= 0) {
        ui.insert(QStringLiteral("mainServerSortColumn"), config.ui().mainServerSortColumn);
        ui.insert(QStringLiteral("mainServerSortOrder"), config.ui().mainServerSortOrder);
    }
    insertIfNotEmpty(ui, QStringLiteral("mainSelectedSubscriptionId"), config.ui().mainSelectedSubId);
    insertIfNotEmpty(ui, QStringLiteral("settingsRoutingRuleTabKey"), config.ui().settingsRoutingRuleTabKey);
    if (config.ui().mainServerLogSplitPercent != 60) {
        ui.insert(QStringLiteral("mainServerLogSplitPercent"), config.ui().mainServerLogSplitPercent);
    }
    if (config.ui().mainServerQrSplitPercent != 78) {
        ui.insert(QStringLiteral("mainServerQrSplitPercent"), config.ui().mainServerQrSplitPercent);
    }
    insertIfTrue(ui, QStringLiteral("mainQrPreviewVisible"), config.ui().mainQrPreviewVisible);
    insertIfTrue(ui, QStringLiteral("mainProxyEnabled"), config.ui().mainProxyEnabled);

    QJsonObject mainColumnWidths;
    for (auto it = config.ui().mainColumnWidths.constBegin(); it != config.ui().mainColumnWidths.constEnd(); ++it) {
        if (it.value() > 0) {
            mainColumnWidths.insert(it.key(), it.value());
        }
    }
    if (!mainColumnWidths.isEmpty()) {
        ui.insert(QStringLiteral("mainColumnWidths"), mainColumnWidths);
    }

    return ui;
}

QJsonArray toServerStateArray(const QList<VmessItem>& servers)
{
    QJsonArray array;
    for (const VmessItem& server : servers) {
        const bool hasTestResult = !server.testResult.trimmed().isEmpty();
        if (!hasTestResult) {
            continue;
        }

        QJsonObject object;
        insertIfNotEmpty(object, QStringLiteral("indexId"), server.indexId);
        insertIfNotEmpty(object, QStringLiteral("testResult"), server.testResult);
        if (object.contains(QStringLiteral("indexId")) && object.size() > 1) {
            array.append(object);
        }
    }
    return array;
}

} // namespace

namespace JsonConfigStateSerialization {

void read(const QJsonObject& root, Config& config)
{
    const QJsonObject ui = root.value(QStringLiteral("ui")).toObject();
    if (!ui.isEmpty()) {
        config.ui().mainLocationX = ui.value(QStringLiteral("mainLocationX")).toInt(config.ui().mainLocationX);
        config.ui().mainLocationY = ui.value(QStringLiteral("mainLocationY")).toInt(config.ui().mainLocationY);
        config.ui().mainSizeWidth = ui.value(QStringLiteral("mainSizeWidth")).toInt(config.ui().mainSizeWidth);
        config.ui().mainSizeHeight = ui.value(QStringLiteral("mainSizeHeight")).toInt(config.ui().mainSizeHeight);
        if (ui.contains(QStringLiteral("mainServerSortColumn"))) {
            config.ui().mainServerSortColumn = ui.value(QStringLiteral("mainServerSortColumn")).toInt(-1);
            config.ui().mainServerSortOrder = ui.value(QStringLiteral("mainServerSortOrder")).toInt(0);
            if (config.ui().mainServerSortColumn < 0) {
                config.ui().mainServerSortColumn = -1;
                config.ui().mainServerSortOrder = 0;
            }
        }
        if (ui.contains(QStringLiteral("mainSelectedSubscriptionId"))) {
            config.ui().mainSelectedSubId = ui.value(QStringLiteral("mainSelectedSubscriptionId")).toString();
        }
        if (ui.contains(QStringLiteral("settingsRoutingRuleTabKey"))) {
            config.ui().settingsRoutingRuleTabKey = ui.value(QStringLiteral("settingsRoutingRuleTabKey")).toString();
        }
        config.ui().mainServerLogSplitPercent =
            ui.value(QStringLiteral("mainServerLogSplitPercent")).toInt(config.ui().mainServerLogSplitPercent);
        config.ui().mainServerQrSplitPercent =
            ui.value(QStringLiteral("mainServerQrSplitPercent")).toInt(config.ui().mainServerQrSplitPercent);
        config.ui().mainQrPreviewVisible =
            ui.value(QStringLiteral("mainQrPreviewVisible")).toBool(config.ui().mainQrPreviewVisible);
        config.ui().mainProxyEnabled =
            ui.value(QStringLiteral("mainProxyEnabled")).toBool(config.ui().mainProxyEnabled);

        const QJsonObject mainColumnWidths = ui.value(QStringLiteral("mainColumnWidths")).toObject();
        for (auto it = mainColumnWidths.constBegin(); it != mainColumnWidths.constEnd(); ++it) {
            if (!it.value().isDouble()) {
                continue;
            }

            const int width = it.value().toInt();
            if (width > 0) {
                config.ui().mainColumnWidths.insert(it.key(), width);
            }
        }
    }

    const QJsonArray serverStates = root.value(QStringLiteral("serverStates")).toArray();
    for (const QJsonValue& value : serverStates) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        const QString indexId = object.value(QStringLiteral("indexId")).toString();
        if (indexId.trimmed().isEmpty()) {
            continue;
        }

        for (VmessItem& server : config.collection().servers) {
            if (server.indexId != indexId) {
                continue;
            }

            if (object.contains(QStringLiteral("testResult"))) {
                server.testResult = object.value(QStringLiteral("testResult")).toString();
            }
            break;
        }
    }
}

void write(QJsonObject& root, const Config& config)
{
    root = QJsonObject();

    const QJsonObject ui = toUiStateObject(config);
    if (!ui.isEmpty()) {
        root.insert(QStringLiteral("ui"), ui);
    }

    const QJsonArray serverStates = toServerStateArray(config.collection().servers);
    if (!serverStates.isEmpty()) {
        root.insert(QStringLiteral("serverStates"), serverStates);
    }
}

} // namespace JsonConfigStateSerialization
