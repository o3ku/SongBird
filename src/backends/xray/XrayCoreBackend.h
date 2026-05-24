#pragma once

#include "runtime/core/ICoreBackend.h"

class XrayCoreBackend final : public ICoreBackend {
public:
    CoreDescriptor descriptor() const override;
    CoreType type() const override;
    QString displayName() const override;
    bool supportsConfigType(ConfigType configType) const override;
    QStringList executableNames() const override;
    QStringList launchArguments(const QString& configPlaceholder) const override;
    bool appendConfigArgument() const override;
    QStringList configPreflightArguments(const QString& configFilePath) const override;
    QStringList versionCommandArguments() const override;
    QString extractVersionFromOutput(const QString& output) const override;
    OperationResult validateServer(const VmessItem& server) const override;
    QJsonObject buildClientRoot(const Config& config, const VmessItem& server) const override;
    QUrl releasesApiUrl() const override;
    CoreUpdateAssetPolicy updateAssetPolicy() const override;
    int scoreReleaseAssetName(const QString& assetName, bool prefer64Bit) const override;

private:
    static QJsonObject buildLog(const Config& config);
    static QJsonArray buildInbounds(const Config& config);
    static QJsonArray buildOutbounds(const Config& config, const VmessItem& server);
};
