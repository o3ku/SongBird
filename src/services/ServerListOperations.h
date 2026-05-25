#pragma once

#include <QList>
#include <QStringList>

#include "domain/models/VmessItem.h"

enum class ServerMoveOperation {
    Top = 0,
    Up,
    Down,
    Bottom
};

namespace ServerListOperations {

enum class ReorderResult {
    Unchanged,
    Updated,
    Invalid
};

QStringList normalizedIndexIds(const QList<QString>& indexIds);
bool moveServers(QList<VmessItem>& servers, const QStringList& selectedIndexIds, ServerMoveOperation operation);
ReorderResult reorderServers(QList<VmessItem>& servers, const QStringList& orderedIndexIds);

} // namespace ServerListOperations
