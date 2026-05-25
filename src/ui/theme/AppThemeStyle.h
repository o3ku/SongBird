#pragma once

#include <QString>
#include <QStringView>

#include "ui/theme/AppThemePalette.h"

class QApplication;

namespace AppThemeInternal {

void applyApplicationPalette(QApplication& app, const ThemePalette& palette);
QString loadThemeStyleSheet(QStringView themeName);

} // namespace AppThemeInternal
