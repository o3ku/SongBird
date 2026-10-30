#include "persistence/JsonConfigStateSerialization.h"

#include <QJsonArray>

#include "appcore/HealthStripPolicy.h"
#include "persistence/JsonConfigUtils.h"

namespace {

using namespace JsonConfigUtils;

// A captured value has three states, not two, and the JSON has to carry all three or a restore
// cannot tell "this key did not exist" from "this key was empty" -- which is exactly the
// distinction SystemProxySnapshot exists to preserve.
//
//   key absent   -> nullopt (delete the registry key on restore)
//   key present  -> the string, possibly empty
//
// Only reached when the snapshot is valid; an invalid one writes no object at all.
QJsonObject toSystemProxySnapshotObject(const SystemProxySnapshot& snapshot)
{
    QJsonObject object;
    object.insert(QStringLiteral("enabled"), snapshot.enabled);

    const auto writeCaptured = [&object](const QString& key, const std::optional<QString>& value) {
        if (value.has_value()) {
            object.insert(key, *value);
        }
    };
    writeCaptured(QStringLiteral("server"), snapshot.server);
    writeCaptured(QStringLiteral("overrideList"), snapshot.overrideList);
    writeCaptured(QStringLiteral("autoConfigUrl"), snapshot.autoConfigUrl);
    return object;
}

std::optional<QString> readCaptured(const QJsonObject& object, const QString& key)
{
    if (!object.contains(key)) {
        return std::nullopt;
    }
    return object.value(key).toString();
}

SystemProxySnapshot fromSystemProxySnapshotObject(const QJsonObject& object)
{
    SystemProxySnapshot snapshot;
    if (object.isEmpty()) {
        return snapshot;
    }
    snapshot.valid = true;
    snapshot.enabled = readBool(object, QStringLiteral("enabled"), false);
    snapshot.server = readCaptured(object, QStringLiteral("server"));
    snapshot.overrideList = readCaptured(object, QStringLiteral("overrideList"));
    snapshot.autoConfigUrl = readCaptured(object, QStringLiteral("autoConfigUrl"));
    return snapshot;
}

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
    // Written only when set: unchecked is the default, and a state file from a build without the
    // toggle reads back as subscription view, which is what its user last saw.
    writeIfTrue(ui, QStringLiteral("mainGroupByLocation"), config.ui().mainGroupByLocation);
    writeIfNotEmpty(ui, QStringLiteral("mainSelectedLocationId"), config.ui().mainSelectedLocationId);
    writeIfTrue(ui, QStringLiteral("autoSwitchInGroup"), config.ui().autoSwitchInGroup);
    writeIfNotEmpty(ui, QStringLiteral("settingsRoutingRuleTabKey"), config.ui().settingsRoutingRuleTabKey);
    writeIfNotDefault(ui, QStringLiteral("mainServerLogSplitPercent"), config.ui().mainServerLogSplitPercent, 60);
    writeIfNotDefault(ui, QStringLiteral("mainServerQrSplitPercent"), config.ui().mainServerQrSplitPercent, 78);
    writeIfTrue(ui, QStringLiteral("mainQrPreviewVisible"), config.ui().mainQrPreviewVisible);
    writeIfTrue(ui, QStringLiteral("mainProxyEnabled"), config.ui().mainProxyEnabled);
    writeIfTrue(ui, QStringLiteral("coreSetupPromptShown"), config.ui().coreSetupPromptShown);

    QJsonObject mainColumnWidths;
    for (auto it = config.ui().mainColumnWidths.constBegin(); it != config.ui().mainColumnWidths.constEnd(); ++it) {
        if (it.value() > 0) {
            mainColumnWidths.insert(it.key(), it.value());
        }
    }
    writeObjectIfNotEmpty(ui, QStringLiteral("mainColumnWidths"), mainColumnWidths);

    return ui;
}

QJsonArray toServerStateArray(const QList<VmessItem>& servers)
{
    QJsonArray array;
    for (const VmessItem& server : servers) {
        QJsonObject object;
        writeIfNotEmpty(object, QStringLiteral("indexId"), server.indexId);
        writeIfNotEmpty(object, QStringLiteral("testResult"), server.testResult);
        // The health strip is state for the same reason testResult is: it is measured, not
        // configured, and it belongs next to the other per-server measurement. Written through the
        // policy's own decoder so a value the policy would reject is never persisted in the first
        // place -- the encode()/decode() pair is the format, and this is the only writer.
        const QList<HealthStripPolicy::Cell> cells = HealthStripPolicy::decode(server.healthStrip);
        if (!cells.isEmpty()) {
            writeIfNotEmpty(object, QStringLiteral("healthStrip"), HealthStripPolicy::encode(cells));
        }
        // Location grouping state sits beside the other per-server state: it is a hand-made
        // assignment, not configuration, and it must survive a restart without touching the
        // primary config file. favorite is only written when set, so a state file written before
        // this field existed reads back identically to one written by a build that has it.
        writeIfNotEmpty(object, QStringLiteral("locationOverride"), server.locationOverride);
        if (server.favorite) {
            object.insert(QStringLiteral("favorite"), true);
        }
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
    config.systemProxySnapshot() = fromSystemProxySnapshotObject(
        readObject(root, QStringLiteral("systemProxySnapshot")));

    const QJsonObject ui = readObject(root, QStringLiteral("ui"));
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
        config.ui().mainGroupByLocation =
            readBool(ui, QStringLiteral("mainGroupByLocation"), config.ui().mainGroupByLocation);
        if (ui.contains(QStringLiteral("mainSelectedLocationId"))) {
            config.ui().mainSelectedLocationId = readString(ui, QStringLiteral("mainSelectedLocationId"));
        }
        config.ui().autoSwitchInGroup =
            readBool(ui, QStringLiteral("autoSwitchInGroup"), config.ui().autoSwitchInGroup);
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
        config.ui().coreSetupPromptShown =
            readBool(ui, QStringLiteral("coreSetupPromptShown"), config.ui().coreSetupPromptShown);

        const QJsonObject mainColumnWidths = readObject(ui, QStringLiteral("mainColumnWidths"));
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

    const QJsonArray serverStates = readArray(root, QStringLiteral("serverStates"));
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
            if (object.contains(QStringLiteral("healthStrip"))) {
                // Decoded rather than copied: a malformed strip yields an empty history, and an
                // empty history renders as untested hours. Showing the wrong history would be worse
                // than showing none, because the user would trust it.
                server.healthStrip = HealthStripPolicy::encode(
                    HealthStripPolicy::decode(readString(object, QStringLiteral("healthStrip"))));
            }
            if (object.contains(QStringLiteral("locationOverride"))) {
                // Not validated against the catalog here: persistence is a dumb copy, and the
                // catalog's own normalize-at-read (LocationCatalog::resolvedLocationId) is what
                // keeps a stale or hand-edited value from hiding the node from every item.
                server.locationOverride = readString(object, QStringLiteral("locationOverride"));
            }
            server.favorite = readBool(object, QStringLiteral("favorite"), false);
            break;
        }
    }
}

void write(QJsonObject& root, const Config& config)
{
    root = QJsonObject();

    const QJsonObject ui = toUiStateObject(config);
    writeObjectIfNotEmpty(root, QStringLiteral("ui"), ui);

    const QJsonArray serverStates = toServerStateArray(config.collection().servers);
    writeArrayIfNotEmpty(root, QStringLiteral("serverStates"), serverStates);

    // Written last, and only when capture() actually ran: an invalid snapshot must leave no key
    // behind, so a config that never touched the proxy stays byte-identical to one written by a
    // build that predates this field.
    if (config.systemProxySnapshot().valid) {
        root.insert(QStringLiteral("systemProxySnapshot"),
                    toSystemProxySnapshotObject(config.systemProxySnapshot()));
    }
}

} // namespace JsonConfigStateSerialization
