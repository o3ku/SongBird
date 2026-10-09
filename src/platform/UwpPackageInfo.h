#pragma once

#include <QString>

// One installed UWP package, as the loopback-exemption code exchanges it.
//
// It lives outside platform/windows/ on purpose: the type carries no Windows dependency
// (five strings and a bool), and the UI layer and the platform seam both name it. Keeping
// it here lets them include one small neutral header instead of pulling in the Windows
// service declaration just to describe a row.
struct WindowsUwpPackageInfo {
    QString name;
    QString packageFamilyName;
    QString packageFullName;
    QString publisher;
    QString installLocation;
    bool loopbackEnabled = false;
};
