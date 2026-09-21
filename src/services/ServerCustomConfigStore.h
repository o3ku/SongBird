#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "domain/models/VmessItem.h"

class ServerCustomConfigStore {
public:
    explicit ServerCustomConfigStore(QString customConfigDirectory = {});

    QString resolveConfigPath(const QString& address) const;
    OperationResult prepareServer(VmessItem& server, const VmessItem* existing) const;

    // Deletes a config file this store copied into managed storage.
    //
    // Succeeds when there is nothing of ours to delete -- an unmanaged address, or a file that is
    // already gone -- because neither leaves a stale file behind. It only fails when a managed
    // file exists and could not be removed, and then the message names the file and the reason:
    // a plain bool could not tell "nothing to do" apart from "could not delete", so a caller that
    // ignored the result silently leaked the file.
    OperationResult removeManagedConfig(const QString& address) const;

private:
    bool isManagedConfigPath(const QString& filePath) const;

    QString customConfigDirectory_;
};
