#pragma once

#include <QString>
#include <QList>

#include "domain/models/VmessItem.h"

// Serialises proxy nodes into a Mihomo / Clash Meta proxies YAML block.
//
// The output format mirrors what ClashYamlProxyParser and ClashProxyItemParser
// read, so a round-trip through build → parse should produce equivalent nodes
// (with caveats: transport fields that have no Clash equivalent are dropped, and
// Custom nodes are skipped entirely).
namespace ClashYamlBuilder {

// Returns a single-line flow-scalar YAML entry for the proxy, e.g.
//   {type: vmess, name: "my node", server: 1.2.3.4, port: 1234, ...}
// Returns an empty string when the item cannot be represented (e.g. Custom,
// Unknown, or a node with an empty address or invalid port).
QString buildProxyEntry(const VmessItem& item);

// Returns the full `proxies:` block as a YAML string with one proxy per line:
//   proxies:
//     - {type: vmess, ...}
//     - {type: ss, ...}
// Nodes that buildProxyEntry() cannot represent are silently skipped.
// Returns "proxies: []\n" when nothing can be represented.
QString buildProxiesBlock(const QList<VmessItem>& items);

} // namespace ClashYamlBuilder
