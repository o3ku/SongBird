#include "ui/theme/AppTheme.h"

#include <QApplication>
#include <QAbstractItemModel>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QIODevice>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QRegularExpression>
#include <QStringView>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QSvgRenderer>
#include <QTableView>
#include <QWidget>

namespace {

constexpr auto kServerTableStyleProperty = "serverTableStyle";
constexpr auto kServerTableHoveredRowProperty = "serverTableHoveredRow";
constexpr auto kServerTableHoverTrackerName = "serverTableHoverTracker";
constexpr auto kServerTableStyleDelegateName = "serverTableStyleDelegate";

constexpr auto kThemeLight = "Light";
constexpr auto kThemeDark = "Dark";

struct ThemePalette {
    const char* background;
    const char* surface;
    const char* toolbarSurface;
    const char* surfaceRaised;
    const char* surfaceMuted;
    const char* surfaceSubtle;
    const char* control;
    const char* controlHover;
    const char* controlDisabled;
    const char* dialogButtonDisabled;
    const char* border;
    const char* borderMuted;
    const char* borderStrong;
    const char* buttonBorder;
    const char* buttonBorderHover;
    const char* toolbarButtonBorder;
    const char* runtimeDisabledBorder;
    const char* inputBorderHover;
    const char* subtleButtonBorder;
    const char* disabledBorder;
    const char* dialogButtonDisabledBorder;
    const char* panelBorder;
    const char* toolbarBorder;
    const char* focus;
    const char* text;
    const char* textMuted;
    const char* textSubtle;
    const char* accent;
    const char* accentSoft;
    const char* accentHover;
    const char* accentPressed;
    const char* activeBackground;
    const char* qrPressedBackground;
    const char* hover;
    const char* selection;
    const char* success;
    const char* warning;
    const char* error;
    const char* neutral;
    const char* validationError;
    const char* overlay;
    const char* loadingContent;
    const char* link;
    const char* scrollbar;
    const char* scrollbarHover;
    const char* menuSeparator;
    const char* splitter;
    const char* splitterHover;
    const char* serverSelectedBackground;
    const char* serverCurrentBackground;
    const char* selectionText;
    const char* tabSelectedText;
    const char* downArrowImage;
    const char* upArrowImage;
};

constexpr ThemePalette kLightPalette{
    .background = "#f3f5f8",
    .surface = "#ffffff",
    .toolbarSurface = "#ffffff",
    .surfaceRaised = "#ffffff",
    .surfaceMuted = "#eef2f6",
    .surfaceSubtle = "#f8fafc",
    .control = "#f8fafc",
    .controlHover = "#eef2f6",
    .controlDisabled = "#eef2f6",
    .dialogButtonDisabled = "#eef2f6",
    .border = "#c5ced8",
    .borderMuted = "#dde3ea",
    .borderStrong = "#8b98a8",
    .buttonBorder = "#c5ced8",
    .buttonBorderHover = "#8b98a8",
    .toolbarButtonBorder = "#dde3ea",
    .runtimeDisabledBorder = "#c5ced8",
    .inputBorderHover = "#8b98a8",
    .subtleButtonBorder = "#c5ced8",
    .disabledBorder = "#dde3ea",
    .dialogButtonDisabledBorder = "#dde3ea",
    .panelBorder = "#d8dee7",
    .toolbarBorder = "#d8dee7",
    .focus = "#2f6d56",
    .text = "#1f2328",
    .textMuted = "#667085",
    .textSubtle = "#8a94a6",
    .accent = "#2f6d56",
    .accentSoft = "#e5f0eb",
    .accentHover = "#d8e8df",
    .accentPressed = "#e5f0eb",
    .activeBackground = "#2f6d56",
    .qrPressedBackground = "#e5f0eb",
    .hover = "#f2f6f3",
    .selection = "#e6f0eb",
    .success = "#2f6d45",
    .warning = "#8a6a20",
    .error = "#a33a3a",
    .neutral = "#4d5f73",
    .validationError = "#a33a3a",
    .overlay = "rgba(255, 255, 255, 224)",
    .loadingContent = "rgba(255, 255, 255, 216)",
    .link = "#0969da",
    .scrollbar = "#8b98a8",
    .scrollbarHover = "#667085",
    .menuSeparator = "#e4e7ec",
    .splitter = "#d8dee7",
    .splitterHover = "#8b98a8",
    .serverSelectedBackground = "#b7dec9",
    .serverCurrentBackground = "#e1f3e9",
    .selectionText = "#1f2328",
    .tabSelectedText = "#ffffff",
    .downArrowImage = ":/app/down.svg",
    .upArrowImage = ":/app/up.svg"};

constexpr ThemePalette kDarkPalette{
    .background = "#20262d",
    .surface = "#262d35",
    .toolbarSurface = "#242b33",
    .surfaceRaised = "#2a323b",
    .surfaceMuted = "#303944",
    .surfaceSubtle = "#222932",
    .control = "#303944",
    .controlHover = "#394451",
    .controlDisabled = "#29313a",
    .dialogButtonDisabled = "#242b33",
    .border = "#536170",
    .borderMuted = "#46525f",
    .borderStrong = "#7f8d9e",
    .buttonBorder = "#667586",
    .buttonBorderHover = "#93a1b2",
    .toolbarButtonBorder = "#607080",
    .runtimeDisabledBorder = "#46525f",
    .inputBorderHover = "#8391a3",
    .subtleButtonBorder = "#536170",
    .disabledBorder = "#46525f",
    .dialogButtonDisabledBorder = "#3b4652",
    .panelBorder = "#46525f",
    .toolbarBorder = "#4a5664",
    .focus = "#86d49c",
    .text = "#edf1f6",
    .textMuted = "#bac5d1",
    .textSubtle = "#8793a2",
    .accent = "#63b58e",
    .accentSoft = "#264838",
    .accentHover = "#35654f",
    .accentPressed = "#2b5542",
    .activeBackground = "#35654f",
    .qrPressedBackground = "#2b5542",
    .hover = "#26362e",
    .selection = "#264838",
    .success = "#86d49c",
    .warning = "#d8b45f",
    .error = "#f08080",
    .neutral = "#c4ceda",
    .validationError = "#f08080",
    .overlay = "rgba(32, 38, 45, 224)",
    .loadingContent = "rgba(38, 45, 53, 216)",
    .link = "#8bdfff",
    .scrollbar = "#687585",
    .scrollbarHover = "#b8c4d2",
    .menuSeparator = "#3b4652",
    .splitter = "#536170",
    .splitterHover = "#8391a3",
    .serverSelectedBackground = "#4a8b6d",
    .serverCurrentBackground = "#2b4a3a",
    .selectionText = "#edf1f6",
    .tabSelectedText = "#edf1f6",
    .downArrowImage = ":/app/down-dark.svg",
    .upArrowImage = ":/app/up-dark.svg"};

const ThemePalette& paletteForName(QStringView themeName)
{
    return themeName.compare(QString::fromLatin1(kThemeDark), Qt::CaseInsensitive) == 0
        ? kDarkPalette
        : kLightPalette;
}

QString currentThemeName = QString::fromLatin1(kThemeLight);

const ThemePalette& currentPalette()
{
    return paletteForName(currentThemeName);
}

QString color(const char* value)
{
    return QString::fromLatin1(value);
}

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

class ServerTableStyleDelegate final : public QStyledItemDelegate {
public:
    explicit ServerTableStyleDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        setObjectName(QString::fromLatin1(kServerTableStyleDelegateName));
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem styledOption(option);
        initStyleOption(&styledOption, index);

        const auto* tableView = qobject_cast<const QTableView*>(parent());
        const bool selected = (styledOption.state & QStyle::State_Selected) != 0;
        const int hoveredRow = tableView == nullptr
            ? -1
            : tableView->property(kServerTableHoveredRowProperty).toInt();
        const bool hovered = !selected && hoveredRow == index.row();
        const ThemePalette& palette = currentPalette();
        const QColor rowDividerColor(color(palette.borderMuted));
        const QColor hoveredDividerColor(color(palette.border));
        const QColor selectedDividerColor(color(palette.accent));
        const QColor currentDividerColor(color(palette.success));
        const QColor selectedBackgroundColor(color(palette.serverSelectedBackground));
        const QColor currentBackgroundColor(color(palette.serverCurrentBackground));
        const bool currentServer = index.data(Qt::UserRole + 1).toBool();
        const bool currentServerMarkerCell = currentServer && index.column() == 0;
        const QIcon currentServerIcon = currentServerMarkerCell
            ? qvariant_cast<QIcon>(index.data(Qt::DecorationRole))
            : QIcon();
        const QColor baseColor = tableView == nullptr
            ? QColor(color(palette.surface))
            : tableView->palette().color(QPalette::Base);
        const QColor alternateBaseColor = tableView == nullptr
            ? QColor(color(palette.surfaceSubtle))
            : tableView->palette().color(QPalette::AlternateBase);
        const QColor bgColor = selected
            ? selectedBackgroundColor
            : (currentServer
                    ? currentBackgroundColor
                    : (hovered
                    ? QColor(color(palette.hover))
                    : (((index.row() % 2) != 0) ? alternateBaseColor : baseColor)));
        const QColor themeTextColor(color(palette.text));

        if (currentServer) {
            styledOption.font.setBold(true);
        }
        styledOption.palette.setColor(QPalette::Base, baseColor);
        styledOption.palette.setColor(QPalette::AlternateBase, alternateBaseColor);
        const QVariant foregroundData = index.data(Qt::ForegroundRole);
        if (currentServer) {
            const QColor successColor(color(palette.success));
            styledOption.palette.setColor(QPalette::Text, successColor);
            styledOption.palette.setColor(QPalette::WindowText, successColor);
            styledOption.palette.setColor(QPalette::HighlightedText, successColor);
        } else if (!foregroundData.isValid()) {
            styledOption.palette.setColor(QPalette::Text, themeTextColor);
            styledOption.palette.setColor(QPalette::WindowText, themeTextColor);
            styledOption.palette.setColor(QPalette::HighlightedText, themeTextColor);
        }
        styledOption.palette.setColor(QPalette::Highlight, selectedBackgroundColor);

        styledOption.state &= ~QStyle::State_HasFocus;
        styledOption.state &= ~QStyle::State_MouseOver;
        styledOption.state &= ~QStyle::State_Selected;
        styledOption.features.setFlag(QStyleOptionViewItem::Alternate, false);
        styledOption.backgroundBrush = QBrush(Qt::NoBrush);
        if (currentServerMarkerCell) {
            styledOption.text.clear();
            styledOption.icon = QIcon();
        }
        painter->fillRect(option.rect, bgColor);

        const QWidget* widget = styledOption.widget;
        QStyle* style = widget == nullptr ? QApplication::style() : widget->style();
        style->drawControl(QStyle::CE_ItemViewItem, &styledOption, painter, widget);

        painter->save();
        if (currentServerMarkerCell && !currentServerIcon.isNull()) {
            const int iconExtent = qMax(12, qMin(option.rect.width(), option.rect.height()) - 8);
            const QRect iconRect(
                option.rect.left() + ((option.rect.width() - iconExtent) / 2),
                option.rect.top() + ((option.rect.height() - iconExtent) / 2),
                iconExtent,
                iconExtent);
            currentServerIcon.paint(painter, iconRect, Qt::AlignCenter);
        }
        painter->setPen(QPen(selected ? selectedDividerColor : (currentServer ? currentDividerColor : (hovered ? hoveredDividerColor : rowDividerColor))));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        painter->restore();
    }
};

class ServerTableHoverTracker final : public QObject {
public:
    explicit ServerTableHoverTracker(QTableView* tableView)
        : QObject(tableView)
        , tableView_(tableView)
    {
        setObjectName(QString::fromLatin1(kServerTableHoverTrackerName));
        if (tableView_ != nullptr && tableView_->viewport() != nullptr) {
            tableView_->viewport()->installEventFilter(this);
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (tableView_ == nullptr
            || tableView_->viewport() == nullptr
            || watched != tableView_->viewport()
            || event == nullptr) {
            return QObject::eventFilter(watched, event);
        }

        switch (event->type()) {
        case QEvent::MouseMove: {
            const auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QModelIndex index = tableView_->indexAt(mouseEvent->pos());
            setHoveredRow(index.isValid() ? index.row() : -1);
            break;
        }
        case QEvent::Leave:
            setHoveredRow(-1);
            break;
        default:
            break;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void setHoveredRow(int row)
    {
        if (tableView_ == nullptr) {
            return;
        }

        const int previousRow = tableView_->property(kServerTableHoveredRowProperty).toInt();
        if (previousRow == row) {
            return;
        }

        tableView_->setProperty(kServerTableHoveredRowProperty, row);

        if (tableView_->model() == nullptr) {
            tableView_->viewport()->update();
            return;
        }

        const int columnCount = tableView_->model()->columnCount();
        for (int column = 0; column < columnCount; ++column) {
            if (previousRow >= 0) {
                tableView_->update(tableView_->model()->index(previousRow, column));
            }
            if (row >= 0) {
                tableView_->update(tableView_->model()->index(row, column));
            }
        }
    }

    QPointer<QTableView> tableView_;
};

} // namespace

void AppTheme::applyApplicationTheme(QApplication& app)
{
    applyApplicationTheme(app, QString::fromLatin1(kThemeLight));
}

void AppTheme::applyApplicationTheme(QApplication& app, QStringView themeName)
{
    currentThemeName = normalizeThemeName(themeName);
    applyApplicationPalette(app, currentPalette());
    app.setStyleSheet(loadThemeStyleSheet(currentThemeName));
    polishThemedWidgets(app);
}

void AppTheme::polishThemedWidgets(QApplication& app)
{
    const auto tables = app.allWidgets();
    for (QWidget* widget : tables) {
        auto* tableView = qobject_cast<QTableView*>(widget);
        if (tableView != nullptr && tableView->property(kServerTableStyleProperty).toBool()) {
            applyServerTableStyle(tableView);
        }
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
    if (tableView == nullptr) {
        return;
    }

    tableView->setProperty(kServerTableStyleProperty, true);
    tableView->setProperty(kServerTableHoveredRowProperty, -1);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->setMouseTracking(true);

    if (tableView->viewport() != nullptr) {
        tableView->viewport()->setMouseTracking(true);
    }

    const ThemePalette& theme = currentPalette();

    if (tableView->findChild<QObject*>(QString::fromLatin1(kServerTableHoverTrackerName)) == nullptr) {
        new ServerTableHoverTracker(tableView);
    }

    if (tableView->itemDelegate() == nullptr
        || tableView->itemDelegate()->objectName() != QString::fromLatin1(kServerTableStyleDelegateName)) {
        tableView->setItemDelegate(new ServerTableStyleDelegate(tableView));
    }

    tableView->style()->unpolish(tableView);
    tableView->style()->polish(tableView);

    QPalette palette = tableView->palette();
    palette.setColor(QPalette::Base, QColor(color(theme.surface)));
    palette.setColor(QPalette::AlternateBase, QColor(color(theme.surfaceSubtle)));
    palette.setColor(QPalette::Text, QColor(color(theme.text)));
    palette.setColor(QPalette::WindowText, QColor(color(theme.text)));
    palette.setColor(QPalette::HighlightedText, QColor(color(theme.text)));
    palette.setColor(QPalette::Highlight, QColor(color(theme.accentSoft)));
    tableView->setPalette(palette);
    if (tableView->viewport() != nullptr) {
        tableView->viewport()->setPalette(palette);
    }

    tableView->update();
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

QIcon AppTheme::themedSvgIcon(const QString& resourcePath)
{
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

    return QIcon(pixmap);
}

QString AppTheme::iconColor(bool enabled)
{
    const ThemePalette& palette = currentPalette();
    return enabled ? color(palette.text) : color(palette.textSubtle);
}
