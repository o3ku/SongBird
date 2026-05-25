#include "ui/theme/AppThemeStyle.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QIODevice>
#include <QPalette>

namespace {

using namespace AppThemeInternal;

void replaceToken(QString& styleSheet, const char* tokenName, const char* value)
{
    styleSheet.replace(
        QStringLiteral("@%1@").arg(QString::fromLatin1(tokenName)),
        QString::fromLatin1(value));
}

void applyThemeTokens(QString& styleSheet, const ThemePalette& palette)
{
    replaceToken(styleSheet, "background", palette.background);
    replaceToken(styleSheet, "surface", palette.surface);
    replaceToken(styleSheet, "toolbarSurface", palette.toolbarSurface);
    replaceToken(styleSheet, "surfaceRaised", palette.surfaceRaised);
    replaceToken(styleSheet, "surfaceMuted", palette.surfaceMuted);
    replaceToken(styleSheet, "surfaceSubtle", palette.surfaceSubtle);
    replaceToken(styleSheet, "sectionHeader", palette.sectionHeader);
    replaceToken(styleSheet, "sectionBody", palette.sectionBody);
    replaceToken(styleSheet, "control", palette.control);
    replaceToken(styleSheet, "controlHover", palette.controlHover);
    replaceToken(styleSheet, "controlDisabled", palette.controlDisabled);
    replaceToken(styleSheet, "dialogButtonDisabled", palette.dialogButtonDisabled);
    replaceToken(styleSheet, "border", palette.border);
    replaceToken(styleSheet, "borderMuted", palette.borderMuted);
    replaceToken(styleSheet, "borderStrong", palette.borderStrong);
    replaceToken(styleSheet, "buttonBorder", palette.buttonBorder);
    replaceToken(styleSheet, "buttonBorderHover", palette.buttonBorderHover);
    replaceToken(styleSheet, "toolbarButtonBorder", palette.toolbarButtonBorder);
    replaceToken(styleSheet, "runtimeDisabledBorder", palette.runtimeDisabledBorder);
    replaceToken(styleSheet, "inputBorderHover", palette.inputBorderHover);
    replaceToken(styleSheet, "subtleButtonBorder", palette.subtleButtonBorder);
    replaceToken(styleSheet, "disabledBorder", palette.disabledBorder);
    replaceToken(styleSheet, "dialogButtonDisabledBorder", palette.dialogButtonDisabledBorder);
    replaceToken(styleSheet, "panelBorder", palette.panelBorder);
    replaceToken(styleSheet, "toolbarBorder", palette.toolbarBorder);
    replaceToken(styleSheet, "focus", palette.focus);
    replaceToken(styleSheet, "text", palette.text);
    replaceToken(styleSheet, "textMuted", palette.textMuted);
    replaceToken(styleSheet, "textSubtle", palette.textSubtle);
    replaceToken(styleSheet, "accent", palette.accent);
    replaceToken(styleSheet, "accentSoft", palette.accentSoft);
    replaceToken(styleSheet, "accentHover", palette.accentHover);
    replaceToken(styleSheet, "accentPressed", palette.accentPressed);
    replaceToken(styleSheet, "activeBackground", palette.activeBackground);
    replaceToken(styleSheet, "qrPressedBackground", palette.qrPressedBackground);
    replaceToken(styleSheet, "hover", palette.hover);
    replaceToken(styleSheet, "selection", palette.selection);
    replaceToken(styleSheet, "success", palette.success);
    replaceToken(styleSheet, "warning", palette.warning);
    replaceToken(styleSheet, "error", palette.error);
    replaceToken(styleSheet, "neutral", palette.neutral);
    replaceToken(styleSheet, "validationError", palette.validationError);
    replaceToken(styleSheet, "overlay", palette.overlay);
    replaceToken(styleSheet, "loadingContent", palette.loadingContent);
    replaceToken(styleSheet, "link", palette.link);
    replaceToken(styleSheet, "scrollbar", palette.scrollbar);
    replaceToken(styleSheet, "scrollbarHover", palette.scrollbarHover);
    replaceToken(styleSheet, "menuSeparator", palette.menuSeparator);
    replaceToken(styleSheet, "splitter", palette.splitter);
    replaceToken(styleSheet, "splitterHover", palette.splitterHover);
    replaceToken(styleSheet, "serverSelectedBackground", palette.serverSelectedBackground);
    replaceToken(styleSheet, "serverCurrentBackground", palette.serverCurrentBackground);
    replaceToken(styleSheet, "selectionText", palette.selectionText);
    replaceToken(styleSheet, "tabSelectedText", palette.tabSelectedText);
    replaceToken(styleSheet, "downArrowImage", palette.downArrowImage);
    replaceToken(styleSheet, "upArrowImage", palette.upArrowImage);
}

} // namespace

namespace AppThemeInternal {

void applyApplicationPalette(QApplication& app, const ThemePalette& palette)
{
    QPalette appPalette;
    appPalette.setColor(QPalette::Window, QColor(color(palette.background)));
    appPalette.setColor(QPalette::WindowText, QColor(color(palette.text)));
    appPalette.setColor(QPalette::Base, QColor(color(palette.surface)));
    appPalette.setColor(QPalette::AlternateBase, QColor(color(palette.surfaceSubtle)));
    appPalette.setColor(QPalette::ToolTipBase, QColor(color(palette.surfaceRaised)));
    appPalette.setColor(QPalette::ToolTipText, QColor(color(palette.text)));
    appPalette.setColor(QPalette::Text, QColor(color(palette.text)));
    appPalette.setColor(QPalette::Button, QColor(color(palette.control)));
    appPalette.setColor(QPalette::ButtonText, QColor(color(palette.text)));
    appPalette.setColor(QPalette::BrightText, QColor(color(palette.error)));
    appPalette.setColor(QPalette::Highlight, QColor(color(palette.selection)));
    appPalette.setColor(QPalette::HighlightedText, QColor(color(palette.selectionText)));
    appPalette.setColor(QPalette::Link, QColor(color(palette.link)));
    appPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(color(palette.textSubtle)));
    appPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(color(palette.textSubtle)));
    appPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(color(palette.textSubtle)));
    appPalette.setColor(QPalette::Disabled, QPalette::Button, QColor(color(palette.controlDisabled)));
    app.setPalette(appPalette);
}

QString loadThemeStyleSheet(QStringView themeName)
{
    const ThemePalette& palette = paletteForName(themeName);
    QFile file(QStringLiteral(":/themes/AppTheme.qss.in"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString styleSheet = QString::fromUtf8(file.readAll());
        applyThemeTokens(styleSheet, palette);
        return styleSheet;
    }

    return QString();
}

} // namespace AppThemeInternal
