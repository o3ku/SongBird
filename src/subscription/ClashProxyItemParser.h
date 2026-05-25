#pragma once

#include <QJsonArray>
#include <QList>

#include "domain/models/VmessItem.h"

namespace ClashProxyItemParser {

QList<VmessItem> parseProxyArray(const QJsonArray& proxies);

} // namespace ClashProxyItemParser
