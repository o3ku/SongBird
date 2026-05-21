#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigPolicySerialization {

void read(const QJsonObject& root, PolicyConfigState& config);
void write(QJsonObject& root, const PolicyConfigState& config);

} // namespace JsonConfigPolicySerialization
