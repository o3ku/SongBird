#pragma once

#include <QList>
#include <QString>

class QApplication;
class QStringView;
class QWidget;

namespace AppTheme {

void applyApplicationTheme(QApplication& app);
void applyCompactFont(QWidget* widget);
void applyCompactFont(const QList<QWidget*>& widgets);
QString semanticStatusProperty(QStringView colorHex);
QString successStatusColor();
QString warningStatusColor();
QString errorStatusColor();

} // namespace AppTheme
