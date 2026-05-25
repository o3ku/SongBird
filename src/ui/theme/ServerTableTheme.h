#pragma once

class QTableView;
class QWidget;

namespace ServerTableTheme {

void apply(QTableView* tableView);
void applyIfStyled(QWidget* widget);

} // namespace ServerTableTheme
