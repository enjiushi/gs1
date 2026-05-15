#pragma once

#include <cstdint>

namespace gs1
{
inline constexpr std::uint64_t SITE_PROJECTION_UPDATE_NONE = 0ULL;
inline constexpr std::uint64_t SITE_PROJECTION_UPDATE_ALL = 1ULL << 0U;
inline constexpr std::uint64_t SITE_PROJECTION_UPDATE_TASKS = 1ULL << 1U;
inline constexpr std::uint64_t SITE_PROJECTION_UPDATE_PHONE = 1ULL << 2U;
inline constexpr std::uint64_t SITE_PROJECTION_UPDATE_HUD = 1ULL << 3U;
inline constexpr std::uint64_t SITE_PROJECTION_UPDATE_WEATHER = 1ULL << 4U;
inline constexpr std::uint64_t SITE_PROJECTION_UPDATE_WORKER = 1ULL << 5U;
}  // namespace gs1
