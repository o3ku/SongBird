#include "services/ExportService.h"

#include <QSet>

#include "subscription/ClashYamlBuilder.h"
#include "subscription/NodeFingerprint.h"
#include "subscription/ShareUrlBuilder.h"

QStringList ExportService::exportUris(const QList<VmessItem>& items) const
{
    QStringList uris;
    uris.reserve(items.size());
    for (const VmessItem& item : items) {
        const QString uri = ShareUrlBuilder::build(item).trimmed();
        if (!uri.isEmpty()) {
            uris.append(uri);
        }
    }
    return uris;
}

QString ExportService::exportClashYaml(const QList<VmessItem>& items) const
{
    return ClashYamlBuilder::buildProxiesBlock(items);
}

QList<VmessItem> ExportService::deduplicate(const QList<VmessItem>& items) const
{
    QSet<QString> seen;
    QList<VmessItem> result;
    result.reserve(items.size());
    for (const VmessItem& item : items) {
        const QString fp = NodeFingerprint::compute(item);
        if (!seen.contains(fp)) {
            seen.insert(fp);
            result.append(item);
        }
    }
    return result;
}

QStringList ExportService::exportDedupedUris(const QList<VmessItem>& items) const
{
    return exportUris(deduplicate(items));
}

QString ExportService::exportDedupedClashYaml(const QList<VmessItem>& items) const
{
    return exportClashYaml(deduplicate(items));
}
