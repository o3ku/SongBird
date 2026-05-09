#include "ui/theme/AppTheme.h"

#include <QApplication>
#include <QStringView>

namespace {

QString buildApplicationStyleSheet()
{
    return QStringLiteral(
        "QMainWindow, QDialog, QMessageBox { background: #f3f3f3; color: #1f1f1f; }"
        "QWidget { color: #1f1f1f; font-family: 'Microsoft YaHei'; }"
        "QToolBar#mainToolBar { background: #ffffff; border: none; border-bottom: 1px solid #d0d0d0; spacing: 0px; padding: 6px 8px; }"
        "QToolBar#mainToolBar::separator { width: 0px; margin: 0px; background: transparent; }"
        "QToolButton, QPushButton { background: #ffffff; border: 1px solid #bdbdbd; border-radius: 0px; padding: 4px 8px 4px 8px; color: #1f1f1f; }"
        "QToolButton:hover, QPushButton:hover { background: #efefef; border-color: #9f9f9f; }"
        "QToolButton:pressed, QToolButton:checked, QPushButton:pressed { background: #dddddd; border-color: #8f8f8f; }"
        "QToolBar#mainToolBar QToolButton { border: none; margin: 0px; }"
        "QToolBar#mainToolBar QToolButton:hover { border: none; }"
        "QToolBar#mainToolBar QToolButton:pressed, QToolBar#mainToolBar QToolButton:checked { border: none; }"
        "QToolBar#mainToolBar QComboBox { }"
        "QToolBar#mainToolBar QToolButton[toolbarMenuButton=\"true\"] { padding: 4px 12px 4px 8px; }"
        "QToolBar#mainToolBar QToolButton[toolbarMenuButton=\"true\"]::menu-indicator { image: none; width: 0px; }"
        "QToolBar#mainToolBar QToolButton#qrCodeButton { border: 1px solid #bdbdbd; padding: 4px 2px 4px 8px; }"
        "QToolBar#mainToolBar QToolButton#qrCodeButton:hover { border: 1px solid #9f9f9f; }"
        "QToolBar#mainToolBar QToolButton#qrCodeButton:pressed, QToolBar#mainToolBar QToolButton#qrCodeButton:checked { border: 1px solid #8f8f8f; }"
        "QDialogButtonBox QPushButton { min-width: 72px; }"
        "QComboBox, QLineEdit, QSpinBox, QListView, QTableView, QTableWidget, QMenu, QTabWidget::pane { background: #ffffff; border: 1px solid #c8c8c8; border-radius: 0px; }"
        "QTextEdit { background: #ffffff; border: 1px solid #c8c8c8; border-radius: 0px; }"
        "QComboBox, QLineEdit, QSpinBox { min-height: 28px; padding: 0 8px; }"
        "QComboBox:hover, QLineEdit:hover, QSpinBox:hover, QListView:hover, QTextEdit:hover { border-color: #a9a9a9; }"
        "QComboBox:focus, QLineEdit:focus, QSpinBox:focus, QListView:focus, QTextEdit:focus { border: 1px solid #7f7f7f; }"
        "QComboBox { padding-right: 28px; }"
        "QSpinBox { padding-right: 22px; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 18px; border: none; border-left: 1px solid #d0d0d0; background: #f0f0f0; border-radius: 0px; }"
        "QComboBox::drop-down:hover { background: #e6e6e6; }"
        "QComboBox::down-arrow { image: url(:/app/chevron-down.svg); width: 10px; height: 10px; }"
        "QComboBox[styledChevron=\"true\"]::down-arrow { image: none; width: 0px; height: 0px; }"
        "QSpinBox::up-button, QSpinBox::down-button { width: 18px; border: none; border-left: 1px solid #d0d0d0; background: #f0f0f0; border-radius: 0px; }"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #e6e6e6; }"
        "QSpinBox::up-arrow { image: url(:/app/chevron-up.svg); width: 10px; height: 10px; }"
        "QSpinBox::down-arrow { image: url(:/app/chevron-down.svg); width: 10px; height: 10px; }"
        "QAbstractItemView { selection-background-color: #dfdfdf; selection-color: #1f1f1f; outline: 0; }"
        "QAbstractItemView#comboPopupView { background: #ffffff; color: #1f1f1f; border: 1px solid #c8c8c8; border-radius: 0px; padding: 4px; outline: 0; }"
        "QAbstractItemView#comboPopupView::item { min-height: 24px; padding: 0px 12px; border-radius: 0px; }"
        "QAbstractItemView#comboPopupView::item:hover { background: #efefef; color: #1f1f1f; }"
        "QAbstractItemView#comboPopupView::item:selected { background: #dfdfdf; color: #1f1f1f; }"
        "QHeaderView::section { background: #f1f1f1; color: #444444; padding: 2px 8px; border: none; border-bottom: 1px solid #d0d0d0; border-right: 1px solid #e6e6e6; font-weight: 600; }"
        "QHeaderView::section:last { border-right: none; }"
        "QTableView#serverTableView { border: none; }"
        "QTableView#serverTableView QHeaderView::section { background: #ffffff; }"
        "QTabBar#subscriptionTabBar { background: transparent; }"
        "QTabBar#subscriptionTabBar::tab { background: #ececec; color: #4a4a4a; min-height: 26px; padding: 0 12px; margin-left: -1px; margin-right: 0px; margin-bottom: 0px; border: 1px solid #c8c8c8; border-bottom: none; border-radius: 0px; }"
        "QTabBar#subscriptionTabBar::tab:first { margin-left: 0px; border-left: none; }"
        "QTabBar#subscriptionTabBar::tab:selected { background: #ffffff; color: #1f1f1f; min-height: 26px; padding: 1px 12px; border-color: #c8c8c8; border-bottom: none; }"
        "QTabBar#subscriptionTabBar::tab:hover:!selected { background: #e2e2e2; color: #1f1f1f; }"
        "#serverHeaderRow, QWidget#serverFilterContainer { background: #ffffff; }"
        "#serverHeaderRow { border-bottom: 1px solid #d6d6d6; }"
        "QLineEdit#serverFilterEdit, QLineEdit#logFilterEdit { background: #ffffff; border: 1px solid #c8c8c8; border-radius: 0px; padding: 0 8px; }"
        "QLineEdit#serverFilterEdit:hover, QLineEdit#logFilterEdit:hover { background: #ffffff; border: 1px solid #a9a9a9; }"
        "QLineEdit#serverFilterEdit:focus, QLineEdit#logFilterEdit:focus { background: #ffffff; border: 1px solid #7f7f7f; }"
        "#logHeaderRow { background: transparent; border-top: 1px solid #d6d6d6; }"
        "#logPanel, #qrPanel { background: transparent; }"
        "QListView#logView { background: #ffffff; border: none; border-radius: 0px; padding: 0px; }"
        "QLabel#logTitleLabel { color: #4b4b4b; font-weight: 600; }"
        "QLabel#aboutTitleLabel { font-size: 22px; font-weight: 700; }"
        "QLabel#qrPlaceholder { border: 1px dashed #bdbdbd; background: #ffffff; color: #7a7a7a; border-radius: 0px; }"
        "QWidget#loadingOverlay { background: rgba(255,255,255,200); }"
        "QStatusBar { background: #ffffff; border-top: 1px solid #d0d0d0; }"
        "QStatusBar QLabel { color: #5b5b5b; padding: 0 4px; }"
        "QStatusBar QLabel#routingStatusLabel { color: #4d4d4d; font-weight: 400; }"
        "QLabel[semanticState=\"#2f5d3a\"] { color: #2f5d3a; font-weight: 600; }"
        "QLabel[semanticState=\"#7a6330\"] { color: #7a6330; font-weight: 600; }"
        "QLabel[semanticState=\"#7a3434\"] { color: #7a3434; font-weight: 600; }"
        "QLabel[semanticState=\"#4d4d4d\"] { color: #4d4d4d; font-weight: 600; }"
        "QSplitter::handle { background: #ffffff; }"
        "QSplitter::handle:horizontal { width: 2px; margin: 0px; }"
        "QSplitter::handle:vertical { height: 2px; margin: 0px; }"
        "QMenu { background: #ffffff; color: #1f1f1f; border: 1px solid #c8c8c8; border-radius: 0px; padding: 4px; }"
        "QMenu::item { padding: 4px 20px; border-radius: 0px; margin: 0px; }"
        "QMenu::item:selected { background: #dfdfdf; color: #1f1f1f; }"
        "QMenu::separator { height: 1px; background: #e0e0e0; margin: 4px 0px; }"
        "QCheckBox { color: #4b4b4b; }"
        "QPushButton[baseRouteCard=\"true\"] { text-align: left; padding: 8px; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 6px 2px 6px 0; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; margin: 0 6px 2px 6px; }"
        "QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #b8b8b8; min-height: 24px; min-width: 24px; border-radius: 0px; }"
        "QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background: #9e9e9e; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical, QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal, QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; width: 0px; height: 0px; }"
        "QLineEdit[validationState=\"error\"] { border: 1px solid #8a3d3d; }");
}

} // namespace

void AppTheme::applyApplicationTheme(QApplication& app)
{
    app.setStyleSheet(buildApplicationStyleSheet());
}

QString AppTheme::semanticStatusProperty(QStringView colorHex)
{
    return colorHex.toString();
}

QString AppTheme::successStatusColor()
{
    return QStringLiteral("#2f5d3a");
}

QString AppTheme::warningStatusColor()
{
    return QStringLiteral("#7a6330");
}

QString AppTheme::errorStatusColor()
{
    return QStringLiteral("#7a3434");
}
