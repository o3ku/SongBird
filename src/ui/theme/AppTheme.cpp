#include "ui/theme/AppTheme.h"

#include <QApplication>
#include <QAbstractItemModel>
#include <QEvent>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QStringView>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QWidget>

namespace {

constexpr auto kServerTableStyleProperty = "serverTableStyle";
constexpr auto kServerTableHoveredRowProperty = "serverTableHoveredRow";
constexpr auto kServerTableHoverTrackerName = "serverTableHoverTracker";
constexpr auto kServerTableStyleDelegateName = "serverTableStyleDelegate";

class ServerTableStyleDelegate final : public QStyledItemDelegate {
public:
    explicit ServerTableStyleDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        setObjectName(QString::fromLatin1(kServerTableStyleDelegateName));
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem styledOption(option);
        initStyleOption(&styledOption, index);

        const auto* tableView = qobject_cast<const QTableView*>(parent());
        const bool selected = (styledOption.state & QStyle::State_Selected) != 0;
        const int hoveredRow = tableView == nullptr
            ? -1
            : tableView->property(kServerTableHoveredRowProperty).toInt();
        const bool hovered = !selected && hoveredRow == index.row();
        const QColor rowDividerColor(QStringLiteral("#e1e7ee"));
        const QColor hoveredDividerColor(QStringLiteral("#9babc5"));
        const QColor selectedDividerColor(QStringLiteral("#b0c4e3"));
        const QColor baseColor = tableView == nullptr
            ? QColor(QStringLiteral("#ffffff"))
            : tableView->palette().color(QPalette::Base);
        const QColor alternateBaseColor = tableView == nullptr
            ? QColor(QStringLiteral("#f9f9f9"))
            : tableView->palette().color(QPalette::AlternateBase);
        const QColor bgColor = selected
            ? QColor(QStringLiteral("#c5dbfe"))
            : (hovered
                    ? QColor(QStringLiteral("#f2f6fc"))
                    : (((index.row() % 2) != 0) ? alternateBaseColor : baseColor));

        styledOption.state &= ~QStyle::State_HasFocus;
        styledOption.state &= ~QStyle::State_MouseOver;
        styledOption.state &= ~QStyle::State_Selected;
        styledOption.backgroundBrush = QBrush(bgColor);
        painter->fillRect(option.rect, bgColor);

        const QWidget* widget = styledOption.widget;
        QStyle* style = widget == nullptr ? QApplication::style() : widget->style();
        style->drawControl(QStyle::CE_ItemViewItem, &styledOption, painter, widget);

        painter->save();
        painter->setPen(QPen(selected ? selectedDividerColor : (hovered ? hoveredDividerColor : rowDividerColor)));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        painter->restore();
    }
};

class ServerTableHoverTracker final : public QObject {
public:
    explicit ServerTableHoverTracker(QTableView* tableView)
        : QObject(tableView)
        , tableView_(tableView)
    {
        setObjectName(QString::fromLatin1(kServerTableHoverTrackerName));
        if (tableView_ != nullptr && tableView_->viewport() != nullptr) {
            tableView_->viewport()->installEventFilter(this);
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (tableView_ == nullptr
            || tableView_->viewport() == nullptr
            || watched != tableView_->viewport()
            || event == nullptr) {
            return QObject::eventFilter(watched, event);
        }

        switch (event->type()) {
        case QEvent::MouseMove: {
            const auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QModelIndex index = tableView_->indexAt(mouseEvent->pos());
            setHoveredRow(index.isValid() ? index.row() : -1);
            break;
        }
        case QEvent::Leave:
            setHoveredRow(-1);
            break;
        default:
            break;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void setHoveredRow(int row)
    {
        if (tableView_ == nullptr) {
            return;
        }

        const int previousRow = tableView_->property(kServerTableHoveredRowProperty).toInt();
        if (previousRow == row) {
            return;
        }

        tableView_->setProperty(kServerTableHoveredRowProperty, row);

        if (tableView_->model() == nullptr) {
            tableView_->viewport()->update();
            return;
        }

        const int columnCount = tableView_->model()->columnCount();
        for (int column = 0; column < columnCount; ++column) {
            if (previousRow >= 0) {
                tableView_->update(tableView_->model()->index(previousRow, column));
            }
            if (row >= 0) {
                tableView_->update(tableView_->model()->index(row, column));
            }
        }
    }

    QPointer<QTableView> tableView_;
};

QString buildApplicationStyleSheet()
{
    return QStringLiteral(
        "QMainWindow { background: #ffffff; color: #1f1f1f; }"
        "QDialog, QMessageBox { background: #ffffff; color: #1f1f1f; }"
        "QWidget { color: #1f1f1f; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; font-size: 9pt; }"
        "QToolBar#mainToolBar { background: #ffffff; border: none; border-bottom: 1px solid #222222; spacing: 0px; }"
        "QToolBar#mainToolBar::separator { width: 0px; margin: 0px; background: transparent; }"
        "QToolButton, QPushButton { background: #ffffff; border: 1px solid #bdbdbd; border-radius: 0px; padding: 4px 8px 4px 8px; color: #1f1f1f; }"
        "QPushButton { min-width: 51px; }"
        "QToolButton:hover, QPushButton:hover { background: #efefef; border-color: #9f9f9f; }"
        "QToolButton:pressed, QToolButton:checked, QPushButton:pressed { background: #dddddd; border-color: #8f8f8f; }"
        "QDialogButtonBox QPushButton, QMessageBox QPushButton, QFileDialog QPushButton { background: #ffffff; border: 1px solid #bdbdbd; border-radius: 0px; padding: 4px 8px 4px 8px; color: #1f1f1f; }"
        "QDialogButtonBox QPushButton:hover, QMessageBox QPushButton:hover, QFileDialog QPushButton:hover { background: #efefef; border-color: #9f9f9f; }"
        "QDialogButtonBox QPushButton:pressed, QMessageBox QPushButton:pressed, QFileDialog QPushButton:pressed { background: #dddddd; border-color: #8f8f8f; }"
        "QDialogButtonBox QPushButton:default, QMessageBox QPushButton:default, QFileDialog QPushButton:default { background: #ffffff; border: 1px solid #bdbdbd; }"
        "QDialogButtonBox QPushButton:focus, QMessageBox QPushButton:focus, QFileDialog QPushButton:focus { border: 1px solid #9f9f9f; outline: none; }"
        "QPushButton:default { background: #ffffff; border: 1px solid #bdbdbd; }"
        "QPushButton:focus { border: 1px solid #9f9f9f; outline: none; }"
        "QToolBar#mainToolBar QToolButton { border: none; margin: 0px; }"
        "QToolBar#mainToolBar QToolButton:hover { background: #ffffff; border-color: #000000; }"
        "QToolBar#mainToolBar QToolButton:pressed { background: #ffffff; border-color: #000000; }"
        "QToolBar#mainToolBar QToolButton:checked { border: none; }"
        "QToolBar#mainToolBar QComboBox { }"
        "QToolBar#mainToolBar QComboBox#routingModeCombo { border: 1px solid #bdbdbd; padding: 5px 4px 4px 8px; }"
        "QToolBar#mainToolBar QComboBox:hover { background: #ffffff; border-color: #000000; }"
        "QToolBar#mainToolBar QToolButton#serverButton, QToolBar#mainToolBar QToolButton#subButton, QToolBar#mainToolBar QToolButton#settingButton, QToolBar#mainToolBar QToolButton#updateButton, QToolBar#mainToolBar QToolButton#helpButton { border: 1px solid transparent; }"
        "QToolBar#mainToolBar QComboBox#routingModeCombo:hover { background: #ffffff; border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton#serverButton:hover, QToolBar#mainToolBar QToolButton#subButton:hover, QToolBar#mainToolBar QToolButton#settingButton:hover, QToolBar#mainToolBar QToolButton#updateButton:hover, QToolBar#mainToolBar QToolButton#helpButton:hover { background: #ffffff; border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton#serverButton:pressed, QToolBar#mainToolBar QToolButton#subButton:pressed, QToolBar#mainToolBar QToolButton#settingButton:pressed, QToolBar#mainToolBar QToolButton#updateButton:pressed, QToolBar#mainToolBar QToolButton#helpButton:pressed { border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton[toolbarMenuButton=\"true\"]::menu-indicator { image: none; }"
        "QToolBar#mainToolBar QToolButton#coreToggleButton, QToolBar#mainToolBar QToolButton#proxyToggleButton { border: 1px solid #bdbdbd; }"
        "QToolBar#mainToolBar QToolButton#coreToggleButton:hover, QToolBar#mainToolBar QToolButton#proxyToggleButton:hover { background: #ffffff; border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton#coreToggleButton:pressed, QToolBar#mainToolBar QToolButton#proxyToggleButton:pressed { border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton#coreToggleButton:checked, QToolBar#mainToolBar QToolButton#proxyToggleButton:checked { background: #1f1f1f; color: #ffffff; border: 1px solid #1f1f1f; }"
        "QToolBar#mainToolBar QToolButton#coreToggleButton:checked:hover, QToolBar#mainToolBar QToolButton#proxyToggleButton:checked:hover { border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton#coreToggleButton:disabled, QToolBar#mainToolBar QToolButton#proxyToggleButton:disabled { color: #8f8f8f; border-color: #d0d0d0; }"
        "QToolBar#mainToolBar QToolButton#qrCodeButton { border: 1px solid #bdbdbd; }"
        "QToolBar#mainToolBar QToolButton#qrCodeButton:hover { border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton#qrCodeButton:pressed { border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton#qrCodeButton:checked { background: transparent; border: 1px solid #000000; }"
        "QToolBar#mainToolBar QToolButton#qrCodeButton:checked:hover { background: transparent; border: 1px solid #000000; }"
        "QComboBox, QLineEdit, QSpinBox, QListView, QTableView, QTableWidget, QMenu, QTabWidget::pane { background: #ffffff; border: 1px solid #c8c8c8; border-radius: 0px; }"
        "QTextEdit { background: #ffffff; border: 1px solid #c8c8c8; border-radius: 0px; }"
        "QComboBox, QLineEdit, QSpinBox { padding: 2px 8px; }"
        "QComboBox:hover, QLineEdit:hover, QSpinBox:hover, QListView:hover, QTextEdit:hover { border-color: #a9a9a9; }"
        "QComboBox:focus, QLineEdit:focus, QSpinBox:focus, QListView:focus, QTextEdit:focus { border: 1px solid #7f7f7f; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 18px; border: none; border-left: 1px solid #d0d0d0; background: #f0f0f0; border-radius: 0px; }"
        "QComboBox::drop-down:hover { background: #e6e6e6; }"
        "QComboBox::down-arrow { image: url(:/app/chevron-down.svg); width: 10px; height: 10px; }"
        "QComboBox[styledChevron=\"true\"]::down-arrow { image: none; width: 0px; height: 0px; }"
        "QSpinBox::up-button, QSpinBox::down-button { width: 18px; border: none; border-left: 1px solid #d0d0d0; background: #f0f0f0; border-radius: 0px; }"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #e6e6e6; }"
        "QSpinBox::up-arrow { image: url(:/app/chevron-up.svg); width: 10px; height: 10px; }"
        "QSpinBox::down-arrow { image: url(:/app/chevron-down.svg); width: 10px; height: 10px; }"
        "QAbstractItemView { selection-background-color: #c5dbfe; selection-color: #1f1f1f; outline: 0; }"
        "QAbstractItemView#comboPopupView { background: #ffffff; color: #1f1f1f; border: 1px solid #c8c8c8; border-radius: 0px; padding: 4px; outline: 0; }"
        "QAbstractItemView#comboPopupView::item { min-height: 24px; padding: 0px 12px; border-radius: 0px; }"
        "QAbstractItemView#comboPopupView::item:hover { background: #f2f6fc; color: #1f1f1f; }"
        "QAbstractItemView#comboPopupView::item:selected { background: #c5dbfe; color: #1f1f1f; }"
        "QHeaderView::section { background: #f1f1f1; color: #444444; padding: 2px 8px; border: none; border-bottom: 1px solid #d0d0d0; border-right: 1px solid #e6e6e6; font-weight: 600; }"
        "QHeaderView::section:last { border-right: none; }"
        "QTableView[serverTableStyle=\"true\"], QTableWidget[serverTableStyle=\"true\"] { border: 1px solid #c8c8c8; font-size: 9pt; }"
        "QTableView[serverTableStyle=\"true\"] QHeaderView::section, QTableWidget[serverTableStyle=\"true\"] QHeaderView::section { background: #ffffff; }"
        "QTableView#serverTableView[serverTableStyle=\"true\"] { border: none; }"
        "QSplitter#rootSplitter { background: #ffffff; }"
        "QWidget#settingsStackContainer { background: #ffffff; border: none; }"
        "QWidget#settingsTabBarContainer, QWidget#subscriptionTabBarContainer { background: transparent; border: none; border-bottom: 1px solid #000000; }"
        "QWidget#settingsActionBar { background: #ffffff; border: none; border-top: 1px solid #000000; }"
        "QTabWidget#routingCustomRuleTabs::pane { background: #ffffff; border: none; }"
        "QTabBar#settingsTabBar, QTabBar#subscriptionTabBar, QTabWidget#routingCustomRuleTabs QTabBar { background: transparent; border: none; }"
        "QTabBar#settingsTabBar::tab, QTabBar#subscriptionTabBar::tab, QTabWidget#routingCustomRuleTabs QTabBar::tab { background: transparent; color: #808080; font-weight: 600; padding: 6px 18px; margin: 0px 0px 1px 0px; border: none; }"
        "QTabBar#settingsTabBar::tab:selected, QTabBar#subscriptionTabBar::tab:selected, QTabWidget#routingCustomRuleTabs QTabBar::tab:selected { background: #111111; color: #ffffff; }"
        "QTabBar#settingsTabBar::tab:hover:!selected, QTabBar#subscriptionTabBar::tab:hover:!selected, QTabWidget#routingCustomRuleTabs QTabBar::tab:hover:!selected { background: #dddddd; color: #222222; }"
        "#serverHeaderRow { background: #f4f4f4; }"
        "#serverHeaderRow { border-bottom: 1px solid #d0d0d0; }"
        "QLineEdit#serverFilterEdit, QLineEdit#logFilterEdit { background: #ffffff; border: 1px solid #c8c8c8; border-radius: 0px; padding: 2px 8px; }"
        "QLineEdit#serverFilterEdit:hover, QLineEdit#logFilterEdit:hover { background: #ffffff; border: 1px solid #a9a9a9; }"
        "QLineEdit#serverFilterEdit:focus, QLineEdit#logFilterEdit:focus { background: #ffffff; border: 1px solid #7f7f7f; }"
        "#logHeaderRow { background: #f4f4f4; border: none; border-bottom: 1px solid #d0d0d0; }"
        "QToolButton#logStickToBottomButton { background: transparent; border: 1px solid #bdbdbd; padding: 0px; }"
        "QToolButton#logStickToBottomButton:hover { background: transparent; border: 1px solid #000000; }"
        "QToolButton#logStickToBottomButton:pressed { background: transparent; border: 1px solid #000000; }"
        "QToolButton#logStickToBottomButton:checked { background: transparent; border: 1px solid #000000; }"
        "QToolButton#logStickToBottomButton:checked:hover { background: transparent; border: 1px solid #000000; }"
        "#logPanel { background: transparent; }"
        "#qrPanel { background: #ffffff; }"
        "QWidget#shareContentPanel, QPlainTextEdit#shareLinkLabel { background: #ffffff; }"
        "QListView#logView { background: #ffffff; border: none; border-radius: 0px; padding: 0px; }"
        "QLabel#logTitleLabel { color: #444444; font-weight: 600; }"
        "QLabel#aboutTitleLabel { font-size: 22px; font-weight: 700; }"
        "QLabel#qrPlaceholder { border: none; background: #ffffff; color: #7a7a7a; border-radius: 0px; }"
        "QPlainTextEdit#shareLinkLabel { border: none; color: #4b4b4b; padding: 0px; border-radius: 0px; }"
        "QWidget#loadingOverlay { background: rgba(255,255,255,200); }"
        "QStatusBar { background: #f4f4f4; border-top: 1px solid #d0d0d0; font-size: 9pt; }"
        "QStatusBar QLabel { color: #5b5b5b; padding: 0 4px; font-size: 9pt; }"
        "QStatusBar QLabel#routingStatusLabel { color: #4d4d4d; font-weight: 400; }"
        "QLabel[semanticState=\"#2f5d3a\"] { color: #2f5d3a; font-weight: 600; }"
        "QLabel[semanticState=\"#7a6330\"] { color: #7a6330; font-weight: 600; }"
        "QLabel[semanticState=\"#7a3434\"] { color: #7a3434; font-weight: 600; }"
        "QLabel[semanticState=\"#4d4d4d\"] { color: #4d4d4d; font-weight: 600; }"
        "QSplitter::handle { background: #ffffff; }"
        "QSplitter::handle:horizontal { width: 2px; margin: 0px; }"
        "QSplitter::handle:vertical { height: 2px; margin: 0px; }"
        "QSplitter#rootSplitter::handle { background: #999999; }"
        "QSplitter#rootSplitter::handle:vertical { height: 1px; margin: 0px; }"
        "QSplitter#topSplitter::handle { background: #d0d0d0; }"
        "QSplitter#topSplitter::handle:hover { background: #b8b8b8; }"
        "QSplitter#topSplitter::handle:horizontal { width: 3px; margin: 0px; }"
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

void AppTheme::applyCompactFont(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }

    QFont font = widget->font();
    font.setPointSizeF(9.0);
    widget->setFont(font);
}

void AppTheme::applyCompactFont(const QList<QWidget*>& widgets)
{
    for (QWidget* widget : widgets) {
        applyCompactFont(widget);
    }
}

void AppTheme::applyServerTableStyle(QTableView* tableView)
{
    if (tableView == nullptr) {
        return;
    }

    tableView->setProperty(kServerTableStyleProperty, true);
    tableView->setProperty(kServerTableHoveredRowProperty, -1);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->setMouseTracking(true);

    if (tableView->viewport() != nullptr) {
        tableView->viewport()->setMouseTracking(true);
    }

    QPalette palette = tableView->palette();
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f9f9f9")));
    tableView->setPalette(palette);

    if (tableView->findChild<QObject*>(QString::fromLatin1(kServerTableHoverTrackerName)) == nullptr) {
        new ServerTableHoverTracker(tableView);
    }

    if (tableView->itemDelegate() == nullptr
        || tableView->itemDelegate()->objectName() != QString::fromLatin1(kServerTableStyleDelegateName)) {
        tableView->setItemDelegate(new ServerTableStyleDelegate(tableView));
    }

    tableView->style()->unpolish(tableView);
    tableView->style()->polish(tableView);
    tableView->update();
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
