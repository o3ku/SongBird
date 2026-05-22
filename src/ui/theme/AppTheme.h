#pragma once

#include <QIcon>
#include <QList>
#include <QString>

class QApplication;
class QStringView;
class QTableView;
class QWidget;

namespace AppTheme {

void applyApplicationTheme(QApplication& app);
void applyApplicationTheme(QApplication& app, QStringView themeName);
void polishThemedWidgets(QApplication& app);
void applyCompactFont(QWidget* widget);
void applyCompactFont(const QList<QWidget*>& widgets);
void applyServerTableStyle(QTableView* tableView);
QString normalizeThemeName(QStringView themeName);
QString lightThemeName();
QString darkThemeName();
QString hoverBackgroundColor();
QString selectionBackgroundColor();
QString tableAlternateBaseColor();
QString tableAlternateBaseColor(QStringView themeName);
QIcon themedSvgIcon(const QString& resourcePath);
QString iconColor(bool enabled);

} // namespace AppTheme
