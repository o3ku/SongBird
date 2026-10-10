#pragma once

#include <QString>

// Names sing-box uses for the files it keeps on disk.
namespace SingBoxPaths {

// Directory, directly under the application directory, holding the compiled rule-set files that
// `route.rule_set` entries point at. Three places have to agree on the name, which is why it is not
// private to any one of them: the config generator writes the paths into the config
// (`SingBoxRoutingConfigFragments`), the downloader saves the `.srs` files there
// (`GeoResourceUpdateService`), and the startup check asks whether they are already present
// (`ProxySession`). A second copy of the literal is a second place to change when the layout moves,
// and only one of them failing is what makes it a bug instead of a rename.
//
// Not to be confused with the `rule-set` in `GitHubUrls::singRuleSetDownloadUrl`: that one is the
// *branch name* of the upstream sing-geosite/sing-geoip repositories, and it only coincides with this
// directory name. Sharing a constant between them would tie our local layout to someone else's branch
// name, so a rename here would silently point the downloader at a branch that does not exist.
inline QString ruleSetDirectoryName()
{
    return QStringLiteral("rule-set");
}

} // namespace SingBoxPaths
