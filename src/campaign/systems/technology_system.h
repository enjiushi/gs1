#pragma once

#include "campaign/campaign_state.h"
#include "campaign/systems/campaign_system_context.h"
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
    [[nodiscard]] const char* name() const noexcept override;
    void register_game_message_subscriptions(RuntimeSystemRegistration& registration) override;
    void register_host_message_subscriptions(RuntimeSystemRegistration& registration) override;
    void register_feedback_event_subscriptions(RuntimeSystemRegistration& registration) override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status process_game_message(
        GameRuntimeTempBridge& bridge,
        const GameMessage& message) override;
    [[nodiscard]] Gs1Status process_host_message(
        GameRuntimeTempBridge& bridge,
        const Gs1HostMessage& message) override;
    void run(GameRuntimeTempBridge& bridge) override;

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        CampaignSystemContext& context,
        const GameMessage& message);
    [[nodiscard]] static bool node_purchased(
        const CampaignState& campaign,
        TechNodeId tech_node_id) noexcept;
    [[nodiscard]] static bool technology_tier_visible(
        const CampaignState& campaign,
        const TechnologyNodeDef& node_def) noexcept;
    [[nodiscard]] static bool plant_unlocked(
        const CampaignState& campaign,
        PlantId plant_id) noexcept;
    [[nodiscard]] static bool item_unlocked(
        const CampaignState& campaign,
        ItemId item_id) noexcept;
    [[nodiscard]] static bool structure_recipe_unlocked(
        const CampaignState& campaign,
        StructureId structure_id) noexcept;
    [[nodiscard]] static bool recipe_unlocked(
        const CampaignState& campaign,
        RecipeId recipe_id) noexcept;
    [[nodiscard]] static bool craft_output_unlocked(
        const CampaignState& campaign,
        ItemId item_id) noexcept;
    [[nodiscard]] static std::int32_t faction_reputation(
        const CampaignState& campaign,
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
    [[nodiscard]] static bool node_claimable(
        const CampaignState& campaign,
        const TechnologyNodeDef& node_def) noexcept;
};
}  // namespace gs1
