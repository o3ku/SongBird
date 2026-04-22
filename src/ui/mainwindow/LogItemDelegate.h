#pragma once

#include <QStyledItemDelegate>

class LogItemDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit LogItemDelegate(QObject* parent = nullptr);

    static constexpr int HorizontalPadding = 8;
    static constexpr int VerticalPadding = 1;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
