#include "site/systems/modifier_system.h"

#include "campaign/campaign_state.h"
#include "site/site_projection_update_flags.h"
#include "site/site_run_state.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace gs1
{
namespace
{
constexpr float k_modifier_channel_limit = 1.0f;
constexpr float k_modifier_change_epsilon = 1e-4f;

constexpr std::array<ModifierChannelTotals, 3> k_nearby_aura_presets = {
    ModifierChannelTotals{
        -0.15f, -0.08f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.08f, 0.0f},
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.12f, 0.07f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.04f, 0.0f},
    ModifierChannelTotals{
        0.0f, 0.0f, -0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f}};

constexpr std::array<ModifierChannelTotals, 4> k_run_modifier_presets = {
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f, 0.05f, 0.25f, 0.0f, 0.08f, 0.1f},
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.05f, 0.0f, 0.0f, 0.12f, 0.3f},
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.14f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.25f, 0.15f, 0.0f, 0.0f, 0.04f, 0.0f},
    ModifierChannelTotals{
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.18f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f}};

void accumulate_totals(
    ModifierChannelTotals& destination,
    const ModifierChannelTotals& source) noexcept
{
    destination.heat += source.heat;
    destination.wind += source.wind;
    destination.dust += source.dust;
    destination.moisture += source.moisture;
    destination.fertility += source.fertility;
    destination.salinity += source.salinity;
    destination.growth_pressure += source.growth_pressure;
    destination.salinity_density_cap += source.salinity_density_cap;
    destination.plant_density += source.plant_density;
    destination.health += source.health;
    destination.hydration += source.hydration;
    destination.nourishment += source.nourishment;
    destination.energy_cap += source.energy_cap;
    destination.energy += source.energy;
    destination.morale += source.morale;
    destination.work_efficiency += source.work_efficiency;
}

ModifierChannelTotals clamp_totals(ModifierChannelTotals totals) noexcept
{
    totals.heat = std::clamp(totals.heat, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.wind = std::clamp(totals.wind, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.dust = std::clamp(totals.dust, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.moisture = std::clamp(totals.moisture, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.fertility = std::clamp(totals.fertility, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.salinity = std::clamp(totals.salinity, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.growth_pressure =
        std::clamp(totals.growth_pressure, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.salinity_density_cap =
        std::clamp(totals.salinity_density_cap, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.plant_density =
        std::clamp(totals.plant_density, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.health = std::clamp(totals.health, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.hydration = std::clamp(totals.hydration, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.nourishment =
        std::clamp(totals.nourishment, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.energy_cap =
        std::clamp(totals.energy_cap, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.energy = std::clamp(totals.energy, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.morale = std::clamp(totals.morale, -k_modifier_channel_limit, k_modifier_channel_limit);
    totals.work_efficiency =
        std::clamp(totals.work_efficiency, -k_modifier_channel_limit, k_modifier_channel_limit);
    return totals;
}

ModifierChannelTotals apply_nearby_aura_modifier(ModifierId id) noexcept
{
    if (id.value == 0U)
    {
        return {};
    }

    const auto bucket = static_cast<std::size_t>(id.value) % k_nearby_aura_presets.size();
    return k_nearby_aura_presets[bucket];
}

ModifierChannelTotals apply_run_modifier(ModifierId id) noexcept
{
    if (id.value == 0U)
    {
        return {};
    }

    const auto bucket = static_cast<std::size_t>(id.value) % k_run_modifier_presets.size();
    return k_run_modifier_presets[bucket];
}

ModifierChannelTotals camp_comfort_bias(const CampState& camp) noexcept
{
    const float clamped_durability = std::clamp(camp.camp_durability, 0.0f, 100.0f);
    const float normalized = (clamped_durability * 0.01f * 2.0f) - 1.0f;

    ModifierChannelTotals totals {};
    totals.morale = normalized * 0.12f;
    totals.work_efficiency = normalized * 0.06f;
    return totals;
}

bool totals_match(const ModifierChannelTotals& lhs, const ModifierChannelTotals& rhs) noexcept
{
    return std::fabs(lhs.heat - rhs.heat) <= k_modifier_change_epsilon &&
        std::fabs(lhs.wind - rhs.wind) <= k_modifier_change_epsilon &&
        std::fabs(lhs.dust - rhs.dust) <= k_modifier_change_epsilon &&
        std::fabs(lhs.moisture - rhs.moisture) <= k_modifier_change_epsilon &&
        std::fabs(lhs.fertility - rhs.fertility) <= k_modifier_change_epsilon &&
        std::fabs(lhs.salinity - rhs.salinity) <= k_modifier_change_epsilon &&
        std::fabs(lhs.growth_pressure - rhs.growth_pressure) <= k_modifier_change_epsilon &&
        std::fabs(lhs.salinity_density_cap - rhs.salinity_density_cap) <= k_modifier_change_epsilon &&
        std::fabs(lhs.plant_density - rhs.plant_density) <= k_modifier_change_epsilon &&
        std::fabs(lhs.health - rhs.health) <= k_modifier_change_epsilon &&
        std::fabs(lhs.hydration - rhs.hydration) <= k_modifier_change_epsilon &&
        std::fabs(lhs.nourishment - rhs.nourishment) <= k_modifier_change_epsilon &&
        std::fabs(lhs.energy_cap - rhs.energy_cap) <= k_modifier_change_epsilon &&
        std::fabs(lhs.energy - rhs.energy) <= k_modifier_change_epsilon &&
        std::fabs(lhs.morale - rhs.morale) <= k_modifier_change_epsilon &&
        std::fabs(lhs.work_efficiency - rhs.work_efficiency) <= k_modifier_change_epsilon;
}

ModifierChannelTotals resolve_owned_totals(const SiteRunState& site_run) noexcept
{
    ModifierChannelTotals totals {};

    for (const auto modifier_id : site_run.modifier.active_nearby_aura_modifier_ids)
    {
        accumulate_totals(totals, apply_nearby_aura_modifier(modifier_id));
    }

    for (const auto modifier_id : site_run.modifier.active_run_modifier_ids)
    {
        accumulate_totals(totals, apply_run_modifier(modifier_id));
    }

    accumulate_totals(totals, camp_comfort_bias(site_run.camp));
    return clamp_totals(totals);
}

void resolve_modifier_totals(SiteSystemContext& context)
{
    const auto next_totals = resolve_owned_totals(context.site_run);
    auto& current_totals = context.site_run.modifier.resolved_channel_totals;

    if (!totals_match(current_totals, next_totals))
    {
        current_totals = next_totals;
        context.site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_HUD;
    }
}

void handle_site_run_started(
    SiteSystemContext& context,
    const SiteRunStartedCommand& /*payload*/) noexcept
{
    auto& modifier_state = context.site_run.modifier;
    modifier_state.active_run_modifier_ids.clear();
    modifier_state.active_nearby_aura_modifier_ids.clear();
    modifier_state.resolved_channel_totals = {};

    const auto& aura_ids = context.campaign.loadout_planner_state.active_nearby_aura_modifier_ids;
    modifier_state.active_nearby_aura_modifier_ids.insert(
        modifier_state.active_nearby_aura_modifier_ids.end(),
        aura_ids.begin(),
        aura_ids.end());

    resolve_modifier_totals(context);
}
}  // namespace

bool ModifierSystem::subscribes_to(GameCommandType type) noexcept
{
    return type == GameCommandType::SiteRunStarted;
}

Gs1Status ModifierSystem::process_command(
    SiteSystemContext& context,
    const GameCommand& command)
{
    if (command.type == GameCommandType::SiteRunStarted)
    {
        handle_site_run_started(context, command.payload_as<SiteRunStartedCommand>());
    }

    return GS1_STATUS_OK;
}

void ModifierSystem::run(SiteSystemContext& context)
{
    resolve_modifier_totals(context);
}
}  // namespace gs1
