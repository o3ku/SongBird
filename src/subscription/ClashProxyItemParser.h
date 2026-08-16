#pragma once

#include <QJsonArray>
#include <QList>
#include <QStringList>

#include "domain/models/VmessItem.h"

namespace ClashProxyItemParser {

// skippedTypes, when non-null, receives the declared type of every proxy this
// parser could not convert, one entry per dropped node.
QList<VmessItem> parseProxyArray(const QJsonArray& proxies, QStringList* skippedTypes = nullptr);

} // namespace ClashProxyItemParser
