#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigTunSerialization {

void read(const QJsonObject& root, TunConfigState& config);
void write(QJsonObject& root, const TunConfigState& config);

} // namespace JsonConfigTunSerialization
