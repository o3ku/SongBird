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
QString accentSoftColor();
QString navigationSelectedTextColor();
QIcon themedSvgIcon(const QString& resourcePath);
// Same rendering as themedSvgIcon, but tinted with the palette's accent (light #2f6d56, dark
// #63b58e) instead of the text colour -- for glyphs that sit on a neutral ground and need to
// read as brand accent (the location-grouping toggle) rather than as content.
QIcon accentSvgIcon(const QString& resourcePath);
QString iconColor(bool enabled);
int textControlMinimumWidth(
    const QWidget* widget,
    const QString& text,
    int minimumCharacters,
    int chromeWidth);
void configureContentSizedLineEdit(QLineEdit* edit, int minimumCharacters);

} // namespace AppTheme
