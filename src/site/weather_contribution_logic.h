#pragma once

#include "site/site_world.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace gs1
{
inline constexpr float k_weather_contribution_epsilon = 0.0001f;
inline constexpr float k_weather_contribution_density_epsilon_raw = 0.01f;
inline constexpr float k_inverse_meter_scale = 0.01f;
inline constexpr float k_own_tile_contribution_scale = 0.04f;
inline constexpr float k_neighbor_contribution_scale = 0.018f;
inline constexpr std::uint8_t k_weather_contribution_wind_direction_sector_count = 16U;
inline constexpr float k_wind_shadow_alignment_exponent = 2.0f;
inline constexpr float k_wind_shadow_falloff_exponent = 3.0f;
inline constexpr float k_wind_shadow_falloff_strength = 2.0f;

struct WeatherContributionSample final
{
    int dx;
    int dy;
    int manhattan_distance;
};

struct WeatherUnitVector final
{
    float x {1.0f};
    float y {0.0f};
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

[[nodiscard]] inline float normalize_wind_direction_degrees(float value) noexcept
{
    const float wrapped = std::fmod(value, 360.0f);
    return wrapped < 0.0f ? wrapped + 360.0f : wrapped;
}

[[nodiscard]] inline WeatherUnitVector resolve_wind_direction_unit_vector(
    float wind_direction_degrees) noexcept
{
    const float radians =
        normalize_wind_direction_degrees(wind_direction_degrees) *
        (3.14159265358979323846f / 180.0f);
    return WeatherUnitVector {std::cos(radians), std::sin(radians)};
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
    const auto raw_sector = static_cast<std::uint32_t>(normalized / sector_size);
    return static_cast<std::uint8_t>(raw_sector % sector_count);
}

[[nodiscard]] inline float compute_directional_wind_shadow_scale(
    int source_x,
    int source_y,
    int target_x,
    int target_y,
    std::uint8_t protection_range,
    WeatherUnitVector wind_direction) noexcept
{
    if (protection_range == 0U)
    {
        return 0.0f;
    }

    const float offset_x = static_cast<float>(target_x - source_x);
    const float offset_y = static_cast<float>(target_y - source_y);
    const float distance = std::sqrt(offset_x * offset_x + offset_y * offset_y);
    if (distance <= k_weather_contribution_epsilon ||
        distance > static_cast<float>(protection_range) + k_weather_contribution_epsilon)
    {
        return 0.0f;
    }

    const float alignment =
        ((offset_x / distance) * wind_direction.x) +
        ((offset_y / distance) * wind_direction.y);
    if (alignment <= k_weather_contribution_epsilon)
    {
        return 0.0f;
    }

    const float alignment_scale =
        std::pow(std::clamp(alignment, 0.0f, 1.0f), k_wind_shadow_alignment_exponent);
    const float normalized_progress =
        protection_range <= 1U
        ? 0.0f
        : std::clamp(
              (distance - 1.0f) /
                  static_cast<float>(protection_range - 1U),
              0.0f,
              1.0f);
    const float distance_scale =
        std::exp(
            -k_wind_shadow_falloff_strength *
            std::pow(normalized_progress, k_wind_shadow_falloff_exponent));
    return alignment_scale * distance_scale;
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
