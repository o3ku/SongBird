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

    // A group activation's generation outcome: the config set plus the members it actually
    // embeds, anchor first (the member tags are "member-1..N" over this order). Members that
    // cannot run on the launch core -- the protocol belongs to another core, or the core's
    // generator rejects the transport -- are dropped rather than failing the whole group, and
    // their display names come back for the log line.
    struct GroupGeneration {
        GeneratedConfigSet generated;
        QList<VmessItem> embeddedMembers;
        QStringList droppedMemberNames;
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

    // Group activation: the primary outbound becomes the core's native auto-select group over
    // the members. members must be non-empty and ordered (first = anchor, preferred as the
    // launch core). When the anchor cannot run on a core with a native auto-select group, the
    // first member that can takes over the launch core and the anchor is dropped with the other
    // foreign members. embeddedMembers, when not null, receives the members the written config
    // actually embeds.
    OperationResult writeClientGroupConfigs(
        const Config& config,
        const QList<VmessItem>& members,
        const QString& filePath,
        QStringList* auxiliaryPaths,
        QList<VmessItem>* embeddedMembers = nullptr,
        // Set to true when the failure is the target itself (generation rejected the member
        // set: no member runs on the launch core, or the core has no auto-select group), as
        // opposed to a runtime path or disk failure that would hit a single-node write too.
        bool* targetUnusable = nullptr) const;

    GeneratedConfigSet generateClientConfigs(
        const Config& config,
        const VmessItem& server) const;

    GroupGeneration generateClientGroupConfigs(
        const Config& config,
        const QList<VmessItem>& members) const;

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
