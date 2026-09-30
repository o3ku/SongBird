#pragma once

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#endif

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

inline QStringList startupRelaunchArgumentsForRunningInstance(
    const QStringList& applicationArguments,
    bool adminRelaunch,
    qint64 currentPid)
{
    QStringList arguments = startupAdminRelaunchArguments(applicationArguments);

    for (int index = arguments.size() - 1; index >= 0; --index) {
        const QString argument = arguments.at(index);
        if (argument == QStringLiteral("--admin-relaunch")
            || argument == QStringLiteral("--restart-handoff")
            || argument.startsWith(QStringLiteral("--restart-wait-pid="))) {
            arguments.removeAt(index);
        }
    }

    arguments.append(QStringLiteral("--restart-wait-pid=%1").arg(currentPid));
    if (adminRelaunch) {
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
    bool restartContextDetected)
{
    StartupSingleInstanceAcquireDecision decision;
    decision.shouldBypassSingleInstance = disableSingleInstance;
    if (!disableSingleInstance && restartContextDetected) {
        decision.maxAcquireAttempts = 100;
        decision.retryIntervalMs = 100;
    }

    return decision;
}

inline qint64 parseRestartWaitPidArgument(const QString& value)
{
    bool ok = false;
    const qint64 pid = value.toLongLong(&ok);
    return ok && pid > 0 ? pid : 0;
}

inline bool waitForProcessExit(qint64 pid, int timeoutMs)
{
#if defined(Q_OS_WIN)
    if (pid <= 0) {
        return true;
    }

    HANDLE processHandle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (processHandle == nullptr) {
        return true;
    }

    const DWORD waitResult = WaitForSingleObject(processHandle, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
    CloseHandle(processHandle);
    return waitResult == WAIT_OBJECT_0 || waitResult == WAIT_FAILED;
#else
    Q_UNUSED(pid);
    Q_UNUSED(timeoutMs);
    return true;
#endif
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

inline bool restartProcessAsAdministrator(const QString& program, const QStringList& arguments)
{
#if defined(Q_OS_WIN)
    const QString nativeProgram = QDir::toNativeSeparators(program);
    const QString parameters = buildWindowsShellExecuteParameters(arguments);
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"runas",
        reinterpret_cast<LPCWSTR>(nativeProgram.utf16()),
        parameters.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(parameters.utf16()),
        nullptr,
        SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
#else
    Q_UNUSED(program);
    Q_UNUSED(arguments);
    return false;
#endif
}
