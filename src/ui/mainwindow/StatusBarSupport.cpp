#include "ui/mainwindow/StatusBarSupport.h"

#include <QFontMetrics>
#include <QObject>

#include "common/AppPlatform.h"

namespace {

QString currentServerDisplayName(const QFontMetrics& fontMetrics, const QString& name, int availableWidth)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return noServerPlaceholderText();
    }

    return StatusBarSupport::elideTextWithThreeDots(fontMetrics, trimmed, availableWidth);
}

} // namespace

QString StatusBarSupport::elideTextWithThreeDots(
    const QFontMetrics& fontMetrics,
    const QString& text,
    int availableWidth)
{
    static const QString ellipsis = QStringLiteral("...");
    if (availableWidth <= 0 || fontMetrics.horizontalAdvance(text) <= availableWidth) {
        return text;
    }

    if (fontMetrics.horizontalAdvance(ellipsis) >= availableWidth) {
        return ellipsis;
    }

    int low = 0;
    int high = text.size();
    while (low < high) {
        const int mid = (low + high + 1) / 2;
        const QString candidate = text.left(mid) + ellipsis;
        if (fontMetrics.horizontalAdvance(candidate) <= availableWidth) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }

    return text.left(low) + ellipsis;
}

QString StatusBarSupport::currentServerStatusText(
    const QFontMetrics& fontMetrics,
    const QString& currentServerName,
    const QString& currentServerLocation,
    const QString& currentServerWarning,
    int currentServerNameMaximumWidth)
{
    QString text = QObject::tr("Current: %1").arg(
        currentServerDisplayName(fontMetrics, currentServerName, currentServerNameMaximumWidth));
    if (!currentServerLocation.isEmpty()) {
        text += QStringLiteral(" | %1").arg(currentServerLocation);
    }
    if (!currentServerWarning.isEmpty()) {
        text += QStringLiteral(" | %1").arg(currentServerWarning);
    }
    return text;
}

QString StatusBarSupport::currentServerToolTip(
    const QFontMetrics& fontMetrics,
    const QString& currentServerName,
    int currentServerTooltipNameMaximumWidth)
{
    const QString trimmed = currentServerName.trimmed();
    if (trimmed.isEmpty()) {
        return QObject::tr("Click to show the current server.");
    }

    return QObject::tr("Click to show the current server: %1").arg(
        elideTextWithThreeDots(fontMetrics, trimmed, currentServerTooltipNameMaximumWidth));
}

QString StatusBarSupport::routingStatusText(const QString& listenSummary)
{
    const QString listenText = listenSummary.isEmpty()
        ? QObject::tr("Unavailable")
        : listenSummary;
    return QObject::tr("Listening: %1").arg(listenText);
}

QString StatusBarSupport::transientStatusText(
    const QString& transientStatusMessage,
    bool backgroundTaskRunning,
    const QString& backgroundTaskDescription)
{
    if (!transientStatusMessage.isEmpty()) {
        return transientStatusMessage;
    }

    if (backgroundTaskRunning && !backgroundTaskDescription.isEmpty()) {
        return QObject::tr("Task: %1").arg(backgroundTaskDescription);
    }

    return QObject::tr("Ready");
}

bool StatusBarSupport::shouldSuppressRoutineStatus(
    const QString& transientStatusMessage,
    bool backgroundTaskRunning,
    const QString& backgroundTaskDescription)
{
    return !transientStatusMessage.isEmpty()
        || (backgroundTaskRunning && !backgroundTaskDescription.isEmpty());
}
