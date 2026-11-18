#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

namespace ClashYamlProxyParser {

// `parseError` (optional) is set, and no items are returned, when the document uses a
// YAML feature the built-in reader cannot read (anchors, aliases, merge keys, block
// scalars, tags, a second document). The walker used to misread those as data, so a
// subscription that used them imported wrong nodes or dropped whole documents without
// a word; failing the document with the reason named is what makes the loss visible.
// The yaml-cpp migration is what would read them for real.
QList<VmessItem> parseProxyItems(const QString& content, QStringList* skippedTypes = nullptr,
                                 QString* parseError = nullptr);

} // namespace ClashYamlProxyParser
