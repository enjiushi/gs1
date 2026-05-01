#pragma once

#include "site/site_world.h"
#include "content/defs/gameplay_tuning_defs.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>

namespace gs1
{
inline constexpr float k_weather_contribution_epsilon = 0.0001f;
inline constexpr float k_weather_contribution_density_epsilon_raw = 0.01f;
inline constexpr float k_inverse_meter_scale = 0.01f;
inline constexpr float k_own_tile_contribution_scale = 0.04f;
inline constexpr float k_neighbor_contribution_scale = 0.018f;
inline constexpr std::uint8_t k_weather_contribution_wind_direction_sector_count = 8U;

struct WeatherContributionSample final
{
    int dx;
    int dy;
    int manhattan_distance;
};

struct WeatherDirectionStep final
{
    int x {1};
    int y {0};
};

inline constexpr std::array<WeatherContributionSample, 13> k_weather_contribution_samples {{
    {0, 0, 0},
    {0, -1, 1},
    {-1, 0, 1},
    {1, 0, 1},
    {0, 1, 1},
    {0, -2, 2},
    {-1, -1, 2},
    {1, -1, 2},
    {-2, 0, 2},
    {2, 0, 2},
    {-1, 1, 2},
    {1, 1, 2},
    {0, 2, 2},
}};

[[nodiscard]] inline float resolve_contribution_scale(int manhattan_distance) noexcept
{
    return manhattan_distance == 0
        ? k_own_tile_contribution_scale
        : (k_neighbor_contribution_scale / static_cast<float>(manhattan_distance));
}

[[nodiscard]] inline float resolve_density_scaled_resistance(
    float max_value,
    float density_raw) noexcept
{
    const auto& tuning = gameplay_tuning_def().ecology;
    const float clamped_density = std::clamp(density_raw * k_inverse_meter_scale, 0.0f, 1.0f);
    const float floor_scale =
        1.0f - std::clamp(tuning.resistance_density_influence, 0.0f, 1.0f);
    const float min_value = max_value * floor_scale;
    return std::lerp(min_value, max_value, clamped_density);
}

[[nodiscard]] inline float resolve_density_scaled_support_value(
    float max_value,
    float density_unit) noexcept
{
    return resolve_density_scaled_resistance(max_value, density_unit * 100.0f);
}

[[nodiscard]] inline float normalize_wind_direction_degrees(float value) noexcept
{
    const float wrapped = std::fmod(value, 360.0f);
    return wrapped < 0.0f ? wrapped + 360.0f : wrapped;
}

[[nodiscard]] inline std::uint8_t quantize_wind_direction_sector(
    float wind_direction_degrees,
    std::uint8_t sector_count = k_weather_contribution_wind_direction_sector_count) noexcept
{
    if (sector_count == 0U)
    {
        return 0U;
    }

    const float normalized = normalize_wind_direction_degrees(wind_direction_degrees);
    const float sector_size = 360.0f / static_cast<float>(sector_count);
    const auto raw_sector = static_cast<std::uint32_t>(
        (normalized + (sector_size * 0.5f)) / sector_size);
    return static_cast<std::uint8_t>(raw_sector % sector_count);
}

[[nodiscard]] inline WeatherDirectionStep resolve_wind_direction_step(
    float wind_direction_degrees) noexcept
{
    switch (quantize_wind_direction_sector(wind_direction_degrees))
    {
    case 0U:
        return WeatherDirectionStep {1, 0};
    case 1U:
        return WeatherDirectionStep {1, 1};
    case 2U:
        return WeatherDirectionStep {0, 1};
    case 3U:
        return WeatherDirectionStep {-1, 1};
    case 4U:
        return WeatherDirectionStep {-1, 0};
    case 5U:
        return WeatherDirectionStep {-1, -1};
    case 6U:
        return WeatherDirectionStep {0, -1};
    default:
        return WeatherDirectionStep {1, -1};
    }
}

[[nodiscard]] inline float compute_directional_wind_shadow_scale(
    int source_x,
    int source_y,
    int target_x,
    int target_y,
    std::uint8_t protection_range,
    WeatherDirectionStep wind_direction) noexcept
{
    if (protection_range == 0U)
    {
        return 0.0f;
    }

    const int offset_x = target_x - source_x;
    const int offset_y = target_y - source_y;
    const int manhattan_distance = std::abs(offset_x) + std::abs(offset_y);
    if (manhattan_distance == 0 || manhattan_distance > static_cast<int>(protection_range))
    {
        return 0.0f;
    }

    const auto matches_signed_axis = [](int component, int direction_component) noexcept -> bool
    {
        if (direction_component == 0)
        {
            return component == 0;
        }

        return component != 0 && ((component > 0) == (direction_component > 0));
    };

    if (wind_direction.x == 0 || wind_direction.y == 0)
    {
        if (matches_signed_axis(offset_x, wind_direction.x) &&
            matches_signed_axis(offset_y, wind_direction.y))
        {
            return 1.0f;
        }

        return 0.0f;
    }

    const bool on_horizontal_lane =
        matches_signed_axis(offset_x, wind_direction.x) && offset_y == 0;
    const bool on_vertical_lane =
        matches_signed_axis(offset_y, wind_direction.y) && offset_x == 0;
    if (on_horizontal_lane || on_vertical_lane)
    {
        return 1.0f;
    }

    if (offset_x == wind_direction.x && offset_y == wind_direction.y)
    {
        return 0.5f;
    }

    return 0.0f;
}

[[nodiscard]] inline SiteWorld::TileWeatherContributionData zero_weather_contribution() noexcept
{
    return SiteWorld::TileWeatherContributionData {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
}

inline void accumulate_weather_contribution(
    SiteWorld::TileWeatherContributionData& target,
    const SiteWorld::TileWeatherContributionData& delta) noexcept
{
    target.heat_protection += delta.heat_protection;
    target.wind_protection += delta.wind_protection;
    target.dust_protection += delta.dust_protection;
    target.fertility_improve += delta.fertility_improve;
    target.salinity_reduction += delta.salinity_reduction;
    target.irrigation += delta.irrigation;
}

[[nodiscard]] inline SiteWorld::TileWeatherContributionData clamp_weather_contribution(
    const SiteWorld::TileWeatherContributionData& contribution) noexcept
{
    return SiteWorld::TileWeatherContributionData {
        std::clamp(contribution.heat_protection, 0.0f, 100.0f),
        std::clamp(contribution.wind_protection, 0.0f, 100.0f),
        std::clamp(contribution.dust_protection, 0.0f, 100.0f),
        std::clamp(contribution.fertility_improve, 0.0f, 100.0f),
        std::clamp(contribution.salinity_reduction, 0.0f, 100.0f),
        std::clamp(contribution.irrigation, 0.0f, 100.0f)};
}

[[nodiscard]] inline SiteWorld::TileWeatherContributionData sum_weather_contributions(
    const SiteWorld::TileWeatherContributionData& plant_contribution,
    const SiteWorld::TileWeatherContributionData& device_contribution) noexcept
{
    SiteWorld::TileWeatherContributionData total = plant_contribution;
    accumulate_weather_contribution(total, device_contribution);
    return total;
}

[[nodiscard]] inline bool weather_contribution_changed(
    const SiteWorld::TileWeatherContributionData& lhs,
    const SiteWorld::TileWeatherContributionData& rhs,
    float epsilon = k_weather_contribution_epsilon) noexcept
{
    return std::fabs(lhs.heat_protection - rhs.heat_protection) > epsilon ||
        std::fabs(lhs.wind_protection - rhs.wind_protection) > epsilon ||
        std::fabs(lhs.dust_protection - rhs.dust_protection) > epsilon ||
        std::fabs(lhs.fertility_improve - rhs.fertility_improve) > epsilon ||
        std::fabs(lhs.salinity_reduction - rhs.salinity_reduction) > epsilon ||
        std::fabs(lhs.irrigation - rhs.irrigation) > epsilon;
}
}  // namespace gs1
