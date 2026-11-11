#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class SingBoxOutboundConfigFragments {
public:
    // tag is the outbound's tag; single-node starts pass "proxy" (the tag the route names),
    // group starts pass the member tag that the urltest group references.
    static QJsonObject buildPrimaryOutbound(const Config& config, const VmessItem& server, const QString& tag);
    // The latency-based auto-select group that takes over the primary "proxy" tag for group
    // activations. url/interval/tolerance stay unset so the core's defaults apply
    // (generate_204 / 3m / 50ms); the app deliberately exposes no knobs for them.
    static QJsonObject buildUrlTestOutbound(const QStringList& memberTags);
    static QJsonObject buildTls(const Config& config, const VmessItem& server);
    static QJsonObject buildTransport(const VmessItem& server);
    static QJsonObject buildSocksOutbound(
        const QString& tag,
        const QString& server,
        int port,
        bool udpOverTcp = false);
};
