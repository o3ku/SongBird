#include "ui/dialogs/CoreSettingsPageSupport.h"

#include <QCoreApplication>

#include "runtime/ProtocolCoreCompat.h"

namespace {

QString trCoreSettings(const char* text)
{
    return QCoreApplication::translate("CoreSettingsPageWidget", text);
}

} // namespace

QStringList CoreSettingsPageSupport::singBoxMuxProtocolOptions()
{
    return {
        QStringLiteral("h2mux"),
        QStringLiteral("smux"),
        QStringLiteral("yamux"),
        QString()};
}

QStringList CoreSettingsPageSupport::xrayUserAgentOptions()
{
    return {
        QString(),
        QStringLiteral("chrome"),
        QStringLiteral("firefox"),
        QStringLiteral("edge"),
        QStringLiteral("golang")};
}

QList<CoreType> CoreSettingsPageSupport::settingsCoreDisplayOrder(const QList<CoreType>& cores)
{
    QList<CoreType> ordered;
    for (const CoreType core : orderedCoreTypes()) {
        if (cores.contains(core)) {
            ordered.append(core);
        }
    }
    for (const CoreType core : cores) {
        if (!ordered.contains(core)) {
            ordered.append(core);
        }
    }
    return ordered;
}

QList<CoreSettingsPageSupport::CoreProtocolEntry> CoreSettingsPageSupport::coreProtocolEntries()
{
    QList<CoreProtocolEntry> entries;
    for (const ConfigType configType : configurableCoreProtocols()) {
        entries.append(CoreProtocolEntry{configTypeDisplayName(configType), configType});
    }
    return entries;
}

int CoreSettingsPageSupport::configuredCoreTypeValue(
    const QList<CoreTypeItem>& coreTypeItems,
    ConfigType configType,
    int fallbackCoreType)
{
    for (const CoreTypeItem& item : coreTypeItems) {
        if (item.configType == static_cast<int>(configType)) {
            return item.coreType;
        }
    }

    return fallbackCoreType;
}

QString CoreSettingsPageSupport::coreStatusTextForProgress(const QString& message)
{
    const QString trimmedMessage = message.trimmed();
    if (trimmedMessage.isEmpty()) {
        return QString();
    }

    const QString lowerMessage = trimmedMessage.toLower();
    if (lowerMessage == trCoreSettings("Failed").toLower()) {
        return trCoreSettings("Failed");
    }
    if (lowerMessage.contains(QStringLiteral("download"))
        || lowerMessage.contains(QStringLiteral("extract"))
        || lowerMessage.contains(QStringLiteral("install"))
        || lowerMessage.contains(QStringLiteral("preparing"))) {
        return trCoreSettings("Downloading...");
    }
    if (lowerMessage.contains(QStringLiteral("checking"))
        || lowerMessage.contains(QStringLiteral("selected"))
        || lowerMessage.contains(QStringLiteral("current"))
        || lowerMessage.contains(QStringLiteral("latest"))
        || lowerMessage.contains(QStringLiteral("release metadata"))) {
        return trCoreSettings("Checking...");
    }

    return trCoreSettings("Checking...");
}

CoreSettingsPageSupport::CoreStatusPresentation CoreSettingsPageSupport::coreStatusPresentation(
    const CoreStatusInput& input)
{
    const bool updateRunning = input.updatingCoreType != CoreType::Unknown;
    const bool isActive = input.coreType == input.updatingCoreType;
    const bool installed = input.existingCoreTypes.contains(input.coreType);

    CoreStatusPresentation presentation;
    if (isActive) {
        presentation.versionText = trCoreSettings("Downloading...");
    } else if (!input.versionText.isEmpty()) {
        presentation.versionText = input.versionText;
    } else if (installed) {
        presentation.versionText = trCoreSettings("Installed");
    } else {
        presentation.versionText = trCoreSettings("Not found");
    }

    presentation.statusText = coreStatusTextForProgress(input.progressText);
    presentation.statusVisible = !isActive && !presentation.statusText.isEmpty();
    presentation.downloadButtonText = installed ? trCoreSettings("Update") : trCoreSettings("Install");
    presentation.downloadButtonVisible = !isActive;
    presentation.downloadButtonEnabled = !updateRunning;
    presentation.setAllButtonVisible = !isActive && installed;
    return presentation;
}
