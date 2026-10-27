#pragma once

#include <QJsonArray>
#include <QList>
#include <QStringList>

#include "domain/models/VmessItem.h"

namespace ClashProxyItemParser {

// skippedTypes, when non-null, receives one entry per proxy this parser could not
// convert: the declared type, plus the node's name, so the entry can be traced
// back to a line in the document.
QList<VmessItem> parseProxyArray(const QJsonArray& proxies, QStringList* skippedTypes = nullptr);

} // namespace ClashProxyItemParser
