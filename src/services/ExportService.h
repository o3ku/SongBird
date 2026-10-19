#pragma once

#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"

// Provides batch export operations on a list of proxy nodes.
//
// All methods are pure (stateless) and accept nodes by value so they can be
// used without a live config or repository.
class ExportService {
public:
    ExportService() = default;

    // Returns one share URI per exportable node, skipping Custom and Unknown
    // nodes that ShareUrlBuilder cannot represent.  Order is preserved.
    QStringList exportUris(const QList<VmessItem>& items) const;

    // Returns a Mihomo / Clash Meta proxies YAML block containing all
    // exportable nodes (see ClashYamlBuilder::buildProxiesBlock).
    QString exportClashYaml(const QList<VmessItem>& items) const;

    // Returns a deduplicated copy of items: the first occurrence of each node
    // (by NodeFingerprint) is kept; subsequent duplicates are dropped.
    // This is a stable operation — relative order among kept nodes is unchanged.
    QList<VmessItem> deduplicate(const QList<VmessItem>& items) const;

    // Convenience: deduplicate then export as share URIs.
    QStringList exportDedupedUris(const QList<VmessItem>& items) const;

    // Convenience: deduplicate then export as Clash YAML.
    QString exportDedupedClashYaml(const QList<VmessItem>& items) const;
};
