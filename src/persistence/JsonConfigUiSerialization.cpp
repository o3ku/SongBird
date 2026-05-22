#include "persistence/JsonConfigUiSerialization.h"

#include "persistence/JsonConfigUtils.h"

namespace {

using namespace JsonConfigUtils;

int normalizeSplitPercent(int value, int fallback)
{
    return value < 10 || value > 90 ? fallback : value;
}

} // namespace

namespace JsonConfigUiSerialization {

void read(const QJsonObject& root, UiConfigState& config)
{
    const QJsonObject ui = readObject(root, QStringLiteral("ui"));
    config.ui().showMainOnStartup = readBool(ui, QStringLiteral("showMainOnStartup"), true);
    config.ui().autoRunEnabled = readBool(ui, QStringLiteral("autoRunEnabled"), false);
    config.ui().languageCode = readString(ui, QStringLiteral("languageCode"));
    const QString themeName = readString(ui, QStringLiteral("themeName"), config.ui().themeName).trimmed();
    config.ui().themeName =
        themeName.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("Dark")
            : QStringLiteral("Light");
    config.ui().mainLocationX = readInt(ui, QStringLiteral("mainLocationX"), 0);
    config.ui().mainLocationY = readInt(ui, QStringLiteral("mainLocationY"), 0);
    config.ui().mainSizeWidth = readInt(ui, QStringLiteral("mainSizeWidth"), 0);
    config.ui().mainSizeHeight = readInt(ui, QStringLiteral("mainSizeHeight"), 0);
    config.ui().mainServerSortColumn = readInt(ui, QStringLiteral("mainServerSortColumn"), -1);
    config.ui().mainServerSortOrder = readInt(ui, QStringLiteral("mainServerSortOrder"), 0);
    if (config.ui().mainServerSortColumn < 0) {
        config.ui().mainServerSortColumn = -1;
        config.ui().mainServerSortOrder = 0;
    }
    config.ui().mainSelectedSubId = readString(ui, QStringLiteral("mainSelectedSubscriptionId"));
    config.ui().settingsRoutingRuleTabKey = readString(ui, QStringLiteral("settingsRoutingRuleTabKey"));
    config.ui().mainServerLogSplitPercent =
        normalizeSplitPercent(readInt(ui, QStringLiteral("mainServerLogSplitPercent"), 60), 60);
    config.ui().mainServerQrSplitPercent =
        normalizeSplitPercent(readInt(ui, QStringLiteral("mainServerQrSplitPercent"), 78), 78);
    config.ui().mainQrPreviewVisible = readBool(ui, QStringLiteral("mainQrPreviewVisible"), false);
    config.ui().mainProxyEnabled = readBool(ui, QStringLiteral("mainProxyEnabled"), false);

    const QJsonObject mainColumnWidths = readObject(ui, QStringLiteral("mainColumnWidths"));
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
    writeIfFalse(ui, QStringLiteral("showMainOnStartup"), config.ui().showMainOnStartup);
    writeIfTrue(ui, QStringLiteral("autoRunEnabled"), config.ui().autoRunEnabled);
    writeIfNotEmpty(ui, QStringLiteral("languageCode"), config.ui().languageCode);
    const QString themeName =
        config.ui().themeName.trimmed().compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("Dark")
            : QStringLiteral("Light");
    if (themeName != QStringLiteral("Light")) {
        ui.insert(QStringLiteral("themeName"), themeName);
    }
    writeObjectIfNotEmpty(root, QStringLiteral("ui"), ui);
}

} // namespace JsonConfigUiSerialization
