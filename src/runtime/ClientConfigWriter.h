#pragma once

#include <QJsonObject>
#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/CoreInfo.h"

class ClientConfigWriter {
public:
    struct GeneratedConfig {
        QString fileName;
        QJsonObject root;
    };

    struct GeneratedConfigSet {
        GeneratedConfig primary;
        QList<GeneratedConfig> auxiliary;
        OperationResult result = OperationResult::ok();
    };

    explicit ClientConfigWriter(QString customConfigDirectory = {});

    void setExistingCoreTypes(const QList<CoreType>& existingCoreTypes);
    // 已有版本探测结果可直接传入；否则探测此次实际启动的可执行文件。
    void setRuntimeCoreInfo(const CoreInfo& coreInfo, const QString& version = {});

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
    CoreType runtimeCoreType_ = CoreType::Unknown;
    QString runtimeCoreVersion_;
};
