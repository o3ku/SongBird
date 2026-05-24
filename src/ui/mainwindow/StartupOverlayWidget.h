#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

class StartupOverlayWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit StartupOverlayWidget(QWidget* parent = nullptr);

    void showChecklist(const QString& title, const QStringList& items);
    void hideOverlay();
    bool isChecklistVisible() const;
    bool isChecklistFailed() const;
    void updateGeometry(const QRect& parentRect);

signals:
    void retryRequested();
    void dismissRequested();

private:
    void updateItems(const QStringList& items);
    void rebuildItems(const QStringList& items);
    void clearItems();
    void updateChecklistRow(QWidget* row, const QString& item);

    QWidget* contentWidget_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QWidget* itemsWidget_ = nullptr;
    QWidget* actionWidget_ = nullptr;
    QVBoxLayout* itemsLayout_ = nullptr;
    QPushButton* retryButton_ = nullptr;
    QPushButton* dismissButton_ = nullptr;
    QStringList checklistItems_;
    bool checklistFailed_ = false;
};
