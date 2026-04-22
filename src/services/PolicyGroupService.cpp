#include "services/PolicyGroupService.h"

PolicyGroupService::PolicyGroupService(IConfigRepository& repository)
    : repository_(repository)
{
}

QList<PolicyGroupItem> PolicyGroupService::list() const
{
    return repository_.load().policyGroups;
}

PolicyGroupItem PolicyGroupService::get(const QString& id) const
{
    const QList<PolicyGroupItem> groups = list();
    for (const PolicyGroupItem& group : groups) {
        if (group.id == id) {
            return group;
        }
    }
    return {};
}

bool PolicyGroupService::add(const PolicyGroupItem& group)
{
    Config config = repository_.load();
    config.policyGroups.append(group);
    return repository_.save(config);
}

bool PolicyGroupService::update(const PolicyGroupItem& group)
{
    Config config = repository_.load();
    bool found = false;
    for (int i = 0; i < config.policyGroups.size(); ++i) {
        if (config.policyGroups[i].id == group.id) {
            config.policyGroups[i] = group;
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }
    return repository_.save(config);
}

bool PolicyGroupService::remove(const QString& id)
{
    Config config = repository_.load();
    for (int i = 0; i < config.policyGroups.size(); ++i) {
        if (config.policyGroups[i].id == id) {
            config.policyGroups.removeAt(i);
            return repository_.save(config);
        }
    }
    return false;
}

QList<VmessItem> PolicyGroupService::resolveMembers(const QString& groupId) const
{
    const PolicyGroupItem group = get(groupId);
    if (group.id.isEmpty()) {
        return {};
    }

    const QList<VmessItem> allServers = repository_.load().servers;
    QList<VmessItem> members;
    for (const VmessItem& server : allServers) {
        if (group.memberServerIds.contains(server.indexId)) {
            members.append(server);
        }
    }
    return members;
}
