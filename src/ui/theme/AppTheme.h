#pragma once

#include <QIcon>
#include <QList>
#include <QString>

class QApplication;
class QLineEdit;
class QStringView;
class QTableView;
class QWidget;

namespace AppTheme {

void applyApplicationTheme(QApplication& app);
void applyApplicationTheme(QApplication& app, QStringView themeName);
void polishThemedWidgets(QApplication& app);
void applyCompactFont(QWidget* widget);
void applyCompactFont(const QList<QWidget*>& widgets);
// Re-evaluates the widget's style after a property change that QSS selectors
// depend on (unpolish/polish/update).
void refreshStyle(QWidget* widget);
void applyServerTableStyle(QTableView* tableView);
QString normalizeThemeName(QStringView themeName);
QString lightThemeName();
QString darkThemeName();
QString hoverBackgroundColor();
QString selectionBackgroundColor();
QString tableAlternateBaseColor();
QString tableAlternateBaseColor(QStringView themeName);
QString linkColor();
QString mutedTextColor();
QString successColor();
QString attentionBorderColor();
QIcon themedSvgIcon(const QString& resourcePath);
QString iconColor(bool enabled);
int textControlMinimumWidth(
    const QWidget* widget,
    const QString& text,
    int minimumCharacters,
    int chromeWidth);
void configureContentSizedLineEdit(QLineEdit* edit, int minimumCharacters);

} // namespace AppTheme
