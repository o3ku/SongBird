#include "ui/mainwindow/LogItemDelegate.h"

#include "ui/models/LogListModel.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>

LogItemDelegate::LogItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize LogItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const int lineCount = index.data(LogListModel::LineCountRole).toInt();
    const int h = option.fontMetrics.height() * qMax(1, lineCount) + VerticalPadding * 2;
    return QSize(0, h);
}

void LogItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    opt.text.clear();
    const bool selected = (opt.state & QStyle::State_Selected) != 0;
    const bool hovered = !selected && (opt.state & QStyle::State_MouseOver) != 0;
    opt.state &= ~QStyle::State_MouseOver;
    opt.state &= ~QStyle::State_Selected;

    const QWidget* widget = opt.widget;
    QStyle* style = widget == nullptr ? QApplication::style() : widget->style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

    if (selected || hovered) {
        painter->fillRect(
            option.rect,
            selected ? QColor(QStringLiteral("#c5dbfe")) : QColor(QStringLiteral("#f2f6fc")));
    }

    const QStringList visualLines = index.data(LogListModel::VisualLinesRole).toStringList();
    if (visualLines.isEmpty()) {
        return;
    }

    const int lineH = opt.fontMetrics.height();
    const int ascent = opt.fontMetrics.ascent();
    const int x = option.rect.left() + HorizontalPadding;
    int y = option.rect.top() + VerticalPadding;

    painter->setPen(opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));

    for (const QString& line : visualLines) {
        painter->drawText(x, y + ascent, line);
        y += lineH;
    }
}
