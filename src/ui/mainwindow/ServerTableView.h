#pragma once

#include <QList>
#include <QTableView>

#include <functional>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QEvent;
class QMouseEvent;

class ServerTableView final : public QTableView {
public:
    explicit ServerTableView(QWidget* parent = nullptr);

    void setRowsReorderEnabled(bool enabled);
    bool rowsReorderEnabled() const;
    void setRowsMoveHandler(std::function<void(const QList<int>& rows, int targetRow)> handler);
    bool moveSelectedRowsTo(int targetRow);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int resolveDropRow(const QPoint& viewportPosition) const;
    bool requestMoveSelectedRows(int targetRow);

    std::function<void(const QList<int>& rows, int targetRow)> rowsMoveHandler_;
    bool rowsReorderEnabled_ = true;
};
