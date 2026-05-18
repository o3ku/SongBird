#pragma once

#include <QString>
#include <QWidget>

class QAction;
class QLineEdit;
class QListView;
class QMenu;
class QToolButton;
class LogFilterProxyModel;
class LogItemDelegate;
class LogListModel;
class QTimer;

class LogPanelWidget final : public QWidget {
public:
    explicit LogPanelWidget(QWidget* parent = nullptr);

    void appendLog(const QString& message);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyLogFilter();
    bool shouldStickLogViewToBottom(bool filterActive) const;
    void updateLogContextActions();
    void showLogContextMenu(const QPoint& position);
    void copySelectedLogLines();
    void copyAllLogLines();
    void updateWrapConfiguration();

    LogListModel* logModel_ = nullptr;
    LogFilterProxyModel* logFilterModel_ = nullptr;
    LogItemDelegate* logItemDelegate_ = nullptr;
    QListView* logView_ = nullptr;
    QLineEdit* logFilterEdit_ = nullptr;
    QToolButton* logStickToBottomButton_ = nullptr;
    QMenu* logContextMenu_ = nullptr;
    QAction* selectAllLogAction_ = nullptr;
    QAction* copySelectedLogAction_ = nullptr;
    QAction* copyAllLogAction_ = nullptr;
    QAction* clearLogAction_ = nullptr;
    QTimer* logScrollTimer_ = nullptr;
    bool logStickToBottomEnabled_ = true;
    bool logWasAtBottom_ = false;
};
