#include "campaign/systems/technology_system.h"

#include "content/defs/progression_defs.h"
#include "runtime/game_runtime.h"
#include "support/currency.h"

#include <algorithm>
#include <cassert>

namespace gs1
{
namespace
{
const FactionProgressState* find_faction_progress(
    std::span<const FactionProgressState> faction_progress,
    FactionId faction_id) noexcept
{
    for (const auto& progress : faction_progress)
    {
        if (progress.faction_id == faction_id)
        {
            return &progress;
        }
    }

    return nullptr;
}

const FactionProgressState* find_faction_progress(
    const CampaignState& campaign,
    FactionId faction_id) noexcept
{
    return find_faction_progress(std::span<const FactionProgressState> {campaign.faction_progress}, faction_id);
}
[[nodiscard]] bool recipe_is_baseline_unlocked(RecipeId recipe_id) noexcept
{
    switch (recipe_id.value)
    {
    case k_recipe_craft_hammer:
    case k_recipe_craft_shovel:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool structure_recipe_is_baseline_unlocked(StructureId structure_id) noexcept
{
    switch (structure_id.value)
    {
    case k_structure_camp_stove:
    case k_structure_workbench:
    case k_structure_storage_crate:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool has_reputation_unlock(
    ReputationUnlockKind unlock_kind,
    std::uint32_t content_id) noexcept
{
    return find_reputation_unlock_def(unlock_kind, content_id) != nullptr;
}

[[nodiscard]] bool reputation_unlock_ready(
    const TechnologyState& technology,
    ReputationUnlockKind unlock_kind,
    std::uint32_t content_id) noexcept
{
    const auto* unlock_def = find_reputation_unlock_def(unlock_kind, content_id);
    return unlock_def != nullptr && technology.total_reputation >= unlock_def->reputation_requirement;
}

[[nodiscard]] bool has_technology_grant(
    TechnologyGrantedContentKind granted_content_kind,
    std::uint32_t granted_content_id) noexcept
{
    return find_technology_node_grant_def(granted_content_kind, granted_content_id) != nullptr;
}

[[nodiscard]] bool technology_grant_unlocked(
    std::span<const FactionProgressState> faction_progress,
    TechnologyGrantedContentKind granted_content_kind,
    std::uint32_t granted_content_id) noexcept
{
    const auto* node_def = find_technology_node_grant_def(granted_content_kind, granted_content_id);
    return node_def != nullptr && TechnologySystem::node_purchased(faction_progress, node_def->tech_node_id);
}

TechnologyNodeGrantState* find_node_grant_state_mut(
    TechnologyState& technology,
    TechNodeId tech_node_id) noexcept
{
    for (std::uint16_t index = 0; index < technology.technology_node_grant_count; ++index)
    {
        auto& grant = technology.technology_node_grants[index];
        if (grant.tech_node_id == tech_node_id)
        {
            return &grant;
        }
    }

    return nullptr;
}

const TechnologyNodeGrantState* find_node_grant_state(
    const TechnologyState& technology,
    TechNodeId tech_node_id) noexcept
{
    for (std::uint16_t index = 0; index < technology.technology_node_grant_count; ++index)
    {
        const auto& grant = technology.technology_node_grants[index];
        if (grant.tech_node_id == tech_node_id)
        {
            return &grant;
        }
    }

    return nullptr;
}

TechnologyNodeGrantState& require_node_grant_state_mut(
    TechnologyState& technology,
    TechNodeId tech_node_id)
{
    if (auto* existing = find_node_grant_state_mut(technology, tech_node_id);
        existing != nullptr)
    {
        return *existing;
    }

    assert(technology.technology_node_grant_count < k_max_technology_node_grant_count);
    auto& grant = technology.technology_node_grants[technology.technology_node_grant_count++];
    grant = TechnologyNodeGrantState {
        tech_node_id,
        TechnologyGrantState::Locked,
        {0U, 0U, 0U}};
    return grant;
}

TechnologyGrantState grant_state_from_payload(std::uint8_t grant_kind) noexcept
{
    switch (static_cast<ProgressionGrantKind>(grant_kind))
    {
    case ProgressionGrantKind::Available:
        return TechnologyGrantState::Available;
    case ProgressionGrantKind::Effective:
        return TechnologyGrantState::Effective;
    default:
        return TechnologyGrantState::Locked;
    }
}

}  // namespace

Gs1Status process_target_granted_message(
    RuntimeInvocation& invocation,
    const TargetGrantedMessage& message);

Gs1Status process_progression_event_message(
    RuntimeInvocation& invocation,
    const ProgressionEventOccurredMessage& message);

Gs1Status process_campaign_reputation_award_message(
    RuntimeInvocation& invocation,
    const CampaignReputationAwardRequestedMessage& message);

template <>
struct system_state_tags<TechnologySystem>
{
    using type = type_list<RuntimeCampaignTechnologyTag>;
};

const char* TechnologySystem::name() const noexcept
{
    return "TechnologySystem";
}

GameMessageSubscriptionSpan TechnologySystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::TargetGranted,
        GameMessageType::ProgressionEventOccurred,
        GameMessageType::CampaignReputationAwardRequested,
        GameMessageType::TechnologyNodeClaimRequested,
        GameMessageType::TechnologyNodeRefundRequested};
    return subscriptions;
}

HostMessageSubscriptionSpan TechnologySystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> TechnologySystem::profile_system_id() const noexcept
{
    return GS1_RUNTIME_PROFILE_SYSTEM_TECHNOLOGY;
}

std::optional<std::uint32_t> TechnologySystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

void TechnologySystem::run(RuntimeInvocation& invocation)
{
    (void)invocation;
}

Gs1Status TechnologySystem::handle(
    RuntimeInvocation& invocation,
    const TargetGrantedMessage& message)
{
    return process_target_granted_message(invocation, message);
}

Gs1Status TechnologySystem::handle(
    RuntimeInvocation& invocation,
    const ProgressionEventOccurredMessage& message)
{
    return process_progression_event_message(invocation, message);
}

Gs1Status TechnologySystem::handle(
    RuntimeInvocation& invocation,
    const CampaignReputationAwardRequestedMessage& message)
{
    return process_campaign_reputation_award_message(invocation, message);
}

Gs1Status TechnologySystem::handle(
    RuntimeInvocation& invocation,
    const TechnologyNodeClaimRequestedMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (message.tech_node_id == 0U || message.reputation_faction_id == 0U)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return GS1_STATUS_OK;
}

Gs1Status TechnologySystem::handle(
    RuntimeInvocation& invocation,
    const TechnologyNodeRefundRequestedMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (message.tech_node_id == 0U)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    return GS1_STATUS_OK;
}

bool TechnologySystem::node_purchased(
    const CampaignState& campaign,
    TechNodeId tech_node_id) noexcept
{
    return node_purchased(std::span<const FactionProgressState> {campaign.faction_progress}, tech_node_id);
}

bool TechnologySystem::node_purchased(
    std::span<const FactionProgressState> faction_progress,
    TechNodeId tech_node_id) noexcept
{
    const auto* node_def = find_technology_node_def(tech_node_id);
    if (node_def == nullptr)
    {
        return false;
    }

    return faction_reputation(faction_progress, node_def->faction_id) >=
        current_reputation_requirement(*node_def);
}

bool TechnologySystem::technology_tier_visible(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    return technology_tier_visible(std::span<const FactionProgressState> {campaign.faction_progress}, node_def);
}

bool TechnologySystem::technology_tier_visible(
    std::span<const FactionProgressState> faction_progress,
    const TechnologyNodeDef& node_def) noexcept
{
    (void)faction_progress;
    (void)node_def;
    return true;
}

bool TechnologySystem::plant_unlocked(
    const CampaignState& campaign,
    PlantId plant_id) noexcept
{
    return plant_unlocked(
        std::span<const FactionProgressState> {campaign.faction_progress},
        campaign.technology_state,
        plant_id);
}

bool TechnologySystem::plant_unlocked(
    std::span<const FactionProgressState> faction_progress,
    const TechnologyState& technology,
    PlantId plant_id) noexcept
{
    if (plant_id.value == 0U)
    {
        return false;
    }

    for (const auto starter_plant_id : all_initial_unlocked_plant_ids())
    {
        if (starter_plant_id == plant_id)
        {
            return true;
        }
    }

    if (has_technology_grant(TechnologyGrantedContentKind::Plant, plant_id.value))
    {
        return technology_grant_unlocked(
            faction_progress,
            TechnologyGrantedContentKind::Plant,
            plant_id.value);
    }

    return reputation_unlock_ready(technology, ReputationUnlockKind::Plant, plant_id.value);
}

bool TechnologySystem::item_unlocked(
    const CampaignState& campaign,
    ItemId item_id) noexcept
{
    return item_unlocked(
        std::span<const FactionProgressState> {campaign.faction_progress},
        campaign.technology_state,
        item_id);
}

bool TechnologySystem::item_unlocked(
    std::span<const FactionProgressState> faction_progress,
    const TechnologyState& technology,
    ItemId item_id) noexcept
{
    if (item_id.value == 0U)
    {
        return false;
    }

    const auto* item_def = find_item_def(item_id);
    if (item_def == nullptr)
    {
        return false;
    }

    if (item_def->linked_plant_id.value != 0U &&
        !plant_unlocked(faction_progress, technology, item_def->linked_plant_id))
    {
        return false;
    }

    if (item_def->linked_structure_id.value != 0U &&
        !structure_recipe_unlocked(faction_progress, technology, item_def->linked_structure_id))
    {
        return false;
    }

    if (has_technology_grant(TechnologyGrantedContentKind::Item, item_id.value))
    {
        return technology_grant_unlocked(
            faction_progress,
            TechnologyGrantedContentKind::Item,
            item_id.value);
    }

    if (!has_reputation_unlock(ReputationUnlockKind::Item, item_id.value))
    {
        return true;
    }

    return reputation_unlock_ready(technology, ReputationUnlockKind::Item, item_id.value);
}

bool TechnologySystem::structure_recipe_unlocked(
    const CampaignState& campaign,
    StructureId structure_id) noexcept
{
    return structure_recipe_unlocked(
        std::span<const FactionProgressState> {campaign.faction_progress},
        campaign.technology_state,
        structure_id);
}

bool TechnologySystem::structure_recipe_unlocked(
    std::span<const FactionProgressState> faction_progress,
    const TechnologyState& technology,
    StructureId structure_id) noexcept
{
    if (structure_id.value == 0U)
    {
        return false;
    }

    if (structure_recipe_is_baseline_unlocked(structure_id))
    {
        return true;
    }

    if (has_technology_grant(TechnologyGrantedContentKind::Structure, structure_id.value))
    {
        return technology_grant_unlocked(
            faction_progress,
            TechnologyGrantedContentKind::Structure,
            structure_id.value);
    }

    return reputation_unlock_ready(
        technology,
        ReputationUnlockKind::StructureRecipe,
        structure_id.value);
}

bool TechnologySystem::recipe_unlocked(
    const CampaignState& campaign,
    RecipeId recipe_id) noexcept
{
    return recipe_unlocked(
        std::span<const FactionProgressState> {campaign.faction_progress},
        campaign.technology_state,
        recipe_id);
}

bool TechnologySystem::recipe_unlocked(
    std::span<const FactionProgressState> faction_progress,
    const TechnologyState& technology,
    RecipeId recipe_id) noexcept
{
    if (recipe_id.value == 0U)
    {
        return false;
    }

    if (recipe_is_baseline_unlocked(recipe_id))
    {
        return true;
    }

    if (reputation_unlock_ready(technology, ReputationUnlockKind::Recipe, recipe_id.value))
    {
        return true;
    }

    if (has_technology_grant(TechnologyGrantedContentKind::Recipe, recipe_id.value))
    {
        return technology_grant_unlocked(
            faction_progress,
            TechnologyGrantedContentKind::Recipe,
            recipe_id.value);
    }

    const auto* recipe_def = find_craft_recipe_def(recipe_id);
    if (recipe_def != nullptr)
    {
        const auto* output_item_def = find_item_def(recipe_def->output_item_id);
        if (output_item_def != nullptr &&
            output_item_def->linked_structure_id.value != 0U &&
            structure_recipe_unlocked(faction_progress, technology, output_item_def->linked_structure_id))
        {
            return true;
        }
    }

    return false;
}

bool TechnologySystem::craft_output_unlocked(
    const CampaignState& campaign,
    ItemId item_id) noexcept
{
    return craft_output_unlocked(
        std::span<const FactionProgressState> {campaign.faction_progress},
        campaign.technology_state,
        item_id);
}

bool TechnologySystem::craft_output_unlocked(
    std::span<const FactionProgressState> faction_progress,
    const TechnologyState& technology,
    ItemId item_id) noexcept
{
    if (item_id.value == 0U)
    {
        return false;
    }

    const auto recipe_indices = craft_recipe_indices_for_output_item(item_id);
    if (recipe_indices.empty())
    {
        return false;
    }

    const auto recipe_defs = all_craft_recipe_defs();
    for (const std::size_t recipe_index : recipe_indices)
    {
        if (recipe_index < recipe_defs.size() &&
            recipe_unlocked(faction_progress, technology, recipe_defs[recipe_index].recipe_id))
        {
            return true;
        }
    }

    return false;
}

std::int32_t TechnologySystem::faction_reputation(
    const CampaignState& campaign,
    FactionId faction_id) noexcept
{
    return faction_reputation(std::span<const FactionProgressState> {campaign.faction_progress}, faction_id);
}

std::int32_t TechnologySystem::faction_reputation(
    std::span<const FactionProgressState> faction_progress,
    FactionId faction_id) noexcept
{
    const auto* progress = find_faction_progress(faction_progress, faction_id);
    return progress == nullptr ? 0 : std::max(0, progress->faction_reputation);
}

std::int32_t TechnologySystem::current_reputation_requirement(
    const TechnologyNodeDef& node_def) noexcept
{
    return std::max(0, node_def.reputation_requirement);
}

std::uint32_t TechnologySystem::current_internal_cost_cash_points(
    const TechnologyNodeDef& node_def) noexcept
{
    return node_def.internal_cost_cash_points;
}

std::int32_t TechnologySystem::current_cash_cost(
    const CampaignState& /*campaign*/,
    const TechnologyNodeDef& node_def) noexcept
{
    return cash_from_cash_points(current_internal_cost_cash_points(node_def));
}

float TechnologySystem::current_effect_parameter(
    const CampaignState& campaign,
    const TechnologyNodeDef& node_def) noexcept
{
    return current_effect_parameter(std::span<const FactionProgressState> {campaign.faction_progress}, node_def);
}

float TechnologySystem::current_effect_parameter(
    std::span<const FactionProgressState> faction_progress,
    const TechnologyNodeDef& node_def) noexcept
{
    const auto bonus_reputation = std::max(
        0,
        faction_reputation(faction_progress, node_def.faction_id) - current_reputation_requirement(node_def));
    const auto parameter =
        node_def.unlock_effect_parameter +
        static_cast<float>(bonus_reputation) * node_def.effect_parameter_per_bonus_reputation;
    return std::max(0.0f, parameter);
}

bool TechnologySystem::node_claimable(
    const CampaignState& /*campaign*/,
    const TechnologyNodeDef& /*node_def*/) noexcept
{
    return false;
}

bool TechnologySystem::node_claimable(
    std::span<const FactionProgressState> /*faction_progress*/,
    const TechnologyNodeDef& /*node_def*/) noexcept
{
    return false;
}

Gs1Status process_target_granted_message(
    RuntimeInvocation& invocation,
    const TargetGrantedMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (message.target_kind_id != k_progression_target_kind_technology_node ||
        message.target_id == 0U)
    {
        return GS1_STATUS_OK;
    }

    auto& technology = runtime_invocation_state_ref<RuntimeCampaignTechnologyTag>(invocation);
    auto& grant = require_node_grant_state_mut(technology, TechNodeId {message.target_id});
    const auto next_state = grant_state_from_payload(message.grant_kind);
    assert(next_state != TechnologyGrantState::Locked);
    assert(grant.grant_state != next_state);
    assert(!(grant.grant_state == TechnologyGrantState::Effective &&
        next_state == TechnologyGrantState::Available));
    grant.grant_state = next_state;
    return GS1_STATUS_OK;
}

Gs1Status process_progression_event_message(
    RuntimeInvocation& invocation,
    const ProgressionEventOccurredMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    const auto* event_def = find_progression_event_def(message.progression_event_id);
    if (event_def == nullptr ||
        event_def->token_kind_id != k_progression_token_kind_total_reputation ||
        message.amount <= 0)
    {
        return GS1_STATUS_OK;
    }

    runtime_invocation_state_ref<RuntimeCampaignTechnologyTag>(invocation).total_reputation +=
        message.amount;
    return GS1_STATUS_OK;
}

Gs1Status process_campaign_reputation_award_message(
    RuntimeInvocation& invocation,
    const CampaignReputationAwardRequestedMessage& message)
{
    if (!runtime_invocation_has_campaign(invocation))
    {
        return GS1_STATUS_INVALID_STATE;
    }

    if (message.delta <= 0)
    {
        return GS1_STATUS_OK;
    }

    runtime_invocation_state_ref<RuntimeCampaignTechnologyTag>(invocation).total_reputation +=
        message.delta;
    return GS1_STATUS_OK;
}

}  // namespace gs1
