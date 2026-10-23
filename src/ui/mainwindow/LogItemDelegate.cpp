#include "ui/mainwindow/LogItemDelegate.h"

#include "ui/models/LogListModel.h"
#include "ui/theme/AppTheme.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QTextLayout>
#include <QTextOption>
#include <QWidget>

LogItemDelegate::LogItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void LogItemDelegate::setView(const QWidget* view)
{
    view_ = view;
}

QSize LogItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // The height of the layout paint() will draw, not the line count LogListModel keeps.
    //
    // Two reasons. The model counts lines against the width it was last told about, which is
    // the viewport width minus HorizontalPadding -- in a QListView that is not the width a row
    // is laid out at, because the view reserves room for a vertical scrollbar. A row that
    // paint() wrapped onto two lines therefore reported one, and its second line was clipped.
    // And that count is only as fresh as the last setWrapConfiguration() call, which is posted
    // through a queued invocation, so a row appended before it is delivered is measured against
    // the previous width.
    //
    // The height is measured with the metrics of the widget the delegate draws on, not with
    // QFontMetrics built from option.font. Those are not the same number for the same font:
    // a QFont that has not been resolved yet resolves differently depending on which object
    // asks first, and the option's copy of the view's font reports a smaller line than the
    // view's own fontMetrics() does (measured under the offscreen plugin: 19 against 21 tall
    // for one 12pt font). The view's answer is the one the row is actually drawn in.
    const QWidget* owner = view_ != nullptr ? view_ : option.widget;
    const int lineHeight = owner != nullptr ? owner->fontMetrics().height() : option.fontMetrics.height();
    return QSize(0, layoutHeight(index.data(Qt::DisplayRole).toString(), option, option.rect.width(), lineHeight) + VerticalPadding * 2);
}

int LogItemDelegate::layoutHeight(const QString& text, const QStyleOptionViewItem& option, int rowWidth, int lineHeight)
{
    const qreal availableWidth = qMax(0, rowWidth - 2 * HorizontalPadding);
    if (text.isEmpty() || availableWidth <= 0) {
        return lineHeight;
    }

    QTextLayout layout(text, option.font);
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(textOption);
    layout.beginLayout();

    int lineCount = 0;
    forever {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(availableWidth);
        ++lineCount;
    }
    layout.endLayout();

    return lineHeight * qMax(1, lineCount);
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
            selected ? QColor(AppTheme::selectionBackgroundColor()) : QColor(AppTheme::hoverBackgroundColor()));
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
    painter->setPen(opt.palette.color(QPalette::Text));
    painter->translate(option.rect.left() + HorizontalPadding, option.rect.top() + VerticalPadding);
    layout.draw(painter, QPointF(0.0, 0.0));
    painter->restore();
}
