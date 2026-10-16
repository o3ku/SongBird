#include "ui/dialogs/CoreSettingsPageSupport.h"

#include <QCoreApplication>

#include "runtime/ProtocolCoreCompat.h"

namespace {

QString trCoreSettings(const char* text)
{
    return QCoreApplication::translate("CoreSettingsPageWidget", text);
}

// Human-readable byte count for a tooltip. Kept here rather than in Qt's
// QLocale because the unit symbols are the same in every locale this app ships,
// and this way the numbers line up with what the download reports.
QString formatByteCount(qint64 bytes)
{
    constexpr double kKilo = 1024.0;
    constexpr double kMega = kKilo * 1024.0;
    constexpr double kGiga = kMega * 1024.0;

    const double value = static_cast<double>(qMax<qint64>(0, bytes));
    if (value >= kGiga) {
        return QStringLiteral("%1 GB").arg(value / kGiga, 0, 'f', 2);
    }
    if (value >= kMega) {
        return QStringLiteral("%1 MB").arg(value / kMega, 0, 'f', 1);
    }
    if (value >= kKilo) {
        return QStringLiteral("%1 KB").arg(value / kKilo, 0, 'f', 1);
    }
    return QStringLiteral("%1 B").arg(static_cast<qint64>(value));
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

    // Cancel is only meaningful while this core is the one being updated and the
    // worker has not already been asked to stop.
    presentation.cancelButtonVisible = isActive;
    presentation.cancelButtonEnabled = isActive && !input.cancelRequested;

    // The button is a retry after a failure, and its tooltip carries the reason so the
    // user does not have to open the log to find out what went wrong.
    if (!isActive && input.lastAttemptFailed) {
        presentation.downloadButtonText = trCoreSettings("Retry");
        const QString reason = input.failureReason.trimmed();
        presentation.downloadButtonToolTip = reason.isEmpty()
            ? trCoreSettings("The last attempt failed. Select to try again.")
            : trCoreSettings("The last attempt failed: %1").arg(reason);
    } else if (!isActive && !presentation.downloadButtonEnabled) {
        presentation.downloadButtonToolTip = trCoreSettings("Another core is being updated.");
    }

    if (input.downloadActive && isActive) {
        if (input.downloadTotalBytes > 0) {
            presentation.downloadPercent = qBound(
                0,
                static_cast<int>((input.downloadReceivedBytes * 100) / input.downloadTotalBytes),
                100);
            presentation.downloadIndeterminate = false;
        } else {
            // Content length unknown: the bar runs as a busy indicator instead of showing a
            // bogus percentage.
            presentation.downloadPercent = 0;
            presentation.downloadIndeterminate = true;
        }
        presentation.downloadProgressVisible = true;

        // The bar is only wide enough for the percentage, so the byte counts go in the
        // tooltip -- which is also the only place the user can see how big the download is.
        if (input.downloadTotalBytes > 0) {
            presentation.downloadProgressToolTip = trCoreSettings("%1 of %2 downloaded (%3%)")
                .arg(formatByteCount(input.downloadReceivedBytes),
                     formatByteCount(input.downloadTotalBytes))
                .arg(presentation.downloadPercent);
        } else {
            presentation.downloadProgressToolTip = trCoreSettings("%1 downloaded")
                .arg(formatByteCount(input.downloadReceivedBytes));
        }
    }
    return presentation;
}
