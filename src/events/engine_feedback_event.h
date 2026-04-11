#pragma once

#include "support/id_types.h"

#include <cstdint>

namespace gs1
{
enum class EngineFeedbackEventType : std::uint32_t
{
    None = 0,
    PhysicsContact = 1,
    TraceHit = 2,
    AnimationNotify = 3
};

struct EngineFeedbackEvent final
{
    EngineFeedbackEventType type {EngineFeedbackEventType::None};
    SiteId site_id {};
    std::uint32_t arg0 {0};
    std::uint32_t arg1 {0};
};
}  // namespace gs1
