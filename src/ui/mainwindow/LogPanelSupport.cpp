#include "ui/mainwindow/LogPanelSupport.h"

#include <QAbstractItemModel>
#include <QFontMetrics>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStyle>

namespace {

int textControlMinimumWidth(const QWidget* widget, const QString& text, int minimumCharacters, int chromeWidth)
{
    if (widget == nullptr) {
        return 0;
    }

    const QFontMetrics metrics(widget->font());
    const int textWidth = metrics.horizontalAdvance(text);
    const int characterWidth = metrics.horizontalAdvance(QString(minimumCharacters, QLatin1Char('M')));
    return qMax(textWidth, characterWidth) + chromeWidth;
}

} // namespace

namespace LogPanelSupport {

void configureContentSizedLineEdit(QLineEdit* edit, int minimumCharacters)
{
    if (edit == nullptr) {
        return;
    }

    edit->setMinimumWidth(textControlMinimumWidth(edit, edit->placeholderText(), minimumCharacters, 40));
    edit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void setLineEditValidationState(QLineEdit* edit, const QString& value)
{
    if (edit == nullptr) {
        return;
    }

    edit->setProperty("validationState", value);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
    edit->update();
}

bool viewAtBottom(const QListView* view)
{
    if (view == nullptr) {
        return false;
    }

    QScrollBar* verticalScrollBar = view->verticalScrollBar();
    return verticalScrollBar != nullptr && verticalScrollBar->value() >= verticalScrollBar->maximum();
}

bool hasSelectedRows(const QListView* view)
{
    return selectedRowCount(view) > 0;
}

int selectedRowCount(const QListView* view)
{
    return view == nullptr || view->selectionModel() == nullptr
        ? 0
        : view->selectionModel()->selectedRows().size();
}

QStringList selectedRowsText(const QListView* view)
{
    if (view == nullptr || view->selectionModel() == nullptr) {
        return {};
    }

    QStringList lines;
    const QModelIndexList proxyIndexes = view->selectionModel()->selectedRows();
    for (const QModelIndex& proxyIndex : proxyIndexes) {
        lines.append(proxyIndex.data(Qt::DisplayRole).toString());
    }
    return lines;
}

QStringList modelRowsText(const QAbstractItemModel* model)
{
    if (model == nullptr) {
        return {};
    }

    QStringList lines;
    for (int row = 0; row < model->rowCount(); ++row) {
        lines.append(model->index(row, 0).data(Qt::DisplayRole).toString());
    }
    return lines;
}

void selectAllRows(QListView* view, QAbstractItemModel* model)
{
    if (view == nullptr || model == nullptr || view->selectionModel() == nullptr) {
        return;
    }

    const QModelIndex topLeft = model->index(0, 0);
    const QModelIndex bottomRight = model->index(model->rowCount() - 1, 0);
    if (!topLeft.isValid() || !bottomRight.isValid()) {
        return;
    }

    view->selectionModel()->select(
        QItemSelection(topLeft, bottomRight),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

} // namespace LogPanelSupport
