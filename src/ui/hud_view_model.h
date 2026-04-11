#pragma once

namespace gs1
{
struct HudViewModel final
{
    float player_health {0.0f};
    float player_hydration {0.0f};
    float player_energy {0.0f};
    std::int32_t current_money {0};
    const char* current_weather_warning {nullptr};
};
}  // namespace gs1
