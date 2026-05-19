#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigSerialization {

Config parseConfig(const QJsonObject& root);
void applyConfig(QJsonObject& root, const Config& config);

} // namespace JsonConfigSerialization
