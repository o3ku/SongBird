#pragma once

#include <functional>

#include <QHash>
#include <QStringList>

class QTableView;
class ServerFilterProxyModel;
class ServerTableModel;
struct ServerTableRow;

class ServerSelectionController final {
public:
    ServerSelectionController(
        QTableView* serverView,
        ServerFilterProxyModel* serverFilterModel,
        ServerTableModel* serverModel,
        std::function<QString()> currentIndexId,
        std::function<void()> refreshSelectionUi);

    void setup();
    void setPreferredSelectionId(const QString& indexId);
    QString preferredSelectionId() const;
    void updateSelectionForVisibleRows();
    QList<const ServerTableRow*> selectedServers() const;
    const ServerTableRow* selectedServer() const;
    QString selectedServerId() const;
    QStringList selectedServerIds() const;
    QStringList selectedShareLinks(const QHash<QString, QString>& shareUrlsByIndexId) const;

private:
    void handleSelectionChanged();

    QTableView* serverView_ = nullptr;
    ServerFilterProxyModel* serverFilterModel_ = nullptr;
    ServerTableModel* serverModel_ = nullptr;
    std::function<QString()> currentIndexId_;
    std::function<void()> refreshSelectionUi_;
    QString preferredSelectionId_;
};
