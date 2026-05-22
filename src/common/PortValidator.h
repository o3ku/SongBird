#pragma once

constexpr int kMinTcpPort = 1;
constexpr int kMaxTcpPort = 65535;

constexpr bool isValidTcpPort(int port)
{
    return port >= kMinTcpPort && port <= kMaxTcpPort;
}

constexpr bool isValidOptionalTcpPort(int port)
{
    return port >= 0 && port <= kMaxTcpPort;
}
