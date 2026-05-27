#pragma once

class QAction;
class QComboBox;
class QIcon;
class QMenu;
class QString;
class QToolBar;
class QToolButton;
class QWidget;

namespace ToolbarWidgets {

constexpr int ControlSpacing = 2;
constexpr int ControlHeight = 28;
constexpr int Padding = 4;
constexpr int BottomBorderWidth = 1;

QIcon loadIcon(const QString& fileName);
QComboBox* createStyledComboBox(QWidget* parent);
void configureStyledComboBox(QComboBox* comboBox);
QToolButton* createMenuButton(
    QToolBar* toolBar,
    const QString& objectName,
    const QString& text,
    const QIcon& icon,
    QMenu* menu);
QToolButton* createActionButton(
    QToolBar* toolBar,
    const QString& objectName,
    const QString& text,
    const QIcon& icon,
    QAction* action);
QWidget* createSpacing(QWidget* parent, int width);
void updateContentSizedComboBox(QComboBox* comboBox, int minimumCharacters);

} // namespace ToolbarWidgets
