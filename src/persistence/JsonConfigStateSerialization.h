#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigStateSerialization {

void read(const QJsonObject& root, Config& config);
void write(QJsonObject& root, const Config& config);

} // namespace JsonConfigStateSerialization
