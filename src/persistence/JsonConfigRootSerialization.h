#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigRootSerialization {

void read(const QJsonObject& root, RootConfigState& config);
void write(QJsonObject& root, const RootConfigState& config);

} // namespace JsonConfigRootSerialization
