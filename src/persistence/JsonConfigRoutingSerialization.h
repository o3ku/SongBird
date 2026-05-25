#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigRoutingSerialization {

void readRouting(const QJsonObject& root, CollectionConfigState& config);
void writeRouting(QJsonObject& root, const CollectionConfigState& config);

} // namespace JsonConfigRoutingSerialization
