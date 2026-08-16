#pragma once

#include <QJsonObject>
#include <QList>
#include <QStringList>

#include "domain/models/VmessItem.h"

namespace SubscriptionSingBoxParser {

// skippedTypes, when non-null, receives the declared type of every outbound
// that carries a proxy protocol this parser could not convert. Built-in
// non-proxy outbounds (direct/block/dns/selector/urltest) are not reported.
QList<VmessItem> parseOutboundObject(const QJsonObject& object, QStringList* skippedTypes = nullptr);

} // namespace SubscriptionSingBoxParser
