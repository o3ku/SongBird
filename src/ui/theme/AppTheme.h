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
// Keeps the "follow the system" preference: normalizeThemeName() folds every
// unrecognised value -- including "System" -- to Light, so the preference has to
// be checked before it. True when the stored value asks for the OS theme.
bool isSystemThemeName(QStringView themeName);
// Resolves a stored preference into a concrete palette name. Pure and
// platform-free: the caller reads the OS preference and passes it in.
//   * a "System" preference resolves to systemPreferredThemeName, or to
//     fallbackThemeName when the platform reported nothing (empty string);
//   * anything else is normalized like before.
QString resolveThemeName(
    QStringView themeName,
    QStringView systemPreferredThemeName,
    QStringView fallbackThemeName);
QString lightThemeName();
QString darkThemeName();
QString systemThemeName();
QString hoverBackgroundColor();
QString selectionBackgroundColor();
QString tableAlternateBaseColor();
QString tableAlternateBaseColor(QStringView themeName);
QString linkColor();
QString mutedTextColor();
QString successColor();
QString warningColor();
QString errorColor();
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
