#pragma once

#include "support/id_types.h"

#include <cstdint>

namespace gs1
{
struct FactionProgressState final
{
    FactionId faction_id {};
    std::int32_t faction_reputation {0};
    std::int32_t occupied_reputation {0};
    std::uint32_t unlocked_assistant_package_id {0};
    bool has_unlocked_assistant_package {false};
    bool onboarding_completed {false};
    bool tutorial_completed {false};
};
}  // namespace gs1
