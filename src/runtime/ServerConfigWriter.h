#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "domain/models/VmessItem.h"

class ServerConfigWriter {
public:
    OperationResult writeServerConfig(const VmessItem& server, const QString& filePath) const;

private:
    static OperationResult validateServer(const VmessItem& server);
    static QJsonObject buildRoot(const VmessItem& server);
    static QJsonObject buildLog();
    static QJsonObject buildInbound(const VmessItem& server);
    static QJsonObject buildInboundSettings(const VmessItem& server);
    static QJsonObject buildStreamSettings(const VmessItem& server);
    static QJsonArray buildOutbounds();
    static QStringList splitCsv(const QString& value);
    static QString resolveServerName(const VmessItem& server);
    static bool resolveAllowInsecure(const QString& value);
    static bool isRealityTransport(const QString& value);
};
