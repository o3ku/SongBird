#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

namespace ClashSubscriptionParser {

// `parseError` is passed through to the YAML walker: set, with no items returned, when
// the document uses a YAML feature the built-in reader cannot read. The JSON-first
// branch never sets it -- JSON has no YAML constructs.
QList<VmessItem> parseContent(const QString& content, QStringList* skippedTypes = nullptr,
                              QString* parseError = nullptr);
QList<VmessItem> parseProxyArray(const QJsonArray& proxies, QStringList* skippedTypes = nullptr);

} // namespace ClashSubscriptionParser
