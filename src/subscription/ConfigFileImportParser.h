#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include "common/OperationResult.h"
#include "domain/models/VmessItem.h"

class ConfigFileImportParser {
public:
    static OperationResult parseClientConfig(const QString& text, VmessItem* item);
    static OperationResult parseServerConfig(const QString& text, VmessItem* item);

private:
    static OperationResult parseDocument(const QString& text, QJsonObject* root, const QString& configKind);
    static void populateCommonStreamSettings(const QJsonObject& streamSettings, VmessItem& item);
    static bool parseVnextUser(const QJsonObject& settings, const QString& protocol, VmessItem& item);
    static bool parseServerEndpoint(const QJsonObject& settings, VmessItem& item);
    static QString joinStringValues(const QJsonValue& value);
    static QStringList toStringList(const QJsonValue& value);
};
