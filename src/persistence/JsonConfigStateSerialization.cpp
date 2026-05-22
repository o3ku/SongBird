#include "persistence/JsonConfigStateSerialization.h"

#include <QJsonArray>

#include "persistence/JsonConfigUtils.h"

namespace {

using namespace JsonConfigUtils;

QJsonObject toUiStateObject(const Config& config)
{
    QJsonObject ui;
    writeIfNotDefault(ui, QStringLiteral("mainLocationX"), config.ui().mainLocationX, 0);
    writeIfNotDefault(ui, QStringLiteral("mainLocationY"), config.ui().mainLocationY, 0);
    writeIfNotDefault(ui, QStringLiteral("mainSizeWidth"), config.ui().mainSizeWidth, 0);
    writeIfNotDefault(ui, QStringLiteral("mainSizeHeight"), config.ui().mainSizeHeight, 0);
    if (config.ui().mainServerSortColumn >= 0) {
        ui.insert(QStringLiteral("mainServerSortColumn"), config.ui().mainServerSortColumn);
        ui.insert(QStringLiteral("mainServerSortOrder"), config.ui().mainServerSortOrder);
    }
    writeIfNotEmpty(ui, QStringLiteral("mainSelectedSubscriptionId"), config.ui().mainSelectedSubId);
    writeIfNotEmpty(ui, QStringLiteral("settingsRoutingRuleTabKey"), config.ui().settingsRoutingRuleTabKey);
    writeIfNotDefault(ui, QStringLiteral("mainServerLogSplitPercent"), config.ui().mainServerLogSplitPercent, 60);
    writeIfNotDefault(ui, QStringLiteral("mainServerQrSplitPercent"), config.ui().mainServerQrSplitPercent, 78);
    writeIfTrue(ui, QStringLiteral("mainQrPreviewVisible"), config.ui().mainQrPreviewVisible);
    writeIfTrue(ui, QStringLiteral("mainProxyEnabled"), config.ui().mainProxyEnabled);

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
        writeIfNotEmpty(object, QStringLiteral("indexId"), server.indexId);
        writeIfNotEmpty(object, QStringLiteral("testResult"), server.testResult);
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
        config.ui().mainLocationX = readInt(ui, QStringLiteral("mainLocationX"), config.ui().mainLocationX);
        config.ui().mainLocationY = readInt(ui, QStringLiteral("mainLocationY"), config.ui().mainLocationY);
        config.ui().mainSizeWidth = readInt(ui, QStringLiteral("mainSizeWidth"), config.ui().mainSizeWidth);
        config.ui().mainSizeHeight = readInt(ui, QStringLiteral("mainSizeHeight"), config.ui().mainSizeHeight);
        if (ui.contains(QStringLiteral("mainServerSortColumn"))) {
            config.ui().mainServerSortColumn = readInt(ui, QStringLiteral("mainServerSortColumn"), -1);
            config.ui().mainServerSortOrder = readInt(ui, QStringLiteral("mainServerSortOrder"), 0);
            if (config.ui().mainServerSortColumn < 0) {
                config.ui().mainServerSortColumn = -1;
                config.ui().mainServerSortOrder = 0;
            }
        }
        if (ui.contains(QStringLiteral("mainSelectedSubscriptionId"))) {
            config.ui().mainSelectedSubId = readString(ui, QStringLiteral("mainSelectedSubscriptionId"));
        }
        if (ui.contains(QStringLiteral("settingsRoutingRuleTabKey"))) {
            config.ui().settingsRoutingRuleTabKey = readString(ui, QStringLiteral("settingsRoutingRuleTabKey"));
        }
        config.ui().mainServerLogSplitPercent =
            readInt(ui, QStringLiteral("mainServerLogSplitPercent"), config.ui().mainServerLogSplitPercent);
        config.ui().mainServerQrSplitPercent =
            readInt(ui, QStringLiteral("mainServerQrSplitPercent"), config.ui().mainServerQrSplitPercent);
        config.ui().mainQrPreviewVisible =
            readBool(ui, QStringLiteral("mainQrPreviewVisible"), config.ui().mainQrPreviewVisible);
        config.ui().mainProxyEnabled =
            readBool(ui, QStringLiteral("mainProxyEnabled"), config.ui().mainProxyEnabled);

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
        const QString indexId = readString(object, QStringLiteral("indexId"));
        if (indexId.trimmed().isEmpty()) {
            continue;
        }

        for (VmessItem& server : config.collection().servers) {
            if (server.indexId != indexId) {
                continue;
            }

            if (object.contains(QStringLiteral("testResult"))) {
                server.testResult = readString(object, QStringLiteral("testResult"));
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
