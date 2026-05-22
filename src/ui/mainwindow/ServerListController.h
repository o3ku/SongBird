#pragma once

#include <functional>

#include <QObject>
#include <QStringList>

struct Config;
class QLineEdit;
class ServerFilterProxyModel;
class ServerTableModel;
class ServerTableView;

class ServerListController final : public QObject {
public:
    struct Context {
        ServerTableView* serverView = nullptr;
        QLineEdit* serverFilterEdit = nullptr;
        ServerFilterProxyModel* serverFilterModel = nullptr;
        ServerTableModel* serverModel = nullptr;
    };

    ServerListController(
        const Context& context,
        std::function<void(const QString&)> applyTextFilter,
        std::function<void()> applyCurrentTabFilter,
        std::function<void()> updateSelectionForVisibleRows,
        std::function<void()> updateActionState,
        std::function<void()> updateQrPreview,
        std::function<void(const QStringList&)> reorderServersRequested,
        QObject* parent = nullptr);

    ~ServerListController() override;

    void setup();
    void setDynamicSortEnabled(bool enabled, bool invalidateModel);
    bool dynamicSortSuspended() const;
    void handleFilterTextChanged(const QString& text);
    void handleSubscriptionFilterChanged();
    void updateReorderAvailability();
    void restoreSortState(const Config& config);
    void captureSortState(Config& config) const;

private:
    void applySorting(int logicalIndex, Qt::SortOrder order);
    void toggleSorting(int logicalIndex);
    QStringList buildReorderedServerIds(const QList<int>& movedRows, int targetRow) const;

    Context context_;
    std::function<void(const QString&)> applyTextFilter_;
    std::function<void()> applyCurrentTabFilter_;
    std::function<void()> updateSelectionForVisibleRows_;
    std::function<void()> updateActionState_;
    std::function<void()> updateQrPreview_;
    std::function<void(const QStringList&)> reorderServersRequested_;
    bool dynamicSortSuspended_ = false;
    int sortColumn_ = -1;
    Qt::SortOrder sortOrder_ = Qt::AscendingOrder;
};
