#include "persistence/JsonConfigUiSerialization.h"

namespace {

int normalizeSplitPercent(int value, int fallback)
{
    return value < 10 || value > 90 ? fallback : value;
}

} // namespace

namespace JsonConfigUiSerialization {

void read(const QJsonObject& root, UiConfigState& config)
{
    const QJsonObject ui = root.value(QStringLiteral("ui")).toObject();
    config.ui().showMainOnStartup = ui.value(QStringLiteral("showMainOnStartup")).toBool(true);
    config.ui().autoRunEnabled = ui.value(QStringLiteral("autoRunEnabled")).toBool(false);
    config.ui().languageCode = ui.value(QStringLiteral("languageCode")).toString();
    const QString themeName = ui.value(QStringLiteral("themeName")).toString(config.ui().themeName).trimmed();
    config.ui().themeName =
        themeName.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("Dark")
            : QStringLiteral("Light");
    config.ui().mainLocationX = ui.value(QStringLiteral("mainLocationX")).toInt(0);
    config.ui().mainLocationY = ui.value(QStringLiteral("mainLocationY")).toInt(0);
    config.ui().mainSizeWidth = ui.value(QStringLiteral("mainSizeWidth")).toInt(0);
    config.ui().mainSizeHeight = ui.value(QStringLiteral("mainSizeHeight")).toInt(0);
    config.ui().mainServerSortColumn = ui.value(QStringLiteral("mainServerSortColumn")).toInt(-1);
    config.ui().mainServerSortOrder = ui.value(QStringLiteral("mainServerSortOrder")).toInt(0);
    if (config.ui().mainServerSortColumn < 0) {
        config.ui().mainServerSortColumn = -1;
        config.ui().mainServerSortOrder = 0;
    }
    config.ui().mainSelectedSubId = ui.value(QStringLiteral("mainSelectedSubscriptionId")).toString();
    config.ui().settingsRoutingRuleTabKey = ui.value(QStringLiteral("settingsRoutingRuleTabKey")).toString();
    config.ui().mainServerLogSplitPercent =
        normalizeSplitPercent(ui.value(QStringLiteral("mainServerLogSplitPercent")).toInt(60), 60);
    config.ui().mainServerQrSplitPercent =
        normalizeSplitPercent(ui.value(QStringLiteral("mainServerQrSplitPercent")).toInt(78), 78);
    config.ui().mainQrPreviewVisible = ui.value(QStringLiteral("mainQrPreviewVisible")).toBool(false);
    config.ui().mainProxyEnabled = ui.value(QStringLiteral("mainProxyEnabled")).toBool(false);

    const QJsonObject mainColumnWidths = ui.value(QStringLiteral("mainColumnWidths")).toObject();
    for (auto it = mainColumnWidths.constBegin(); it != mainColumnWidths.constEnd(); ++it) {
        if (!it.value().isDouble()) {
            continue;
        }

        const int width = it.value().toInt(0);
        if (width > 0) {
            config.ui().mainColumnWidths.insert(it.key(), width);
        }
    }
}

void write(QJsonObject& root, const UiConfigState& config)
{
    QJsonObject ui;
    if (!config.ui().showMainOnStartup) {
        ui.insert(QStringLiteral("showMainOnStartup"), false);
    }
    if (config.ui().autoRunEnabled) {
        ui.insert(QStringLiteral("autoRunEnabled"), true);
    }
    if (!config.ui().languageCode.trimmed().isEmpty()) {
        ui.insert(QStringLiteral("languageCode"), config.ui().languageCode);
    }
    const QString themeName =
        config.ui().themeName.trimmed().compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("Dark")
            : QStringLiteral("Light");
    if (themeName != QStringLiteral("Light")) {
        ui.insert(QStringLiteral("themeName"), themeName);
    }
    if (!ui.isEmpty()) {
        root.insert(QStringLiteral("ui"), ui);
    }
}

} // namespace JsonConfigUiSerialization
