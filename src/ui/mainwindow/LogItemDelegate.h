#pragma once

#include <QSize>
#include <QStyledItemDelegate>

class LogItemDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit LogItemDelegate(QObject* parent = nullptr);

    // The view the rows belong to. sizeHint() asks it for the font metrics it draws in, which
    // are not the ones a QFontMetrics built from the option's own font reports -- see the note
    // in sizeHint().
    void setView(const QWidget* view);

    static constexpr int HorizontalPadding = 8;
    static constexpr int VerticalPadding = 1;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    // The height paint() will draw `text` into at `rowWidth`, computed by wrapping the text
    // exactly the way paint() wraps it. sizeHint() cannot ask paint() for the number, so the
    // layout is built once by each and the arithmetic lives here for both to agree on.
    static int layoutHeight(const QString& text, const QStyleOptionViewItem& option, int rowWidth, int lineHeight);

    const QWidget* view_ = nullptr;
};
