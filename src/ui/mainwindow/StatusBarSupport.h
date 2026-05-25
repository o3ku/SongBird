#pragma once

#include <QString>

class QFontMetrics;

namespace StatusBarSupport {

QString elideTextWithThreeDots(const QFontMetrics& fontMetrics, const QString& text, int availableWidth);

QString currentServerStatusText(
    const QFontMetrics& fontMetrics,
    const QString& currentServerName,
    const QString& currentServerLocation,
    const QString& currentServerWarning,
    int currentServerNameMaximumWidth);

QString currentServerToolTip(
    const QFontMetrics& fontMetrics,
    const QString& currentServerName,
    int currentServerTooltipNameMaximumWidth);

QString routingStatusText(const QString& listenSummary);

QString transientStatusText(
    const QString& transientStatusMessage,
    bool backgroundTaskRunning,
    const QString& backgroundTaskDescription);

bool shouldSuppressRoutineStatus(
    const QString& transientStatusMessage,
    bool backgroundTaskRunning,
    const QString& backgroundTaskDescription);

} // namespace StatusBarSupport
