#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigUiSerialization {

void read(const QJsonObject& root, UiConfigState& config);
void write(QJsonObject& root, const UiConfigState& config);

} // namespace JsonConfigUiSerialization
