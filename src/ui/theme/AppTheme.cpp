#include "ui/theme/AppTheme.h"

#include <QApplication>
#include <QAbstractItemModel>
#include <QEvent>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QIODevice>
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
    const char* surfaceMuted;
    const char* surfaceSubtle;
    const char* border;
    const char* borderMuted;
    const char* borderStrong;
    const char* text;
    const char* textMuted;
    const char* textSubtle;
    const char* accent;
    const char* accentSoft;
    const char* accentHover;
    const char* hover;
    const char* success;
    const char* warning;
    const char* error;
    const char* neutral;
    const char* validationError;
    const char* overlay;
};

constexpr ThemePalette kLightPalette{
    "#f3f5f8",
    "#ffffff",
    "#eef2f6",
    "#f8fafc",
    "#c5ced8",
    "#dde3ea",
    "#8b98a8",
    "#1f2328",
    "#667085",
    "#8a94a6",
    "#2f6d56",
    "#e5f0eb",
    "#d8e8df",
    "#eef4f1",
    "#2f6d45",
    "#8a6a20",
    "#a33a3a",
    "#4d5f73",
    "#a33a3a",
    "rgba(255, 255, 255, 224)"};

constexpr ThemePalette kDarkPalette{
    "#1a1f25",
    "#20262d",
    "#252d35",
    "#1d232a",
    "#414b56",
    "#36404a",
    "#657384",
    "#e6eaf0",
    "#aab4c1",
    "#778392",
    "#58a07e",
    "#20372d",
    "#2d4c3f",
    "#202b25",
    "#77c58d",
    "#d1a94d",
    "#e06f6f",
    "#b8c4d2",
    "#e06f6f",
    "rgba(26, 31, 37, 224)"};

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

QString loadThemeStyleSheet(const QString& themeName)
{
    QFile file(QStringLiteral(":/themes/%1.qss").arg(themeName));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(file.readAll());
    }

    if (themeName != QString::fromLatin1(kThemeLight)) {
        return loadThemeStyleSheet(QString::fromLatin1(kThemeLight));
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
        const QColor selectedBackgroundColor = currentThemeName == QString::fromLatin1(kThemeDark)
            ? QColor(QStringLiteral("#3b765d"))
            : QColor(QStringLiteral("#b7dec9"));
        const QColor currentBackgroundColor = currentThemeName == QString::fromLatin1(kThemeDark)
            ? QColor(QStringLiteral("#253d33"))
            : QColor(QStringLiteral("#e1f3e9"));
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
    app.setStyleSheet(loadThemeStyleSheet(currentThemeName));

    const auto tables = app.allWidgets();
    for (QWidget* widget : tables) {
        auto* tableView = qobject_cast<QTableView*>(widget);
        if (tableView != nullptr && tableView->property(kServerTableStyleProperty).toBool()) {
            applyServerTableStyle(tableView);
        }
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
    return color(currentPalette().accentSoft);
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

QString AppTheme::semanticStatusProperty(QStringView colorHex)
{
    return colorHex.toString();
}

QString AppTheme::successStatusColor()
{
    return color(currentPalette().success);
}

QString AppTheme::warningStatusColor()
{
    return color(currentPalette().warning);
}

QString AppTheme::errorStatusColor()
{
    return color(currentPalette().error);
}
