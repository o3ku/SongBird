#pragma once

#include <QJsonObject>
#include <QList>

#include "domain/models/VmessItem.h"

namespace SubscriptionSingBoxParser {

QList<VmessItem> parseOutboundObject(const QJsonObject& object);

} // namespace SubscriptionSingBoxParser
