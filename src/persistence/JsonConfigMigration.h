#pragma once

#include <QJsonObject>

inline constexpr int kCurrentJsonConfigSchemaVersion = 1;

QJsonObject migrateJsonConfigRoot(QJsonObject root);
