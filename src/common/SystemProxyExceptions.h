#pragma once

#include <QString>
#include <QStringList>

#include "domain/models/Config.h"

// The IE/WinINET bypass list, which both front ends write through ISystemProxyService::update().
//
// Why it is here and not in either coordinator: SongBird.exe and SongBirdAuto.exe each carried a
// byte-identical copy of the literal (174 characters, compared character by character), and the two
// call sites then diverged -- the main app also merged the hosts the routing rules send direct,
// while Auto returned the literal untouched. So a `direct` rule bypassed the system proxy in one
// front end and went through the proxy in the other, for the same config file. One list, one merge.
namespace SystemProxyExceptions {

// The bypass list an installation that never customised one gets. Loopback and the RFC 1918
// ranges: traffic to these is local by definition, and sending it through a proxy makes a local
// service unreachable for no benefit.
QString defaultList();

// Splits a stored list into entries. Accepts ',' as well as ';', because the field has always been
// documented as either and existing configs contain both.
QStringList split(const QString& value);

// Appends `value` unless an entry already matches it case-insensitively. Entries are compared as
// written, so "example.com" and "*.example.com" are two entries on purpose.
void appendUnique(QStringList& entries, const QString& value);

// The hosts and single IP addresses this config's routing rules send direct, expressed as bypass
// entries, in the order RoutingConfigFragments::effectiveRoutingRules() gives them -- so this list
// and the core's first-match order say the same thing. Ranged values (geoip:, CIDR wider than /128)
// have no WinINET spelling and stay core-only.
//
// A rule that routes a domain direct is this config saying "do not proxy this", and the system proxy
// list is the other half of that same statement -- without it the domain leaves the machine through
// the system proxy even though the core would have handled it directly.
QStringList routeDerived(const Config& config);

// The complete list the system proxy should carry: the user's own (or the default) plus everything
// the routing rules imply.
QString resolve(const Config& config);

} // namespace SystemProxyExceptions
