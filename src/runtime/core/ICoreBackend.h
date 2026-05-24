#pragma once

#include <QList>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/core/CoreDescriptor.h"

struct CoreUpdateAssetPolicy {
    QString builtInFallbackTagName;
    QString builtInFallbackAssetName64;
    QString builtInFallbackAssetName32;
    QString builtInFallbackRepositoryPath;
    QString directLatestAssetName64;
    QString directLatestAssetName32;
    QUrl directLatestDownloadUrl64;
    QUrl directLatestDownloadUrl32;
};

class ICoreBackend {
public:
    virtual ~ICoreBackend() = default;

    virtual CoreDescriptor descriptor() const = 0;
    virtual CoreType type() const = 0;
    virtual QString displayName() const = 0;
    virtual bool supportsConfigType(ConfigType configType) const = 0;
    virtual QStringList executableNames() const = 0;
    virtual QStringList launchArguments(const QString& configPlaceholder) const = 0;
    virtual bool appendConfigArgument() const = 0;
    virtual QStringList configPreflightArguments(const QString& configFilePath) const = 0;
    virtual QStringList versionCommandArguments() const = 0;
    virtual QString extractVersionFromOutput(const QString& output) const = 0;
    virtual OperationResult validateServer(const VmessItem& server) const = 0;
    virtual QJsonObject buildClientRoot(const Config& config, const VmessItem& server) const = 0;
    virtual QJsonObject buildAuxiliaryTunClientRoot(const Config& config) const
    {
        Q_UNUSED(config)
        return {};
    }
    virtual QUrl releasesApiUrl() const = 0;
    virtual CoreUpdateAssetPolicy updateAssetPolicy() const = 0;
    virtual int scoreReleaseAssetName(const QString& assetName, bool prefer64Bit) const = 0;
};
