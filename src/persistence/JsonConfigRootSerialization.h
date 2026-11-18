#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"

namespace JsonConfigRootSerialization {

// The schema version every save writes. A load that meets a newer one must not
// silently misread it or overwrite it (see JsonConfigRepository).
inline constexpr int kSongBirdJsonConfigSchemaVersion = 7;

void read(const QJsonObject& root, RootConfigState& config);
void write(QJsonObject& root, const RootConfigState& config);

} // namespace JsonConfigRootSerialization
