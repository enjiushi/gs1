#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace gs1
{
enum class ProgressionGrantKind : std::uint8_t
{
    None = 0,
    Unlocked = 1,
    Available = 2,
    Effective = 3
};

struct TokenKindDef final
{
    std::uint32_t token_kind_id {0};
    bool scoped {false};
    std::uint8_t reserved0[3] {};
    std::string_view display_name {};
};

struct TargetKindDef final
{
    std::uint32_t target_kind_id {0};
    std::uint8_t reserved0[4] {};
    std::string_view display_name {};
};

struct ProgressionEventDef final
{
    std::uint32_t progression_event_id {0};
    std::uint32_t token_kind_id {0};
    std::string_view display_name {};
};

struct ThresholdUnlockDef final
{
    std::uint32_t threshold_unlock_id {0};
    std::uint32_t token_kind_id {0};
    std::uint32_t scope_id {0};
    std::int32_t threshold_amount {0};
    std::uint32_t target_kind_id {0};
    std::uint32_t target_id {0};
    ProgressionGrantKind grant_kind {ProgressionGrantKind::None};
    std::uint8_t reserved0[3] {};
    std::string_view display_name {};
};

struct PurchaseDef final
{
    std::uint32_t purchase_entry_id {0};
    std::uint32_t token_kind_id {0};
    std::uint32_t scope_id {0};
    std::int32_t price_amount {0};
    std::uint32_t target_kind_id {0};
    std::uint32_t target_id {0};
    ProgressionGrantKind grant_kind {ProgressionGrantKind::None};
    std::uint8_t reserved0[3] {};
    std::string_view display_name {};
};

inline constexpr std::uint32_t k_progression_token_kind_total_reputation = 1U;
inline constexpr std::uint32_t k_progression_token_kind_faction_reputation = 2U;
inline constexpr std::uint32_t k_progression_token_kind_site_cash = 3U;

inline constexpr std::uint32_t k_progression_target_kind_technology_node = 1U;
inline constexpr std::uint32_t k_progression_target_kind_plant = 2U;
inline constexpr std::uint32_t k_progression_target_kind_item = 3U;
inline constexpr std::uint32_t k_progression_target_kind_structure_recipe = 4U;
inline constexpr std::uint32_t k_progression_target_kind_recipe = 5U;
inline constexpr std::uint32_t k_progression_target_kind_site_unlockable = 6U;

inline constexpr std::uint32_t k_progression_event_campaign_reputation_reward = 1U;
inline constexpr std::uint32_t k_progression_event_faction_reputation_reward = 2U;

[[nodiscard]] std::span<const TokenKindDef> all_token_kind_defs() noexcept;
[[nodiscard]] std::span<const TargetKindDef> all_target_kind_defs() noexcept;
[[nodiscard]] std::span<const ProgressionEventDef> all_progression_event_defs() noexcept;
[[nodiscard]] std::span<const ThresholdUnlockDef> all_threshold_unlock_defs() noexcept;
[[nodiscard]] std::span<const PurchaseDef> all_purchase_defs() noexcept;

[[nodiscard]] const TokenKindDef* find_token_kind_def(std::uint32_t token_kind_id) noexcept;
[[nodiscard]] const TargetKindDef* find_target_kind_def(std::uint32_t target_kind_id) noexcept;
[[nodiscard]] const ProgressionEventDef* find_progression_event_def(
    std::uint32_t progression_event_id) noexcept;
[[nodiscard]] const PurchaseDef* find_purchase_def(std::uint32_t purchase_entry_id) noexcept;
}  // namespace gs1
