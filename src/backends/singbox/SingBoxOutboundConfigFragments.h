#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class SingBoxOutboundConfigFragments {
public:
    static QJsonObject buildPrimaryOutbound(const Config& config, const VmessItem& server);
    static QJsonObject buildTls(const Config& config, const VmessItem& server);
    static QJsonObject buildTransport(const VmessItem& server);
    static QJsonObject buildSocksOutbound(
        const QString& tag,
        const QString& server,
        int port,
        bool udpOverTcp = false);
};
