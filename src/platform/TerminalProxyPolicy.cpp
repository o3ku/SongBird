#include "platform/TerminalProxyPolicy.h"

#include "platform/ITerminalProxyService.h"

#include <QUrl>

namespace {

// The loopback host used in the URL. 127.0.0.1 rather than localhost: a resolver that maps
// localhost to ::1 first would reach a listener bound only to IPv4 and fail, and the failure would
// look like "the proxy is broken" rather than "the URL used the wrong loopback".
constexpr const char* kLoopbackHost = "127.0.0.1";

} // namespace

namespace TerminalProxyPolicy {

QString proxyUrl(int httpPort)
{
    if (httpPort <= 0 || httpPort > 65535) {
        return {};
    }
    return QStringLiteral("http://%1:%2")
        .arg(QString::fromLatin1(kLoopbackHost))
        .arg(httpPort);
}

QString defaultNoProxy()
{
    // Comma-separated, no spaces: curl, git and Python's requests all split on commas, and a space
    // after the comma becomes part of the next entry in at least one of them.
    return QStringLiteral("localhost,127.0.0.1,::1,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16");
}

bool shouldRestoreOnShutdown(const TerminalProxySnapshot& snapshot, int httpPort)
{
    if (!snapshot.valid) {
        return false;
    }

    // A snapshot pointing at our own port means the session that captured it was already ours (the
    // crashed-session case). Restoring it would re-apply a dead endpoint, so the caller clears
    // instead.
    const QString ours = proxyUrl(httpPort);
    if (!ours.isEmpty()) {
        if (snapshot.httpProxy.has_value() && isOurValue(*snapshot.httpProxy, httpPort)) {
            return false;
        }
        if (snapshot.httpsProxy.has_value() && isOurValue(*snapshot.httpsProxy, httpPort)) {
            return false;
        }
    }

    return true;
}

bool isOurValue(const QString& value, int httpPort)
{
    const QString expected = proxyUrl(httpPort);
    if (expected.isEmpty()) {
        return false;
    }
    // Compare through QUrl so a trailing slash or a different case in the scheme still matches: the
    // point is to recognise our own value, and a strict string compare would miss the spellings a
    // user or another tool could have normalised it to.
    const QUrl actual(value.trimmed());
    const QUrl wanted(expected);
    if (!actual.isValid() || !wanted.isValid()) {
        return value.trimmed().compare(expected, Qt::CaseInsensitive) == 0;
    }
    return actual.scheme().compare(wanted.scheme(), Qt::CaseInsensitive) == 0
        && actual.host().compare(wanted.host(), Qt::CaseInsensitive) == 0
        && actual.port() == wanted.port();
}

} // namespace TerminalProxyPolicy
