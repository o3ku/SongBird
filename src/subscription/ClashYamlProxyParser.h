#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

namespace ClashYamlProxyParser {

QList<VmessItem> parseProxyItems(const QString& content, QStringList* skippedTypes = nullptr);

} // namespace ClashYamlProxyParser
