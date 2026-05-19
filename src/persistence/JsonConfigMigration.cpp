#include "persistence/JsonConfigMigration.h"

#include <QJsonArray>
#include <QString>

namespace {

void promoteArrayAlias(QJsonObject& root, const QString& canonicalKey, const QString& legacyKey)
{
    const QJsonArray legacy = root.value(legacyKey).toArray();
    if (!root.contains(canonicalKey) && (!legacy.isEmpty() || root.contains(legacyKey))) {
        root.insert(canonicalKey, legacy);
    }
}

void migrateToSchemaVersion1(QJsonObject& root)
{
    promoteArrayAlias(root, QStringLiteral("GlobalHotkeys"), QStringLiteral("globalHotkeys"));
    promoteArrayAlias(root, QStringLiteral("routings"), QStringLiteral("routingItems"));

    QJsonObject uiItem = root.value(QStringLiteral("uiItem")).toObject();
    if (uiItem.value(QStringLiteral("languageCode")).toString().trimmed().isEmpty()
        && root.contains(QStringLiteral("languageCode"))) {
        uiItem.insert(QStringLiteral("languageCode"), root.value(QStringLiteral("languageCode")).toString());
    }

    if (!uiItem.isEmpty() || root.contains(QStringLiteral("uiItem")) || root.contains(QStringLiteral("languageCode"))) {
        root.insert(QStringLiteral("uiItem"), uiItem);
    }
}

} // namespace

QJsonObject migrateJsonConfigRoot(QJsonObject root)
{
    const int schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(0);
    if (schemaVersion >= kCurrentJsonConfigSchemaVersion) {
        return root;
    }

    migrateToSchemaVersion1(root);
    root.insert(QStringLiteral("schemaVersion"), kCurrentJsonConfigSchemaVersion);
    return root;
}
