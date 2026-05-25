#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/CoreTypeItem.h"
#include "domain/models/VmessItem.h"

namespace CoreSettingsPageSupport {

struct CoreProtocolEntry {
    QString name;
    ConfigType configType = ConfigType::VMess;
};

struct CoreStatusInput {
    CoreType coreType = CoreType::Unknown;
    CoreType updatingCoreType = CoreType::Unknown;
    QList<CoreType> existingCoreTypes;
    QString versionText;
    QString progressText;
};

struct CoreStatusPresentation {
    QString versionText;
    QString statusText;
    QString downloadButtonText;
    bool statusVisible = false;
    bool downloadButtonVisible = true;
    bool downloadButtonEnabled = true;
    bool setAllButtonVisible = false;
};

QStringList singBoxMuxProtocolOptions();
QStringList xrayUserAgentOptions();
QList<CoreType> settingsCoreDisplayOrder(const QList<CoreType>& cores);
QList<CoreProtocolEntry> coreProtocolEntries();
int configuredCoreTypeValue(
    const QList<CoreTypeItem>& coreTypeItems,
    ConfigType configType,
    int fallbackCoreType);
QString coreStatusTextForProgress(const QString& message);
CoreStatusPresentation coreStatusPresentation(const CoreStatusInput& input);

} // namespace CoreSettingsPageSupport
