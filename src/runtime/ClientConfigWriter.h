#pragma once

#include <QJsonObject>
#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class ClientConfigWriter {
public:
    struct GeneratedConfig {
        QString fileName;
        QJsonObject root;
    };

    struct GeneratedConfigSet {
        GeneratedConfig primary;
        QList<GeneratedConfig> auxiliary;
    };

    explicit ClientConfigWriter(QString customConfigDirectory = {});

    void setExistingCoreTypes(const QList<CoreType>& existingCoreTypes);

    OperationResult writeClientConfig(
        const Config& config,
        const VmessItem& server,
        const QString& filePath) const;
    OperationResult writeClientConfigs(
        const Config& config,
        const VmessItem& server,
        const QString& filePath,
        QStringList* auxiliaryPaths) const;

    GeneratedConfigSet generateClientConfigs(
        const Config& config,
        const VmessItem& server) const;

private:
    OperationResult writeCustomConfig(const VmessItem& server, const QString& filePath) const;
    OperationResult validateServer(const Config& config, const VmessItem& server) const;
    OperationResult writeGeneratedConfig(const GeneratedConfig& generatedConfig, const QString& filePath) const;
    QJsonObject buildRoot(const Config& config, const VmessItem& server) const;

    QString resolveCustomConfigPath(const QString& address) const;
    QList<CoreType> effectiveExistingCoreTypes() const;

    QString customConfigDirectory_;
    QList<CoreType> existingCoreTypes_;
};
