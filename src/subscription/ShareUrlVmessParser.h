#pragma once

#include <QString>

#include "domain/models/VmessItem.h"

namespace ShareUrlVmessParser {

VmessItem parseVmess(const QString& shareUrl, bool* ok);

} // namespace ShareUrlVmessParser
