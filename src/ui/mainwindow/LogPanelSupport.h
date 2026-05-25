#pragma once

#include <QString>
#include <QStringList>

class QAbstractItemModel;
class QLineEdit;
class QListView;

namespace LogPanelSupport {

constexpr int HeaderFilterMinimumCharacters = 14;

void configureContentSizedLineEdit(QLineEdit* edit, int minimumCharacters);
void setLineEditValidationState(QLineEdit* edit, const QString& value);
bool viewAtBottom(const QListView* view);
bool hasSelectedRows(const QListView* view);
int selectedRowCount(const QListView* view);
QStringList selectedRowsText(const QListView* view);
QStringList modelRowsText(const QAbstractItemModel* model);
void selectAllRows(QListView* view, QAbstractItemModel* model);

} // namespace LogPanelSupport
