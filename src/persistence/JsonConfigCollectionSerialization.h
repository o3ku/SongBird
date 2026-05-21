#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigCollectionSerialization {

void read(const QJsonObject& root, CollectionConfigState& config);
void write(QJsonObject& root, const CollectionConfigState& config);

} // namespace JsonConfigCollectionSerialization
