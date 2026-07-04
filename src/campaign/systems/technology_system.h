#pragma once

#include "campaign/campaign_state.h"
#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/technology_defs.h"
#include "gs1/status.h"
#include "messages/game_message.h"
#include "runtime/system_interface.h"

namespace gs1
{
class TechnologySystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        TargetGrantedMessage,
        ProgressionEventOccurredMessage,
        CampaignReputationAwardRequestedMessage,
        TechnologyNodeClaimRequestedMessage,
        TechnologyNodeRefundRequestedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_TECHNOLOGY;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = std::nullopt;

    static constexpr std::array<StateSetId, 1> k_owned_state_sets {StateSetId::CampaignTechnology};
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const TargetGrantedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const ProgressionEventOccurredMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const CampaignReputationAwardRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const TechnologyNodeClaimRequestedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const TechnologyNodeRefundRequestedMessage& message);
    void run(RuntimeInvocation& invocation) override;
    [[nodiscard]] static bool node_purchased(
        const CampaignState& campaign,
        TechNodeId tech_node_id) noexcept;
    [[nodiscard]] static bool node_purchased(
        std::span<const FactionProgressState> faction_progress,
        TechNodeId tech_node_id) noexcept;
    [[nodiscard]] static bool technology_tier_visible(
        const CampaignState& campaign,
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static bool technology_tier_visible(
        std::span<const FactionProgressState> faction_progress,
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static bool plant_unlocked(
        const CampaignState& campaign,
        PlantId plant_id) noexcept;
    [[nodiscard]] static bool plant_unlocked(
        std::span<const FactionProgressState> faction_progress,
        const TechnologyState& technology,
        PlantId plant_id) noexcept;
    [[nodiscard]] static bool item_unlocked(
        const CampaignState& campaign,
        ItemId item_id) noexcept;
    [[nodiscard]] static bool item_unlocked(
        std::span<const FactionProgressState> faction_progress,
        const TechnologyState& technology,
        ItemId item_id) noexcept;
    [[nodiscard]] static bool structure_recipe_unlocked(
        const CampaignState& campaign,
        StructureId structure_id) noexcept;
    [[nodiscard]] static bool structure_recipe_unlocked(
        std::span<const FactionProgressState> faction_progress,
        const TechnologyState& technology,
        StructureId structure_id) noexcept;
    [[nodiscard]] static bool recipe_unlocked(
        const CampaignState& campaign,
        RecipeId recipe_id) noexcept;
    [[nodiscard]] static bool recipe_unlocked(
        std::span<const FactionProgressState> faction_progress,
        const TechnologyState& technology,
        RecipeId recipe_id) noexcept;
    [[nodiscard]] static bool craft_output_unlocked(
        const CampaignState& campaign,
        ItemId item_id) noexcept;
    [[nodiscard]] static bool craft_output_unlocked(
        std::span<const FactionProgressState> faction_progress,
        const TechnologyState& technology,
        ItemId item_id) noexcept;
    [[nodiscard]] static std::int32_t faction_reputation(
        const CampaignState& campaign,
        FactionId faction_id) noexcept;
    [[nodiscard]] static std::int32_t faction_reputation(
        std::span<const FactionProgressState> faction_progress,
        FactionId faction_id) noexcept;
    [[nodiscard]] static std::int32_t current_reputation_requirement(
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static std::uint32_t current_internal_cost_cash_points(
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static std::int32_t current_cash_cost(
        const CampaignState& campaign,
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static float current_effect_parameter(
        const CampaignState& campaign,
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static float current_effect_parameter(
        std::span<const FactionProgressState> faction_progress,
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static bool node_claimable(
        const CampaignState& campaign,
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static bool node_claimable(
        std::span<const FactionProgressState> faction_progress,
        const TechnologyNodeDef& node_def) noexcept;
};
}  // namespace gs1
