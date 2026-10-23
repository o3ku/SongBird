#pragma once

#include "common/PortValidator.h"

// The local port is a preference and may legitimately be "unset" (0 on disk, which the
// settings spin box shows as the default). Everything downstream -- the core config, the
// system proxy address, the hot-apply decision -- needs the number the program would
// actually use, not the number that happened to be stored.
//
// Those two are not the same thing, and comparing the stored values directly reads
// "unset -> default" as a change. That is how a save with nothing edited once managed to
// restart a running core: the general-page comparison normalised the port while the
// system-proxy comparison did not, so the same unchanged pair of configs answered "equal"
// and "changed" to two different questions.
//
// One rule, one place. Do not re-derive the fallback at a call site.
constexpr int kDefaultLocalPort = 10808;

constexpr int effectiveLocalPort(int configuredPort)
{
    return isValidTcpPort(configuredPort) ? configuredPort : kDefaultLocalPort;
}
