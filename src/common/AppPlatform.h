#pragma once

#include <QCoreApplication>
#include <QString>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#endif

inline bool isWindowsPlatform()
{
#if defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

inline bool isProcessElevated()
{
#if defined(Q_OS_WIN)
    HANDLE tokenHandle = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD returnedSize = 0;
    const BOOL ok = GetTokenInformation(
        tokenHandle,
        TokenElevation,
        &elevation,
        sizeof(elevation),
        &returnedSize);
    CloseHandle(tokenHandle);
    return ok != FALSE && elevation.TokenIsElevated != 0;
#else
    return true;
#endif
}

inline QString noServerPlaceholderText()
{
    return QCoreApplication::translate("AppPlatform", "<No Server>");
}
