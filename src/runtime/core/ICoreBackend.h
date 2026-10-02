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

// What an auxiliary TUN device should do with the traffic it captures.
//
// A TUN device is created by a core process of its own, separate from the proxy session, so the
// front end has to say which of the two states that process is in.
enum class AuxiliaryTunRouting {
    // Relay captured traffic into the local proxy listener. Used while the proxy session runs.
    RelayToLocalProxy,
    // Carry no proxy traffic at all: everything goes direct or is blocked. Used while the proxy
    // session is down so the adapter stays alive instead of being torn down and recreated.
    DirectOnly,
};

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
    // Root for an auxiliary core process whose only job is to create a TUN device.
    // Returns an empty object when the core cannot back a TUN device.
    virtual QJsonObject buildAuxiliaryTunClientRoot(const Config& config, AuxiliaryTunRouting routing) const
    {
        Q_UNUSED(config)
        Q_UNUSED(routing)
        return {};
    }
    virtual QUrl releasesApiUrl() const = 0;
    virtual CoreUpdateAssetPolicy updateAssetPolicy() const = 0;
    virtual int scoreReleaseAssetName(const QString& assetName, bool prefer64Bit) const = 0;
};
