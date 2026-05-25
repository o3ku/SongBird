#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "domain/models/VmessItem.h"

class ServerCustomConfigStore {
public:
    explicit ServerCustomConfigStore(QString customConfigDirectory = {});

    QString resolveConfigPath(const QString& address) const;
    OperationResult prepareServer(VmessItem& server, const VmessItem* existing) const;
    bool removeManagedConfig(const QString& address) const;

private:
    bool isManagedConfigPath(const QString& filePath) const;

    QString customConfigDirectory_;
};
