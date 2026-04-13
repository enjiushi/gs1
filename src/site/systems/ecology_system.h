#pragma once

#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class EcologySystem final
{
public:
    [[nodiscard]] static bool subscribes_to(GameCommandType type) noexcept;
    [[nodiscard]] static Gs1Status process_command(
        SiteSystemContext& context,
        const GameCommand& command);
    static void run(SiteSystemContext& context);
};
}  // namespace gs1
