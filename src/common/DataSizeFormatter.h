#pragma once

#include <QString>

inline QString formatByteCount(quint64 value)
{
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};

    double size = static_cast<double>(value);
    int unitIndex = 0;
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        ++unitIndex;
    }

    return QString::number(size, unitIndex == 0 ? 'f' : 'g', unitIndex == 0 ? 0 : 3) + QString::fromLatin1(units[unitIndex]);
}
