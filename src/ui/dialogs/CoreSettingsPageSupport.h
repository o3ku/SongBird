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
    // Raw download bytes for the progress bar; kept separate from progressText because text is
    // throttled into log lines while the bar needs every update.
    qint64 downloadReceivedBytes = 0;
    qint64 downloadTotalBytes = 0;
    bool downloadActive = false;
    // Set when the last attempt failed. The reason is what the retry button's tooltip shows,
    // so the user does not have to dig through the log to learn why.
    bool lastAttemptFailed = false;
    QString failureReason;
    // True while a cancellation has been requested but the worker has not stopped yet.
    bool cancelRequested = false;
};

struct CoreStatusPresentation {
    QString versionText;
    QString statusText;
    QString downloadButtonText;
    bool statusVisible = false;
    bool downloadButtonVisible = true;
    bool downloadButtonEnabled = true;
    bool setAllButtonVisible = false;
    int downloadPercent = 0;
    bool downloadIndeterminate = false;
    bool downloadProgressVisible = false;
    // Shown only while this core is the active update, so the user can stop it.
    bool cancelButtonVisible = false;
    bool cancelButtonEnabled = true;
    // Explains the button state on hover: the failure reason for a retry, or what the
    // button will do next. Empty means "no tooltip".
    QString downloadButtonToolTip;
    // The full progress text behind the bar, which is too narrow to show it.
    QString downloadProgressToolTip;
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
