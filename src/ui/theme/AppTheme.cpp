#include "ui/theme/AppTheme.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QIODevice>
#include <QMetaObject>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QStringView>
#include <QSvgRenderer>
#include <QWidget>

#include "ui/theme/AppThemePalette.h"
#include "ui/theme/AppThemeStyle.h"
#include "ui/theme/ServerTableTheme.h"

using namespace AppThemeInternal;

namespace {

QHash<QString, QIcon>& themedIconCache()
{
    static QHash<QString, QIcon> cache;
    return cache;
}

} // namespace

void AppTheme::applyApplicationTheme(QApplication& app)
{
    applyApplicationTheme(app, QString::fromLatin1(kThemeLight));
}

void AppTheme::applyApplicationTheme(QApplication& app, QStringView themeName)
{
    const QString normalizedThemeName = normalizeThemeName(themeName);
    setCurrentThemeName(normalizedThemeName);
    themedIconCache().clear();
    applyApplicationPalette(app, currentPalette());
    app.setStyleSheet(loadThemeStyleSheet(normalizedThemeName));
    polishThemedWidgets(app);
}

void AppTheme::polishThemedWidgets(QApplication& app)
{
    const auto tables = app.allWidgets();
    for (QWidget* widget : tables) {
        ServerTableTheme::applyIfStyled(widget);
        QMetaObject::invokeMethod(widget, "refreshThemeAssets", Qt::DirectConnection);
    }
}

void AppTheme::applyCompactFont(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }

    QFont font = widget->font();
    font.setPointSizeF(9.0);
    widget->setFont(font);
}

void AppTheme::applyCompactFont(const QList<QWidget*>& widgets)
{
    for (QWidget* widget : widgets) {
        applyCompactFont(widget);
    }
}

void AppTheme::applyServerTableStyle(QTableView* tableView)
{
    ServerTableTheme::apply(tableView);
}

QString AppTheme::normalizeThemeName(QStringView themeName)
{
    const QString trimmed = themeName.toString().trimmed();
    if (trimmed.compare(QString::fromLatin1(kThemeDark), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(kThemeDark);
    }
    return QString::fromLatin1(kThemeLight);
}

QString AppTheme::lightThemeName()
{
    return QString::fromLatin1(kThemeLight);
}

QString AppTheme::darkThemeName()
{
    return QString::fromLatin1(kThemeDark);
}

QString AppTheme::hoverBackgroundColor()
{
    return color(currentPalette().hover);
}

QString AppTheme::selectionBackgroundColor()
{
    return color(currentPalette().selection);
}

QString AppTheme::tableAlternateBaseColor()
{
    return color(currentPalette().surfaceSubtle);
}

QString AppTheme::tableAlternateBaseColor(QStringView themeName)
{
    return color(paletteForName(normalizeThemeName(themeName)).surfaceSubtle);
}

QString AppTheme::linkColor()
{
    return color(currentPalette().link);
}

QString AppTheme::mutedTextColor()
{
    return color(currentPalette().textMuted);
}

QString AppTheme::successColor()
{
    return color(currentPalette().success);
}

QString AppTheme::attentionBorderColor()
{
    return color(currentPalette().warning);
}

QIcon AppTheme::themedSvgIcon(const QString& resourcePath)
{
    const QString cacheKey = iconColor(true) + QLatin1Char('|') + resourcePath;
    const auto cached = themedIconCache().constFind(cacheKey);
    if (cached != themedIconCache().constEnd()) {
        return cached.value();
    }

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QIcon(resourcePath);
    }

    QString svg = QString::fromUtf8(file.readAll());
    const QString iconColorValue = iconColor(true);
    svg.replace(QStringLiteral("currentColor"), iconColorValue);
    svg.replace(QRegularExpression(QStringLiteral("\\swidth=\"1em\"")), QStringLiteral(" width=\"24\""));
    svg.replace(QRegularExpression(QStringLiteral("\\sheight=\"1em\"")), QStringLiteral(" height=\"24\""));
    svg.replace(QRegularExpression(QStringLiteral("<svg\\b(?![^>]*\\bwidth=)")),
                QStringLiteral("<svg width=\"24\""));
    svg.replace(QRegularExpression(QStringLiteral("<svg\\b(?![^>]*\\bheight=)")),
                QStringLiteral("<svg height=\"24\""));

    const QByteArray svgData = svg.toUtf8();
    QSvgRenderer renderer(svgData);
    if (!renderer.isValid()) {
        return QIcon(resourcePath);
    }

    QSize iconSize = renderer.defaultSize();
    if (!iconSize.isValid() || iconSize.isEmpty()) {
        iconSize = QSize(24, 24);
    }

    QPixmap pixmap(iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter);

    const QIcon icon(pixmap);
    themedIconCache().insert(cacheKey, icon);
    return icon;
}

QString AppTheme::iconColor(bool enabled)
{
    const ThemePalette& palette = currentPalette();
    return enabled ? color(palette.text) : color(palette.textSubtle);
}
