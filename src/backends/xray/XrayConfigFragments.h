#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class XrayConfigFragments {
public:
    static QJsonObject buildSocksInbound(const Config& config, bool allowLan, int offset);
    static QJsonObject buildHttpInbound(const Config& config, bool allowLan, int offset);
    static QJsonObject buildHttpInboundWithTag(const Config& config, const QString& tag, int offset);
    static QJsonObject buildSniffing(const Config& config);
    static QJsonObject buildStreamSettings(const Config& config, const VmessItem& server);
    static QJsonObject buildPrimaryOutbound(const Config& config, const VmessItem& server);
    static QJsonObject buildDirectOutbound(const Config& config);
    static QJsonObject buildBlackholeOutbound();
    static QJsonObject buildFragmentOutbound();
};
