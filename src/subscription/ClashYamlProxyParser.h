#pragma once

#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"

namespace ClashYamlProxyParser {

QList<VmessItem> parseProxyItems(const QString& content);

} // namespace ClashYamlProxyParser
