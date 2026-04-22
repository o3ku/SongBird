#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

struct StartupAdminElevationDecision {
    bool shouldPromptForElevation = false;
    QStringList relaunchArguments;
};

struct StartupSingleInstanceAcquireDecision {
    bool shouldBypassSingleInstance = false;
    int maxAcquireAttempts = 1;
    int retryIntervalMs = 0;
};

inline QStringList startupAdminRelaunchArguments(const QStringList& applicationArguments)
{
    return applicationArguments.size() > 1
        ? applicationArguments.mid(1)
        : QStringList();
}

inline QStringList startupAdminRelaunchArgumentsForRunningInstance(const QStringList& applicationArguments)
{
    QStringList arguments = startupAdminRelaunchArguments(applicationArguments);
    if (!arguments.contains(QStringLiteral("--admin-relaunch"))) {
        arguments.append(QStringLiteral("--admin-relaunch"));
    }

    return arguments;
}

inline bool startupAdminInteractivePromptsEnabled(
    bool nonInteractiveOptionSet,
    bool nonInteractiveEnvironmentSet)
{
    return !nonInteractiveOptionSet && !nonInteractiveEnvironmentSet;
}

inline bool startupConfigHasTunEnabled(const QByteArray& rawConfigJson)
{
    const QJsonDocument document = QJsonDocument::fromJson(rawConfigJson);
    if (!document.isObject()) {
        return false;
    }

    return document.object()
        .value(QStringLiteral("tunModeItem"))
        .toObject()
        .value(QStringLiteral("enableTun"))
        .toBool(false);
}

inline StartupAdminElevationDecision evaluateStartupAdminElevation(
    bool isWindows,
    bool isProcessElevated,
    bool interactivePromptsEnabled,
    bool configuredTunEnabled,
    const QStringList& applicationArguments)
{
    StartupAdminElevationDecision decision;
    decision.shouldPromptForElevation =
        isWindows && !isProcessElevated && interactivePromptsEnabled && configuredTunEnabled;
    decision.relaunchArguments = startupAdminRelaunchArguments(applicationArguments);
    return decision;
}

inline StartupSingleInstanceAcquireDecision evaluateStartupSingleInstanceAcquireDecision(
    bool disableSingleInstance,
    bool adminRelaunch)
{
    StartupSingleInstanceAcquireDecision decision;
    decision.shouldBypassSingleInstance = disableSingleInstance;
    if (!disableSingleInstance && adminRelaunch) {
        decision.maxAcquireAttempts = 50;
        decision.retryIntervalMs = 100;
    }

    return decision;
}

template <typename TryAcquire, typename SleepFor>
bool tryAcquireStartupSingleInstance(
    const StartupSingleInstanceAcquireDecision& decision,
    TryAcquire&& tryAcquire,
    SleepFor&& sleepFor)
{
    if (decision.shouldBypassSingleInstance) {
        return true;
    }

    for (int attempt = 0; attempt < decision.maxAcquireAttempts; ++attempt) {
        if (tryAcquire()) {
            return true;
        }

        if (attempt + 1 < decision.maxAcquireAttempts && decision.retryIntervalMs > 0) {
            sleepFor(decision.retryIntervalMs);
        }
    }

    return false;
}

inline bool shouldPromptForLanguageRestartAfterSettingsSave(
    bool languageChanged,
    bool adminRestartInitiated)
{
    return languageChanged && !adminRestartInitiated;
}

inline QString quoteWindowsShellExecuteArgument(const QString& argument)
{
    if (argument.isEmpty()) {
        return QStringLiteral("\"\"");
    }

    bool needsQuotes = false;
    for (const QChar character : argument) {
        if (character.isSpace() || character == QChar('"')) {
            needsQuotes = true;
            break;
        }
    }

    if (!needsQuotes) {
        return argument;
    }

    QString quoted;
    quoted.reserve(argument.size() + 2);
    quoted += QChar('"');

    int consecutiveBackslashes = 0;
    for (const QChar character : argument) {
        if (character == QChar('\\')) {
            ++consecutiveBackslashes;
            continue;
        }

        if (character == QChar('"')) {
            quoted += QString(consecutiveBackslashes * 2 + 1, QChar('\\'));
            quoted += QChar('"');
            consecutiveBackslashes = 0;
            continue;
        }

        if (consecutiveBackslashes > 0) {
            quoted += QString(consecutiveBackslashes, QChar('\\'));
            consecutiveBackslashes = 0;
        }

        quoted += character;
    }

    if (consecutiveBackslashes > 0) {
        quoted += QString(consecutiveBackslashes * 2, QChar('\\'));
    }

    quoted += QChar('"');
    return quoted;
}

inline QString buildWindowsShellExecuteParameters(const QStringList& arguments)
{
    QStringList quotedArguments;
    quotedArguments.reserve(arguments.size());
    for (const QString& argument : arguments) {
        quotedArguments.append(quoteWindowsShellExecuteArgument(argument));
    }

    return quotedArguments.join(QChar(' '));
}
