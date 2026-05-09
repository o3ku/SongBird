#pragma once

#include <QString>

class QApplication;
class QStringView;

namespace AppTheme {

void applyApplicationTheme(QApplication& app);
QString semanticStatusProperty(QStringView colorHex);
QString successStatusColor();
QString warningStatusColor();
QString errorStatusColor();

} // namespace AppTheme
