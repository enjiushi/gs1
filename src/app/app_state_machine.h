#pragma once

#include "gs1/types.h"

namespace gs1
{
class AppStateMachine final
{
public:
    [[nodiscard]] static bool can_transition(Gs1AppState from, Gs1AppState to) noexcept;
};
}  // namespace gs1
