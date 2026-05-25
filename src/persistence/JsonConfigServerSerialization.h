#pragma once

#include <QJsonArray>
#include <QList>

#include "domain/models/VmessItem.h"

namespace JsonConfigServerSerialization {

QList<VmessItem> readServers(const QJsonArray& array);
QJsonArray writeServers(const QList<VmessItem>& items);

} // namespace JsonConfigServerSerialization
