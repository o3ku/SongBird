#pragma once

#include <variant>

#include <QString>

namespace PostStop {

struct Restart
{
    bool showStartupOverlay = false;
};

struct SwitchDefaultServer
{
    QString indexId;
    bool enableTun = false;
    bool showStartupOverlay = false;
};

struct CoreUpdate {};

} // namespace PostStop

using PostStopAction = std::variant<
    std::monostate,
    PostStop::Restart,
    PostStop::SwitchDefaultServer,
    PostStop::CoreUpdate>;
