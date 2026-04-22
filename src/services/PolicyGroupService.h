#pragma once

#include <QList>

#include "domain/models/PolicyGroupItem.h"
#include "domain/models/VmessItem.h"
#include "persistence/IConfigRepository.h"

class PolicyGroupService {
public:
    explicit PolicyGroupService(IConfigRepository& repository);

    QList<PolicyGroupItem> list() const;
    PolicyGroupItem get(const QString& id) const;
    bool add(const PolicyGroupItem& group);
    bool update(const PolicyGroupItem& group);
    bool remove(const QString& id);
    QList<VmessItem> resolveMembers(const QString& groupId) const;

private:
    IConfigRepository& repository_;
};
