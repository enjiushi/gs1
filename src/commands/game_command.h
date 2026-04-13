#pragma once

#include "support/id_types.h"
#include "gs1/types.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <new>
#include <type_traits>

namespace gs1
{
inline constexpr std::size_t k_command_cache_line_size = 64U;
inline constexpr std::size_t k_command_payload_byte_count = k_command_cache_line_size - sizeof(std::uint8_t);

#define GS1_ASSERT_TRIVIAL_COMMAND_TYPE(Type) \
    static_assert(std::is_standard_layout_v<Type>, #Type " must remain standard layout."); \
    static_assert(std::is_trivial_v<Type>, #Type " must remain trivial."); \
    static_assert(std::is_trivially_copyable_v<Type>, #Type " must remain trivially copyable.")

#define GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(Type, ExpectedSize) \
    GS1_ASSERT_TRIVIAL_COMMAND_TYPE(Type); \
    static_assert(sizeof(Type) == (ExpectedSize), #Type " size changed; revisit command payload packing."); \
    static_assert(sizeof(Type) <= k_command_payload_byte_count, #Type " exceeds the game command payload budget.")

enum class GameCommandType : std::uint8_t
{
    OpenMainMenu,
    StartNewCampaign,
    SelectDeploymentSite,
    ClearDeploymentSiteSelection,
    DeploymentSiteSelectionChanged,
    StartSiteAttempt,
    ReturnToRegionalMap,
    SiteAttemptEnded,
    PresentLog,
    Count
};

inline constexpr std::size_t k_game_command_type_count =
    static_cast<std::size_t>(GameCommandType::Count);

struct StartNewCampaignCommand final
{
    std::uint64_t campaign_seed;
    std::uint32_t campaign_days;
};

struct OpenMainMenuCommand final
{
};

struct SelectDeploymentSiteCommand final
{
    std::uint32_t site_id;
};

struct ClearDeploymentSiteSelectionCommand final
{
};

struct DeploymentSiteSelectionChangedCommand final
{
    std::uint32_t selected_site_id;
};

struct StartSiteAttemptCommand final
{
    std::uint32_t site_id;
};

struct ReturnToRegionalMapCommand final
{
};

struct SiteAttemptEndedCommand final
{
    std::uint32_t site_id;
    Gs1SiteAttemptResult result;
};

struct PresentLogCommand final
{
    char text[63];
};

GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(OpenMainMenuCommand, 1U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(StartNewCampaignCommand, 16U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SelectDeploymentSiteCommand, 4U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(ClearDeploymentSiteSelectionCommand, 1U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(DeploymentSiteSelectionChangedCommand, 4U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(StartSiteAttemptCommand, 4U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(ReturnToRegionalMapCommand, 1U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(SiteAttemptEndedCommand, 8U);
GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT(PresentLogCommand, 63U);

struct alignas(k_command_cache_line_size) GameCommand final
{
    unsigned char payload[k_command_payload_byte_count];
    GameCommandType type;

    template <typename PayloadData>
    [[nodiscard]] PayloadData& emplace_payload() noexcept
    {
        validate_payload_type<PayloadData>();
        auto* ptr = std::construct_at(reinterpret_cast<PayloadData*>(payload), PayloadData {});
        return *std::launder(ptr);
    }

    template <typename PayloadData>
    [[nodiscard]] PayloadData& emplace_payload(const PayloadData& value) noexcept
    {
        validate_payload_type<PayloadData>();
        auto* ptr = std::construct_at(reinterpret_cast<PayloadData*>(payload), value);
        return *std::launder(ptr);
    }

    template <typename PayloadData>
    void set_payload(const PayloadData& value) noexcept
    {
        (void)emplace_payload(value);
    }

    template <typename PayloadData>
    [[nodiscard]] PayloadData& payload_as() noexcept
    {
        validate_payload_type<PayloadData>();
        return *std::launder(reinterpret_cast<PayloadData*>(payload));
    }

    template <typename PayloadData>
    [[nodiscard]] const PayloadData& payload_as() const noexcept
    {
        validate_payload_type<PayloadData>();
        return *std::launder(reinterpret_cast<const PayloadData*>(payload));
    }

private:
    template <typename PayloadData>
    static constexpr void validate_payload_type() noexcept
    {
        GS1_ASSERT_TRIVIAL_COMMAND_TYPE(PayloadData);
        static_assert(sizeof(PayloadData) <= k_command_payload_byte_count, "Game command payload data exceeds command payload storage.");
        static_assert(alignof(PayloadData) <= alignof(GameCommand), "Game command payload data requires stronger alignment than GameCommand.");
    }
};

GS1_ASSERT_TRIVIAL_COMMAND_TYPE(GameCommand);
static_assert(sizeof(GameCommand) == k_command_cache_line_size, "GameCommand must fit exactly one cache line.");
static_assert(alignof(GameCommand) == k_command_cache_line_size, "GameCommand must be cache-line aligned.");
static_assert(offsetof(GameCommand, payload) == 0U, "GameCommand payload must start at byte zero.");
static_assert(offsetof(GameCommand, type) == k_command_payload_byte_count, "GameCommand type must sit at the tail byte.");

using GameCommandQueue = std::deque<GameCommand>;

#undef GS1_ASSERT_COMMAND_PAYLOAD_LAYOUT
#undef GS1_ASSERT_TRIVIAL_COMMAND_TYPE
}  // namespace gs1
