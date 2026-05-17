#include "ui/mainwindow/LogItemDelegate.h"

#include "ui/models/LogListModel.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QTextLayout>
#include <QTextOption>

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
            selected ? QColor(QStringLiteral("#eceff3")) : QColor(QStringLiteral("#f2f6fc")));
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty()) {
        return;
    }

    const qreal availableWidth = qMax(0, option.rect.width() - 2 * HorizontalPadding);
    if (availableWidth <= 0) {
        return;
    }

    QTextLayout layout(text, opt.font);
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(textOption);
    layout.beginLayout();
    qreal y = 0.0;
    forever {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(availableWidth);
        line.setPosition(QPointF(0.0, y));
        y += line.height();
    }
    layout.endLayout();

    painter->save();
    painter->setPen(opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
    painter->translate(option.rect.left() + HorizontalPadding, option.rect.top() + VerticalPadding);
    layout.draw(painter, QPointF(0.0, 0.0));
    painter->restore();
}
