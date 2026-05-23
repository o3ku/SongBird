#pragma once

struct TunModeItem {
    bool enableTun = false;
    bool autoRoute = true;
    bool strictRoute = true;
    QString stack = QStringLiteral("system");
    int mtu = 9000;
    bool enableIPv6Address = false;
    QString icmpRouting = QStringLiteral("rule");
    QString udpRouting = QStringLiteral("direct");
    bool enableLegacyProtect = false;
};
