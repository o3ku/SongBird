#pragma once

#include <QList>
#include <QUrl>

// Downloads go to github.com only. Third-party GitHub mirrors used to be prepended here
// (ghfast.top, mirror.ghproxy.com, ghproxy.net) so a download that GitHub served slowly could
// still finish, but a mirror is a man in the middle: it serves whatever bytes it chooses, and a
// checksum published beside the file on GitHub cannot vouch for bytes the mirror substituted.
// The helper stays as a pass-through so the five call sites keep one place to change if a
// legitimate alternate source is ever needed; it must never grow a third-party host again --
// check_no_third_party_mirrors.py fails the build if one appears.
inline QList<QUrl> buildGitHubMirrorCandidateUrls(const QUrl& url)
{
    return {url};
}
