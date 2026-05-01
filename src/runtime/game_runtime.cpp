#include "runtime/game_runtime.h"

#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/campaign_system_context.h"
#include "campaign/systems/faction_reputation_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "campaign/systems/technology_system.h"
#include "content/defs/faction_defs.h"
#include "messages/message_dispatcher.h"
#include "content/defs/item_defs.h"
#include "content/defs/technology_defs.h"
#include "site/inventory_storage.h"
#include "site/site_world_access.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/camp_durability_system.h"
#include "site/systems/craft_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_support_system.h"
#include "site/systems/device_weather_contribution_system.h"
#include "site/systems/ecology_system.h"
#include "site/systems/economy_phone_system.h"
#include "site/site_projection_update_flags.h"
#include "site/systems/inventory_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/phone_panel_system.h"
#include "site/systems/plant_weather_contribution_system.h"
#include "site/systems/placement_validation_system.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"
#include "site/systems/task_board_system.h"
#include "site/systems/weather_event_system.h"
#include "site/systems/worker_condition_system.h"
#include "support/currency.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>

namespace gs1
{
namespace
{
constexpr float k_visible_tile_density_projection_step = 100.0f / 128.0f;
constexpr float k_visible_tile_burial_projection_step = 100.0f / 64.0f;
constexpr float k_visible_tile_local_wind_projection_step = 2.5f;
constexpr float k_visible_tile_moisture_projection_step = 100.0f / 64.0f;
constexpr float k_visible_tile_soil_fertility_projection_step = 100.0f / 64.0f;
constexpr float k_visible_tile_soil_salinity_projection_step = 100.0f / 64.0f;
constexpr double k_site_one_probe_window_minutes = 0.25;
constexpr std::int32_t k_regional_map_tile_spacing = 160;

[[nodiscard]] std::uint16_t projected_remaining_game_hours(double remaining_world_minutes) noexcept
{
    if (remaining_world_minutes <= 0.0)
    {
        return 0U;
    }

    const auto projected_hours = static_cast<std::uint32_t>(std::ceil(remaining_world_minutes / 60.0));
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(
        projected_hours,
        std::numeric_limits<std::uint16_t>::max()));
}

[[nodiscard]] bool is_site_one_probe_tile(TileCoord coord) noexcept
{
    return (coord.x == 12 && coord.y >= 14 && coord.y <= 17) ||
        (coord.x == 13 && coord.y >= 14 && coord.y <= 17);
}

[[nodiscard]] bool should_emit_site_one_runtime_probe(
    const SiteRunState& site_run,
    TileCoord coord) noexcept
{
    return site_run.site_id.value == 1U &&
        is_site_one_probe_tile(coord) &&
        site_run.clock.world_time_minutes <= k_site_one_probe_window_minutes;
}

[[nodiscard]] bool app_state_supports_technology_tree(Gs1AppState app_state) noexcept
{
    return app_state == GS1_APP_STATE_REGIONAL_MAP ||
        app_state == GS1_APP_STATE_SITE_ACTIVE;
}

enum CampaignUnlockCueDetailKind : std::uint32_t
{
    CAMPAIGN_UNLOCK_CUE_DETAIL_REPUTATION_UNLOCK = 1U,
    CAMPAIGN_UNLOCK_CUE_DETAIL_TECHNOLOGY_NODE = 2U
};

[[nodiscard]] std::vector<std::uint32_t> capture_unlocked_reputation_unlock_ids(
    const CampaignState& campaign)
{
    std::vector<std::uint32_t> unlock_ids {};
    unlock_ids.reserve(all_reputation_unlock_defs().size());
    for (const auto& unlock_def : all_reputation_unlock_defs())
    {
        bool unlocked = false;
        switch (unlock_def.unlock_kind)
        {
        case ReputationUnlockKind::Plant:
            unlocked = TechnologySystem::plant_unlocked(campaign, PlantId {unlock_def.content_id});
            break;
        case ReputationUnlockKind::Item:
            unlocked = TechnologySystem::item_unlocked(campaign, ItemId {unlock_def.content_id});
            break;
        case ReputationUnlockKind::StructureRecipe:
            unlocked = TechnologySystem::structure_recipe_unlocked(campaign, StructureId {unlock_def.content_id});
            break;
        case ReputationUnlockKind::Recipe:
            unlocked = TechnologySystem::recipe_unlocked(campaign, RecipeId {unlock_def.content_id});
            break;
        default:
            break;
        }

        if (unlocked)
        {
            unlock_ids.push_back(unlock_def.unlock_id);
        }
    }

    return unlock_ids;
}

[[nodiscard]] std::vector<std::uint32_t> capture_unlocked_technology_node_ids(
    const CampaignState& campaign)
{
    std::vector<std::uint32_t> node_ids {};
    node_ids.reserve(all_technology_node_defs().size());
    for (const auto& node_def : all_technology_node_defs())
    {
        if (TechnologySystem::node_purchased(campaign, node_def.tech_node_id))
        {
            node_ids.push_back(node_def.tech_node_id.value);
        }
    }

    return node_ids;
}

[[nodiscard]] bool sorted_contains(
    const std::vector<std::uint32_t>& values,
    std::uint32_t value) noexcept
{
    return std::binary_search(values.begin(), values.end(), value);
}

[[nodiscard]] std::uint16_t quantize_projected_tile_channel(
    float value,
    float max_value,
    float step) noexcept
{
    if (max_value <= 0.0f || step <= 0.0f)
    {
        return 0U;
    }

    const float clamped = std::clamp(value, 0.0f, max_value);
    const float scaled = std::round(clamped / step);
    return static_cast<std::uint16_t>(std::clamp(scaled, 0.0f, 65535.0f));
}

[[nodiscard]] ProjectedSiteTileState capture_projected_tile_state(
    const SiteWorld::TileData& tile) noexcept
{
    const bool tile_is_occupied =
        tile.ecology.plant_id.value != 0U ||
        tile.ecology.ground_cover_type_id != 0U ||
        tile.device.structure_id.value != 0U;
    const auto visible_excavation_depth = tile_is_occupied
        ? ExcavationDepth::None
        : tile.excavation.depth;
    const float wind_protection =
        tile.plant_weather_contribution.wind_protection +
        tile.device_weather_contribution.wind_protection;
    const float heat_protection =
        tile.plant_weather_contribution.heat_protection +
        tile.device_weather_contribution.heat_protection;
    const float dust_protection =
        tile.plant_weather_contribution.dust_protection +
        tile.device_weather_contribution.dust_protection;
    return ProjectedSiteTileState {
        tile.static_data.terrain_type_id,
        tile.ecology.plant_id.value,
        tile.device.structure_id.value,
        tile.ecology.ground_cover_type_id,
        quantize_projected_tile_channel(
            tile.ecology.plant_density,
            100.0f,
            k_visible_tile_density_projection_step),
        quantize_projected_tile_channel(
            tile.ecology.sand_burial,
            100.0f,
            k_visible_tile_burial_projection_step),
        quantize_projected_tile_channel(
            tile.local_weather.wind,
            100.0f,
            k_visible_tile_local_wind_projection_step),
        quantize_projected_tile_channel(
            wind_protection,
            100.0f,
            k_visible_tile_local_wind_projection_step),
        quantize_projected_tile_channel(
            heat_protection,
            100.0f,
            k_visible_tile_local_wind_projection_step),
        quantize_projected_tile_channel(
            dust_protection,
            100.0f,
            k_visible_tile_local_wind_projection_step),
        quantize_projected_tile_channel(
            tile.ecology.moisture,
            100.0f,
            k_visible_tile_moisture_projection_step),
        quantize_projected_tile_channel(
            tile.ecology.soil_fertility,
            100.0f,
            k_visible_tile_soil_fertility_projection_step),
        quantize_projected_tile_channel(
            tile.ecology.soil_salinity,
            100.0f,
            k_visible_tile_soil_salinity_projection_step),
        static_cast<std::uint8_t>(tile.excavation.depth),
        static_cast<std::uint8_t>(visible_excavation_depth),
        true};
}

[[nodiscard]] bool projected_tile_state_matches(
    const ProjectedSiteTileState& lhs,
    const ProjectedSiteTileState& rhs) noexcept
{
    return lhs.valid == rhs.valid &&
        lhs.terrain_type_id == rhs.terrain_type_id &&
        lhs.plant_type_id == rhs.plant_type_id &&
        lhs.structure_type_id == rhs.structure_type_id &&
        lhs.ground_cover_type_id == rhs.ground_cover_type_id &&
        lhs.plant_density_quantized == rhs.plant_density_quantized &&
        lhs.sand_burial_quantized == rhs.sand_burial_quantized &&
        lhs.local_wind_quantized == rhs.local_wind_quantized &&
        lhs.wind_protection_quantized == rhs.wind_protection_quantized &&
        lhs.heat_protection_quantized == rhs.heat_protection_quantized &&
        lhs.dust_protection_quantized == rhs.dust_protection_quantized &&
        lhs.moisture_quantized == rhs.moisture_quantized &&
        lhs.soil_fertility_quantized == rhs.soil_fertility_quantized &&
        lhs.soil_salinity_quantized == rhs.soil_salinity_quantized &&
        lhs.excavation_depth == rhs.excavation_depth &&
        lhs.visible_excavation_depth == rhs.visible_excavation_depth;
}

[[nodiscard]] std::uint32_t visible_loadout_slot_count(const LoadoutPlannerState& planner) noexcept
{
    std::uint32_t count = 0U;
    for (const auto& slot : planner.selected_loadout_slots)
    {
        if (slot.occupied && slot.item_id.value != 0U && slot.quantity > 0U)
        {
            count += 1U;
        }
    }

    return count;
}

[[nodiscard]] const SiteMetaState* find_site_meta(
    const CampaignState& campaign,
    std::uint32_t site_id) noexcept
{
    for (const auto& site : campaign.sites)
    {
        if (site.site_id.value == site_id)
        {
            return &site;
        }
    }

    return nullptr;
}

[[nodiscard]] const FactionProgressState* find_faction_progress(
    const CampaignState& campaign,
    FactionId faction_id) noexcept
{
    for (const auto& faction_progress : campaign.faction_progress)
    {
        if (faction_progress.faction_id == faction_id)
        {
            return &faction_progress;
        }
    }

    return nullptr;
}

[[nodiscard]] bool site_contributes_to_selected_target(
    const CampaignState& campaign,
    const SiteMetaState& contributor) noexcept
{
    if (!campaign.regional_map_state.selected_site_id.has_value() ||
        contributor.site_state != GS1_SITE_STATE_COMPLETED)
    {
        return false;
    }

    const auto* selected_site =
        find_site_meta(campaign, campaign.regional_map_state.selected_site_id->value);
    if (selected_site == nullptr)
    {
        return false;
    }

    for (const auto adjacent_site_id : selected_site->adjacent_site_ids)
    {
        if (adjacent_site_id == contributor.site_id)
        {
            return true;
        }
    }

    return false;
}

enum RegionalSupportPreviewBits : std::uint16_t
{
    REGIONAL_SUPPORT_PREVIEW_ITEMS = 1U << 0,
    REGIONAL_SUPPORT_PREVIEW_WIND = 1U << 1,
    REGIONAL_SUPPORT_PREVIEW_FERTILITY = 1U << 2,
    REGIONAL_SUPPORT_PREVIEW_RECOVERY = 1U << 3
};

[[nodiscard]] std::uint16_t support_preview_mask_for_site(const SiteMetaState& site) noexcept
{
    std::uint16_t mask = 0U;
    for (const auto& slot : site.exported_support_items)
    {
        if (slot.occupied && slot.item_id.value != 0U && slot.quantity > 0U)
        {
            mask |= REGIONAL_SUPPORT_PREVIEW_ITEMS;
            break;
        }
    }

    for (const auto modifier_id : site.nearby_aura_modifier_ids)
    {
        const auto* modifier_def = find_modifier_def(modifier_id);
        if (modifier_def == nullptr)
        {
            continue;
        }

        const auto& totals = modifier_def->totals;
        if (std::fabs(totals.wind) > 1e-4f ||
            std::fabs(totals.heat) > 1e-4f ||
            std::fabs(totals.dust) > 1e-4f)
        {
            mask |= REGIONAL_SUPPORT_PREVIEW_WIND;
        }
        if (std::fabs(totals.moisture) > 1e-4f ||
            std::fabs(totals.fertility) > 1e-4f ||
            std::fabs(totals.plant_density) > 1e-4f ||
            std::fabs(totals.salinity) > 1e-4f ||
            std::fabs(totals.growth_pressure) > 1e-4f)
        {
            mask |= REGIONAL_SUPPORT_PREVIEW_FERTILITY;
        }
        if (std::fabs(totals.health) > 1e-4f ||
            std::fabs(totals.hydration) > 1e-4f ||
            std::fabs(totals.nourishment) > 1e-4f ||
            std::fabs(totals.energy_cap) > 1e-4f ||
            std::fabs(totals.energy) > 1e-4f ||
            std::fabs(totals.morale) > 1e-4f ||
            std::fabs(totals.work_efficiency) > 1e-4f)
        {
            mask |= REGIONAL_SUPPORT_PREVIEW_RECOVERY;
        }
    }

    return mask;
}

std::size_t message_type_index(GameMessageType type) noexcept
{
    return static_cast<std::size_t>(type);
}

bool is_valid_message_type(GameMessageType type) noexcept
{
    return message_type_index(type) < k_game_message_type_count;
}

std::size_t feedback_event_type_index(Gs1FeedbackEventType type) noexcept
{
    return static_cast<std::size_t>(type);
}

bool is_valid_feedback_event_type(Gs1FeedbackEventType type) noexcept
{
    return feedback_event_type_index(type) < k_feedback_event_type_count;
}

bool is_valid_profile_system_id(Gs1RuntimeProfileSystemId system_id) noexcept
{
    return static_cast<std::size_t>(system_id) <
        static_cast<std::size_t>(GS1_RUNTIME_PROFILE_SYSTEM_COUNT);
}

double elapsed_milliseconds_since(
    std::chrono::steady_clock::time_point started_at) noexcept
{
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started_at)
        .count();
}

constexpr auto record_timing_sample = [](auto& accumulator, double elapsed_ms) noexcept
{
    accumulator.sample_count += 1U;
    accumulator.last_elapsed_ms = elapsed_ms;
    accumulator.total_elapsed_ms += elapsed_ms;
    accumulator.max_elapsed_ms = std::max(accumulator.max_elapsed_ms, elapsed_ms);
};

enum : std::uint16_t
{
    k_hud_warning_none = 0U,
    k_hud_warning_wind_watch = 1U,
    k_hud_warning_wind_exposure = 2U,
    k_hud_warning_severe_gale = 3U,
    k_hud_warning_sandblast = 4U
};

std::uint16_t resolve_hud_warning_code(const SiteRunState& site_run) noexcept
{
    if (site_run.site_world == nullptr || !site_run.site_world->initialized())
    {
        return k_hud_warning_none;
    }

    const auto worker = site_run.site_world->worker();
    if (!site_world_access::tile_coord_in_bounds(site_run, worker.position.tile_coord))
    {
        return k_hud_warning_none;
    }

    const auto local_weather =
        site_world_access::tile_local_weather(site_run, worker.position.tile_coord);
    const float wind = std::max(local_weather.wind, 0.0f);
    const float dust = std::max(local_weather.dust, 0.0f);

    if (wind >= 65.0f || (wind >= 50.0f && dust >= 18.0f))
    {
        return k_hud_warning_sandblast;
    }

    if (!worker.conditions.is_sheltered && wind >= 45.0f)
    {
        return k_hud_warning_severe_gale;
    }

    if (!worker.conditions.is_sheltered && wind >= 28.0f)
    {
        return k_hud_warning_wind_exposure;
    }

    if (wind >= 18.0f ||
        site_run.weather.weather_wind >= 14.0f ||
        site_run.event.event_wind_pressure >= 4.0f)
    {
        return k_hud_warning_wind_watch;
    }

    return k_hud_warning_none;
}

std::uint64_t pack_inventory_use_arg(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index,
    std::uint32_t quantity) noexcept
{
    return static_cast<std::uint64_t>(container_kind) |
        (static_cast<std::uint64_t>(slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 16U);
}

Gs1InventoryContainerKind unpack_inventory_container(std::uint64_t packed) noexcept
{
    return static_cast<Gs1InventoryContainerKind>(packed & 0xffU);
}

std::uint32_t unpack_inventory_slot_index(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 8U) & 0xffU);
}

std::uint32_t unpack_inventory_quantity(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 16U) & 0xffffU);
}

std::uint32_t unpack_inventory_storage_id(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>(packed & 0xffffffffULL);
}

std::uint64_t pack_inventory_transfer_arg(
    Gs1InventoryContainerKind source_container_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_container_kind,
    std::uint32_t quantity) noexcept
{
    return static_cast<std::uint64_t>(source_container_kind) |
        (static_cast<std::uint64_t>(source_slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(destination_container_kind) << 16U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 24U);
}

Gs1InventoryContainerKind unpack_transfer_source_container(std::uint64_t packed) noexcept
{
    return static_cast<Gs1InventoryContainerKind>(packed & 0xffU);
}

std::uint32_t unpack_transfer_source_slot(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 8U) & 0xffU);
}

Gs1InventoryContainerKind unpack_transfer_destination_container(std::uint64_t packed) noexcept
{
    return static_cast<Gs1InventoryContainerKind>((packed >> 16U) & 0xffU);
}

std::uint32_t unpack_transfer_quantity(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 24U) & 0xffffU);
}

std::uint32_t unpack_transfer_source_owner(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>(packed & 0xffffffffULL);
}

std::uint32_t unpack_transfer_destination_owner(std::uint64_t packed) noexcept
{
    return static_cast<std::uint32_t>((packed >> 32U) & 0xffffffffULL);
}

bool action_has_started(const ActionState& action_state) noexcept
{
    return action_state.current_action_id.has_value() &&
        action_state.action_kind != ActionKind::None &&
        action_state.started_at_world_minute.has_value() &&
        action_state.total_action_minutes > 0.0;
}

float action_progress_normalized(const ActionState& action_state) noexcept
{
    if (!action_has_started(action_state))
    {
        return 0.0f;
    }

    const double elapsed_minutes =
        std::max(0.0, action_state.total_action_minutes - action_state.remaining_action_minutes);
    const double normalized = elapsed_minutes / std::max(action_state.total_action_minutes, 0.0001);
    return static_cast<float>(std::clamp(normalized, 0.0, 1.0));
}

Gs1EngineMessage make_engine_message(
    Gs1EngineMessageType type)
{
    Gs1EngineMessage message {};
    message.type = type;
    return message;
}

Gs1TaskPresentationListKind to_task_presentation_list_kind(TaskRuntimeListKind kind) noexcept
{
    switch (kind)
    {
    case TaskRuntimeListKind::Accepted:
        return GS1_TASK_PRESENTATION_LIST_ACCEPTED;
    case TaskRuntimeListKind::PendingClaim:
        return GS1_TASK_PRESENTATION_LIST_PENDING_CLAIM;
    case TaskRuntimeListKind::Claimed:
        return GS1_TASK_PRESENTATION_LIST_CLAIMED;
    case TaskRuntimeListKind::Visible:
    default:
        return GS1_TASK_PRESENTATION_LIST_VISIBLE;
    }
}

Gs1PhoneListingPresentationKind to_phone_listing_presentation_kind(PhoneListingKind kind) noexcept
{
    switch (kind)
    {
    case PhoneListingKind::SellItem:
        return GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM;
    case PhoneListingKind::HireContractor:
        return GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR;
    case PhoneListingKind::PurchaseUnlockable:
        return GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE;
    case PhoneListingKind::BuyItem:
    default:
        return GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM;
    }
}

Gs1PhonePanelSection to_phone_panel_section(PhonePanelSection section) noexcept
{
    switch (section)
    {
    case PhonePanelSection::Home:
        return GS1_PHONE_PANEL_SECTION_HOME;
    case PhonePanelSection::Tasks:
        return GS1_PHONE_PANEL_SECTION_TASKS;
    case PhonePanelSection::Buy:
        return GS1_PHONE_PANEL_SECTION_BUY;
    case PhonePanelSection::Sell:
        return GS1_PHONE_PANEL_SECTION_SELL;
    case PhonePanelSection::Hire:
        return GS1_PHONE_PANEL_SECTION_HIRE;
    case PhonePanelSection::Cart:
        return GS1_PHONE_PANEL_SECTION_CART;
    default:
        return GS1_PHONE_PANEL_SECTION_HOME;
    }
}

template <typename SiteSystemTag, typename ProcessMessageFn>
Gs1Status dispatch_site_system_message(
    ProcessMessageFn process_message_fn,
    const GameMessage& message,
    const std::optional<CampaignState>& campaign,
    std::optional<SiteRunState>& active_site_run,
    GameMessageQueue& message_queue,
    double fixed_step_seconds)
{
    if (!campaign.has_value() || !active_site_run.has_value())
    {
        return GS1_STATUS_INVALID_STATE;
    }

    auto context = make_site_system_context<SiteSystemTag>(
        *campaign,
        *active_site_run,
        message_queue,
        fixed_step_seconds,
        SiteMoveDirectionInput {});
    return process_message_fn(context, message);
}
}  // namespace

GameRuntime::GameRuntime(Gs1RuntimeCreateDesc create_desc)
    : create_desc_(create_desc)
{
    if (create_desc_.fixed_step_seconds > 0.0)
    {
        fixed_step_seconds_ = create_desc_.fixed_step_seconds;
    }

    initialize_subscription_tables();
}

Gs1Status GameRuntime::get_profiling_snapshot(Gs1RuntimeProfilingSnapshot& out_snapshot) const noexcept
{
    if (out_snapshot.struct_size != sizeof(Gs1RuntimeProfilingSnapshot))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_snapshot.system_count = static_cast<std::uint32_t>(GS1_RUNTIME_PROFILE_SYSTEM_COUNT);
    copy_timing_snapshot(phase1_timing_, out_snapshot.phase1_timing);
    copy_timing_snapshot(phase2_timing_, out_snapshot.phase2_timing);
    copy_timing_snapshot(fixed_step_timing_, out_snapshot.fixed_step_timing);

    for (std::size_t index = 0; index < profiled_systems_.size(); ++index)
    {
        auto& destination = out_snapshot.systems[index];
        const auto& source = profiled_systems_[index];
        destination.system_id = static_cast<Gs1RuntimeProfileSystemId>(index);
        destination.enabled = source.enabled ? 1U : 0U;
        destination.reserved0 = 0U;
        destination.reserved1 = 0U;
        copy_timing_snapshot(source.run_timing, destination.run_timing);
        copy_timing_snapshot(source.message_timing, destination.message_timing);
    }

    return GS1_STATUS_OK;
}

void GameRuntime::reset_profiling() noexcept
{
    phase1_timing_ = {};
    phase2_timing_ = {};
    fixed_step_timing_ = {};

    for (auto& system_state : profiled_systems_)
    {
        system_state.run_timing = {};
        system_state.message_timing = {};
    }
}

Gs1Status GameRuntime::set_profiled_system_enabled(
    Gs1RuntimeProfileSystemId system_id,
    bool enabled) noexcept
{
    if (!is_valid_profile_system_id(system_id))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    profiled_systems_[static_cast<std::size_t>(system_id)].enabled = enabled;
    return GS1_STATUS_OK;
}

bool GameRuntime::profiled_system_enabled(Gs1RuntimeProfileSystemId system_id) const noexcept
{
    return is_valid_profile_system_id(system_id) &&
        profiled_systems_[static_cast<std::size_t>(system_id)].enabled;
}

void GameRuntime::copy_timing_snapshot(
    const TimingAccumulator& source,
    Gs1RuntimeTimingStats& destination) const noexcept
{
    destination.sample_count = source.sample_count;
    destination.last_elapsed_ms = source.last_elapsed_ms;
    destination.total_elapsed_ms = source.total_elapsed_ms;
    destination.max_elapsed_ms = source.max_elapsed_ms;
}

void GameRuntime::initialize_subscription_tables()
{
#ifndef NDEBUG
    const std::array site_system_access_registry {
        ActionExecutionSystem::access(),
        WeatherEventSystem::access(),
        WorkerConditionSystem::access(),
        EcologySystem::access(),
        PlantWeatherContributionSystem::access(),
        DeviceWeatherContributionSystem::access(),
        TaskBoardSystem::access(),
        PlacementValidationSystem::access(),
        LocalWeatherResolveSystem::access(),
        InventorySystem::access(),
        CraftSystem::access(),
        EconomyPhoneSystem::access(),
        PhonePanelSystem::access(),
        CampDurabilitySystem::access(),
        DeviceSupportSystem::access(),
        DeviceMaintenanceSystem::access(),
        ModifierSystem::access(),
        FailureRecoverySystem::access(),
        SiteCompletionSystem::access(),
        SiteTimeSystem::access(),
        SiteFlowSystem::access()};
    const auto validation = validate_site_system_access_registry(site_system_access_registry);
    if (!validation.ok)
    {
        std::fprintf(
            stderr,
            "Invalid site system access registry: %.*s system=%.*s component=%.*s other=%.*s\n",
            static_cast<int>(validation.message.size()),
            validation.message.data(),
            static_cast<int>(validation.system_name.size()),
            validation.system_name.data(),
            static_cast<int>(site_component_name(validation.component).size()),
            site_component_name(validation.component).data(),
            static_cast<int>(validation.other_system_name.size()),
            validation.other_system_name.data());
        assert(false && "Invalid site system access registry");
    }
#endif

    for (std::size_t index = 0; index < k_game_message_type_count; ++index)
    {
        const auto type = static_cast<GameMessageType>(index);
        auto& subscribers = message_subscribers_[index];

        if (CampaignFlowSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::CampaignFlow);
        }

        if (LoadoutPlannerSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::LoadoutPlanner);
        }

        if (FactionReputationSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::FactionReputation);
        }

        if (TechnologySystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Technology);
        }

        if (ActionExecutionSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::ActionExecution);
        }

        if (WeatherEventSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::WeatherEvent);
        }

        if (WorkerConditionSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::WorkerCondition);
        }

        if (EcologySystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Ecology);
        }

        if (PlantWeatherContributionSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::PlantWeatherContribution);
        }

        if (DeviceWeatherContributionSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::DeviceWeatherContribution);
        }

        if (TaskBoardSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::TaskBoard);
        }

        if (PlacementValidationSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::PlacementValidation);
        }

        if (LocalWeatherResolveSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::LocalWeatherResolve);
        }

        if (DeviceMaintenanceSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::DeviceMaintenance);
        }

        if (InventorySystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Inventory);
        }

        if (CraftSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Craft);
        }

        if (EconomyPhoneSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::EconomyPhone);
        }

        if (PhonePanelSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::PhonePanel);
        }

        if (CampDurabilitySystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::CampDurability);
        }

        if (DeviceSupportSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::DeviceSupport);
        }

        if (ModifierSystem::subscribes_to(type))
        {
            subscribers.push_back(MessageSubscriberId::Modifier);
        }
    }
}

Gs1Status GameRuntime::submit_host_events(
    const Gs1HostEvent* events,
    std::uint32_t event_count)
{
    if (event_count > 0U && events == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    for (std::uint32_t i = 0; i < event_count; ++i)
    {
        host_events_.push_back(events[i]);
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::submit_feedback_events(
    const Gs1FeedbackEvent* events,
    std::uint32_t event_count)
{
    if (event_count > 0U && events == nullptr)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    for (std::uint32_t i = 0; i < event_count; ++i)
    {
        feedback_events_.push_back(events[i]);
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::run_phase1(const Gs1Phase1Request& request, Gs1Phase1Result& out_result)
{
    if (request.struct_size != sizeof(Gs1Phase1Request))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto phase_started_at = std::chrono::steady_clock::now();
    const auto finish_phase = [this, phase_started_at]() noexcept
    {
        record_timing_sample(phase1_timing_, elapsed_milliseconds_since(phase_started_at));
    };

    out_result = {};
    out_result.struct_size = sizeof(Gs1Phase1Result);
    phase1_site_move_direction_ = {};

    auto status = GS1_STATUS_OK;
    if (!boot_initialized_)
    {
        GameMessage boot_message {};
        boot_message.type = GameMessageType::OpenMainMenu;
        boot_message.set_payload(OpenMainMenuMessage {});
        message_queue_.push_back(boot_message);

        status = dispatch_queued_messages();
        if (status != GS1_STATUS_OK)
        {
            finish_phase();
            return status;
        }

        boot_initialized_ = true;
    }

    status = dispatch_host_events(HostEventDispatchStage::Phase1, out_result.processed_host_event_count);
    if (status != GS1_STATUS_OK)
    {
        phase1_site_move_direction_ = {};
        finish_phase();
        return status;
    }

    if (!active_site_run_.has_value())
    {
        phase1_site_move_direction_ = {};
        out_result.engine_messages_queued = static_cast<std::uint32_t>(engine_messages_.size());
        finish_phase();
        return GS1_STATUS_OK;
    }

    active_site_run_->clock.accumulator_real_seconds += request.real_delta_seconds;

    while (active_site_run_->clock.accumulator_real_seconds >= fixed_step_seconds_)
    {
        active_site_run_->clock.accumulator_real_seconds -= fixed_step_seconds_;
        run_fixed_step();
        out_result.fixed_steps_executed += 1U;
    }

    flush_site_presentation_if_dirty();

    status = dispatch_queued_messages();
    phase1_site_move_direction_ = {};
    out_result.engine_messages_queued = static_cast<std::uint32_t>(engine_messages_.size());
    finish_phase();
    return status;
}

Gs1Status GameRuntime::run_phase2(const Gs1Phase2Request& request, Gs1Phase2Result& out_result)
{
    if (request.struct_size != sizeof(Gs1Phase2Request))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto phase_started_at = std::chrono::steady_clock::now();
    const auto finish_phase = [this, phase_started_at]() noexcept
    {
        record_timing_sample(phase2_timing_, elapsed_milliseconds_since(phase_started_at));
    };

    out_result = {};
    out_result.struct_size = sizeof(Gs1Phase2Result);

    auto status = dispatch_host_events(HostEventDispatchStage::Phase2, out_result.processed_host_event_count);
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    status = dispatch_feedback_events(out_result.processed_feedback_event_count);
    if (status != GS1_STATUS_OK)
    {
        finish_phase();
        return status;
    }

    status = dispatch_queued_messages();
    out_result.engine_messages_queued = static_cast<std::uint32_t>(engine_messages_.size());
    finish_phase();
    return status;
}

Gs1Status GameRuntime::pop_engine_message(Gs1EngineMessage& out_message)
{
    if (engine_messages_.empty())
    {
        return GS1_STATUS_BUFFER_EMPTY;
    }

    out_message = engine_messages_.front();
    engine_messages_.pop_front();
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::handle_message(const GameMessage& message)
{
    message_queue_.push_back(message);
    return dispatch_queued_messages();
}

Gs1Status GameRuntime::dispatch_subscribed_message(const GameMessage& message)
{
    if (!is_valid_message_type(message.type) || message.type == GameMessageType::Count)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto dispatch_profiled_message = [this](Gs1RuntimeProfileSystemId system_id, auto&& dispatch_fn)
        -> Gs1Status
    {
        if (!profiled_system_enabled(system_id))
        {
            return GS1_STATUS_OK;
        }

        const auto started_at = std::chrono::steady_clock::now();
        const auto status = dispatch_fn();
        record_timing_sample(
            profiled_systems_[static_cast<std::size_t>(system_id)].message_timing,
            elapsed_milliseconds_since(started_at));
        return status;
    };

    const auto& subscribers = message_subscribers_[message_type_index(message.type)];
    for (const auto subscriber : subscribers)
    {
        switch (subscriber)
        {
        case MessageSubscriberId::CampaignFlow:
        {
            CampaignFlowMessageContext context {
                campaign_,
                active_site_run_,
                app_state_,
                message_queue_};
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW,
                [&]() -> Gs1Status {
                    return CampaignFlowSystem::process_message(context, message);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            app_state_ = context.app_state;
            break;
        }

        case MessageSubscriberId::LoadoutPlanner:
        {
            if (!campaign_.has_value())
            {
                break;
            }

            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_LOADOUT_PLANNER,
                [&]() -> Gs1Status {
                    CampaignSystemContext context {*campaign_};
                    return LoadoutPlannerSystem::process_message(context, message);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::FactionReputation:
        {
            if (!campaign_.has_value())
            {
                break;
            }

            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_FACTION_REPUTATION,
                [&]() -> Gs1Status {
                    CampaignSystemContext context {*campaign_};
                    return FactionReputationSystem::process_message(context, message);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Technology:
        {
            if (!campaign_.has_value())
            {
                break;
            }

            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_TECHNOLOGY,
                [&]() -> Gs1Status {
                    CampaignSystemContext context {*campaign_};
                    return TechnologySystem::process_message(context, message);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::ActionExecution:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<ActionExecutionSystem>(
                        ActionExecutionSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::WeatherEvent:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<WeatherEventSystem>(
                        WeatherEventSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::WorkerCondition:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<WorkerConditionSystem>(
                        WorkerConditionSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Ecology:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<EcologySystem>(
                        EcologySystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::TaskBoard:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<TaskBoardSystem>(
                        TaskBoardSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::PlantWeatherContribution:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<PlantWeatherContributionSystem>(
                        PlantWeatherContributionSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::DeviceWeatherContribution:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<DeviceWeatherContributionSystem>(
                        DeviceWeatherContributionSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::PlacementValidation:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_PLACEMENT_VALIDATION,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<PlacementValidationSystem>(
                        PlacementValidationSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::LocalWeatherResolve:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<LocalWeatherResolveSystem>(
                        LocalWeatherResolveSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Inventory:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<InventorySystem>(
                        InventorySystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Craft:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_CRAFT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<CraftSystem>(
                        CraftSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::EconomyPhone:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<EconomyPhoneSystem>(
                        EconomyPhoneSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::PhonePanel:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_PHONE_PANEL,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<PhonePanelSystem>(
                        PhonePanelSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::CampDurability:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<CampDurabilitySystem>(
                        CampDurabilitySystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::DeviceSupport:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<DeviceSupportSystem>(
                        DeviceSupportSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::DeviceMaintenance:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<DeviceMaintenanceSystem>(
                        DeviceMaintenanceSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        case MessageSubscriberId::Modifier:
        {
            const auto status = dispatch_profiled_message(
                GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER,
                [&]() -> Gs1Status {
                    return dispatch_site_system_message<ModifierSystem>(
                        ModifierSystem::process_message,
                        message,
                        campaign_,
                        active_site_run_,
                        message_queue_,
                        fixed_step_seconds_);
                });
            if (status != GS1_STATUS_OK)
            {
                return status;
            }
            break;
        }

        default:
            return GS1_STATUS_INVALID_ARGUMENT;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_subscribed_feedback_event(const Gs1FeedbackEvent& event)
{
    if (!is_valid_feedback_event_type(event.type))
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    const auto& subscribers = feedback_event_subscribers_[feedback_event_type_index(event.type)];
    for (const auto subscriber : subscribers)
    {
        (void)subscriber;
    }

    return GS1_STATUS_OK;
}

void GameRuntime::sync_after_processed_message(const GameMessage& message)
{
    switch (message.type)
    {
    case GameMessageType::OpenMainMenu:
        queue_close_active_normal_ui_if_open();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_app_state_message(app_state_);
        queue_main_menu_ui_messages();
        queue_log_message("Entered main menu.");
        break;

    case GameMessageType::StartNewCampaign:
        queue_close_ui_setup_if_open(GS1_UI_SETUP_MAIN_MENU);
        queue_app_state_message(app_state_);
        queue_campaign_resources_message();
        queue_regional_map_snapshot_messages();
        queue_regional_map_menu_ui_messages();
        queue_regional_map_selection_ui_messages();
        queue_log_message("Started new GS1 campaign.");
        break;

    case GameMessageType::SelectDeploymentSite:
        queue_regional_map_snapshot_messages();
        queue_regional_map_menu_ui_messages();
        queue_regional_map_selection_ui_messages();
        queue_log_message("Selected deployment site.");
        break;

    case GameMessageType::ClearDeploymentSiteSelection:
        queue_regional_map_snapshot_messages();
        queue_regional_map_menu_ui_messages();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_SELECTION);
        queue_log_message("Cleared deployment site selection.");
        break;

    case GameMessageType::OpenRegionalMapTechTree:
        if (site_protection_selector_open_)
        {
            site_protection_selector_open_ = false;
            queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        }
        if (site_protection_overlay_mode_ != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
            queue_site_protection_overlay_state_message();
        }
        queue_close_site_inventory_panels_if_open();
        queue_close_site_phone_panel_if_open();
        queue_regional_map_menu_ui_messages();
        queue_regional_map_tech_tree_ui_messages();
        break;

    case GameMessageType::CloseRegionalMapTechTree:
        queue_regional_map_menu_ui_messages();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        break;

    case GameMessageType::SelectRegionalMapTechTreeFaction:
        queue_regional_map_menu_ui_messages();
        queue_regional_map_tech_tree_ui_messages();
        break;

    case GameMessageType::CampaignCashDeltaRequested:
    case GameMessageType::CampaignReputationAwardRequested:
    case GameMessageType::FactionReputationAwardRequested:
    case GameMessageType::TechnologyNodeClaimRequested:
    case GameMessageType::TechnologyNodeRefundRequested:
    case GameMessageType::EconomyMoneyAwardRequested:
    case GameMessageType::PhoneCartCheckoutRequested:
        queue_campaign_resources_message();
        if (app_state_supports_technology_tree(app_state_))
        {
            queue_regional_map_menu_ui_messages();
            queue_regional_map_tech_tree_ui_messages();
        }
        if (message.type == GameMessageType::CampaignReputationAwardRequested ||
            message.type == GameMessageType::FactionReputationAwardRequested ||
            message.type == GameMessageType::TechnologyNodeClaimRequested ||
            message.type == GameMessageType::TechnologyNodeRefundRequested)
        {
            sync_campaign_unlock_presentations();
        }
        break;

    case GameMessageType::PhonePanelSectionRequested:
        if (site_protection_selector_open_)
        {
            site_protection_selector_open_ = false;
            queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        }
        if (site_protection_overlay_mode_ != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
            queue_site_protection_overlay_state_message();
        }
        queue_close_site_inventory_panels_if_open();
        if (campaign_.has_value() &&
            app_state_supports_technology_tree(app_state_) &&
            campaign_->regional_map_state.tech_tree_open)
        {
            GameMessage close_tech_tree {};
            close_tech_tree.type = GameMessageType::CloseRegionalMapTechTree;
            close_tech_tree.set_payload(CloseRegionalMapTechTreeMessage {});
            message_queue_.push_back(close_tech_tree);
        }
        break;

    case GameMessageType::ClosePhonePanel:
        break;

    case GameMessageType::InventoryStorageViewRequest:
    {
        const auto& payload = message.payload_as<InventoryStorageViewRequestMessage>();
        if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            if (site_protection_selector_open_)
            {
                site_protection_selector_open_ = false;
                queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
            }
            if (site_protection_overlay_mode_ != GS1_SITE_PROTECTION_OVERLAY_NONE)
            {
                site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
                queue_site_protection_overlay_state_message();
            }
            queue_close_site_phone_panel_if_open();
            if (campaign_.has_value() &&
                app_state_supports_technology_tree(app_state_) &&
                campaign_->regional_map_state.tech_tree_open)
            {
                GameMessage close_tech_tree {};
                close_tech_tree.type = GameMessageType::CloseRegionalMapTechTree;
                close_tech_tree.set_payload(CloseRegionalMapTechTreeMessage {});
                message_queue_.push_back(close_tech_tree);
            }
        }
        break;
    }

    case GameMessageType::StartSiteAttempt:
        close_site_protection_ui();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_SELECTION);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_MENU);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_app_state_message(app_state_);
        break;

    case GameMessageType::SiteRunStarted:
        close_site_protection_ui();
        queue_site_bootstrap_messages();
        queue_campaign_resources_message();
        queue_site_action_update_message();
        queue_hud_state_message();
        queue_log_message("Started site attempt.");
        break;

    case GameMessageType::ReturnToRegionalMap:
        close_site_protection_ui();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        queue_app_state_message(app_state_);
        queue_campaign_resources_message();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_RESULT);
        queue_regional_map_snapshot_messages();
        queue_regional_map_menu_ui_messages();
        queue_regional_map_selection_ui_messages();
        break;

    case GameMessageType::SiteAttemptEnded:
    {
        const auto& payload = message.payload_as<SiteAttemptEndedMessage>();
        const auto newly_revealed_site_count =
            active_site_run_.has_value() ? active_site_run_->result_newly_revealed_site_count : 0U;
        close_site_protection_ui();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_app_state_message(app_state_);
        queue_site_result_ui_messages(payload.site_id, payload.result);
        queue_site_result_ready_message(
            payload.site_id,
            payload.result,
            newly_revealed_site_count);
        break;
    }

    case GameMessageType::PresentLog:
    {
        const auto& payload = message.payload_as<PresentLogMessage>();
        queue_log_message(payload.text, payload.level);
        break;
    }

    case GameMessageType::TaskRewardClaimResolved:
    {
        const auto& payload = message.payload_as<TaskRewardClaimResolvedMessage>();
        flush_site_presentation_if_dirty();
        queue_task_reward_claimed_cue_message(
            payload.task_instance_id,
            payload.task_template_id,
            payload.reward_candidate_count);
        break;
    }

    case GameMessageType::SiteActionStarted:
    case GameMessageType::SiteActionCompleted:
    case GameMessageType::SiteActionFailed:
        queue_site_action_update_message();
        break;

    case GameMessageType::PlacementModeCommitRejected:
        queue_site_placement_failure_message(
            message.payload_as<PlacementModeCommitRejectedMessage>());
        break;

    case GameMessageType::OpenSiteProtectionSelector:
        if (!active_site_run_.has_value() ||
            app_state_ != GS1_APP_STATE_SITE_ACTIVE ||
            active_site_run_->site_action.placement_mode.active)
        {
            break;
        }
        queue_close_site_inventory_panels_if_open();
        queue_close_site_phone_panel_if_open();
        if (campaign_.has_value() &&
            app_state_supports_technology_tree(app_state_) &&
            campaign_->regional_map_state.tech_tree_open)
        {
            GameMessage close_tech_tree {};
            close_tech_tree.type = GameMessageType::CloseRegionalMapTechTree;
            close_tech_tree.set_payload(CloseRegionalMapTechTreeMessage {});
            message_queue_.push_back(close_tech_tree);
        }
        if (site_protection_overlay_mode_ != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
            queue_site_protection_overlay_state_message();
        }
        site_protection_selector_open_ = true;
        queue_site_protection_selector_ui_messages();
        break;

    case GameMessageType::CloseSiteProtectionUi:
        if (site_protection_selector_open_)
        {
            site_protection_selector_open_ = false;
            queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        }
        else if (site_protection_overlay_mode_ != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
            queue_site_protection_overlay_state_message();
        }
        break;

    case GameMessageType::SetSiteProtectionOverlayMode:
        if (!active_site_run_.has_value() ||
            app_state_ != GS1_APP_STATE_SITE_ACTIVE ||
            active_site_run_->site_action.placement_mode.active)
        {
            break;
        }
        site_protection_selector_open_ = false;
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        site_protection_overlay_mode_ =
            message.payload_as<SetSiteProtectionOverlayModeMessage>().mode;
        queue_site_protection_overlay_state_message();
        break;

    case GameMessageType::DeploymentSiteSelectionChanged:
    case GameMessageType::StartSiteAction:
    case GameMessageType::CancelSiteAction:
    case GameMessageType::SiteGroundCoverPlaced:
    case GameMessageType::SiteTilePlantingCompleted:
    case GameMessageType::SiteTileWatered:
    case GameMessageType::SiteTileBurialCleared:
    case GameMessageType::WorkerMeterDeltaRequested:
    case GameMessageType::WorkerMetersChanged:
    case GameMessageType::TileEcologyChanged:
    case GameMessageType::TileEcologyBatchChanged:
    case GameMessageType::RestorationProgressChanged:
    case GameMessageType::TaskAcceptRequested:
    case GameMessageType::TaskRewardClaimRequested:
    case GameMessageType::PhoneListingPurchased:
    case GameMessageType::PhoneListingSold:
    case GameMessageType::InventoryTransferCompleted:
    case GameMessageType::InventoryItemSubmitted:
    case GameMessageType::InventoryItemUseCompleted:
    case GameMessageType::InventoryCraftCompleted:
        if (active_site_run_.has_value() &&
            active_site_run_->site_action.placement_mode.active &&
            site_protection_overlay_mode_ != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
            queue_site_protection_overlay_state_message();
        }
        break;

    case GameMessageType::InventoryDeliveryRequested:
    case GameMessageType::InventoryItemUseRequested:
    case GameMessageType::InventoryItemConsumeRequested:
    case GameMessageType::InventoryTransferRequested:
    case GameMessageType::InventoryItemSubmitRequested:
    case GameMessageType::InventoryCraftContextRequested:
        if (active_site_run_.has_value() &&
            active_site_run_->site_action.placement_mode.active &&
            site_protection_overlay_mode_ != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
            queue_site_protection_overlay_state_message();
        }
        break;

    case GameMessageType::PhoneListingPurchaseRequested:
    case GameMessageType::PhoneListingSaleRequested:
    case GameMessageType::ContractorHireRequested:
    case GameMessageType::SiteUnlockablePurchaseRequested:
        queue_campaign_resources_message();
        [[fallthrough]];
    default:
        if (active_site_run_.has_value() &&
            active_site_run_->site_action.placement_mode.active &&
            site_protection_overlay_mode_ != GS1_SITE_PROTECTION_OVERLAY_NONE)
        {
            site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
            queue_site_protection_overlay_state_message();
        }
        break;
    }
}

void GameRuntime::queue_log_message(const char* message, Gs1LogLevel level)
{
    auto engine_message = make_engine_message(GS1_ENGINE_MESSAGE_LOG_TEXT);
    auto& payload = engine_message.emplace_payload<Gs1EngineMessageLogTextData>();
    payload.level = level;
    strncpy_s(
        payload.text,
        sizeof(payload.text),
        message,
        _TRUNCATE);
    engine_messages_.push_back(engine_message);
}

void GameRuntime::queue_app_state_message(Gs1AppState app_state)
{
    if (last_emitted_app_state_.has_value() && last_emitted_app_state_.value() == app_state)
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SET_APP_STATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageSetAppStateData>();
    payload.app_state = app_state;
    engine_messages_.push_back(message);
    last_emitted_app_state_ = app_state;
}

void GameRuntime::queue_ui_setup_begin_message(
    Gs1UiSetupId setup_id,
    Gs1UiSetupPresentationType presentation_type,
    std::uint32_t element_count,
    std::uint32_t context_id)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiSetupData>();
    payload.setup_id = setup_id;
    payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    payload.presentation_type = presentation_type;
    payload.element_count = static_cast<std::uint16_t>(element_count);
    payload.context_id = context_id;
    engine_messages_.push_back(message);

    active_ui_setups_[setup_id] = presentation_type;
    if (presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
    {
        active_normal_ui_setup_ = setup_id;
    }
}

void GameRuntime::queue_ui_setup_close_message(
    Gs1UiSetupId setup_id,
    Gs1UiSetupPresentationType presentation_type)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP);
    auto& payload = message.emplace_payload<Gs1EngineMessageCloseUiSetupData>();
    payload.setup_id = setup_id;
    payload.presentation_type = presentation_type;
    engine_messages_.push_back(message);

    active_ui_setups_.erase(setup_id);
    if (active_normal_ui_setup_.has_value() && active_normal_ui_setup_.value() == setup_id)
    {
        active_normal_ui_setup_.reset();
    }
}

void GameRuntime::queue_close_ui_setup_if_open(Gs1UiSetupId setup_id)
{
    const auto it = active_ui_setups_.find(setup_id);
    if (it == active_ui_setups_.end())
    {
        return;
    }

    queue_ui_setup_close_message(setup_id, it->second);
}

void GameRuntime::queue_close_active_normal_ui_if_open()
{
    if (!active_normal_ui_setup_.has_value())
    {
        return;
    }

    queue_close_ui_setup_if_open(active_normal_ui_setup_.value());
}

void GameRuntime::queue_close_site_inventory_panels_if_open()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto queue_close_storage = [this](std::uint32_t storage_id)
    {
        if (storage_id == 0U)
        {
            return;
        }

        GameMessage message {};
        message.type = GameMessageType::InventoryStorageViewRequest;
        message.set_payload(InventoryStorageViewRequestMessage {
            storage_id,
            GS1_INVENTORY_VIEW_EVENT_CLOSE,
            {0U, 0U, 0U}});
        message_queue_.push_back(message);
    };

    const auto& inventory = active_site_run_->inventory;
    if (inventory.worker_pack_panel_open)
    {
        queue_close_storage(inventory.worker_pack_storage_id);
    }
    if (inventory.opened_device_storage_id != 0U)
    {
        queue_close_storage(inventory.opened_device_storage_id);
    }
    if (inventory.pending_device_storage_open.active &&
        inventory.pending_device_storage_open.storage_id != 0U &&
        inventory.pending_device_storage_open.storage_id != inventory.opened_device_storage_id)
    {
        queue_close_storage(inventory.pending_device_storage_open.storage_id);
    }
}

void GameRuntime::queue_close_site_phone_panel_if_open()
{
    if (!active_site_run_.has_value() || !active_site_run_->phone_panel.open)
    {
        return;
    }

    GameMessage message {};
    message.type = GameMessageType::ClosePhonePanel;
    message.set_payload(ClosePhonePanelMessage {});
    message_queue_.push_back(message);
}

void GameRuntime::queue_ui_element_message(
    std::uint32_t element_id,
    Gs1UiElementType element_type,
    std::uint32_t flags,
    const Gs1UiAction& action,
    const char* text)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiElementData>();
    payload.element_id = element_id;
    payload.element_type = element_type;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.action = action;
    strncpy_s(
        payload.text,
        sizeof(payload.text),
        text,
        _TRUNCATE);
    engine_messages_.push_back(message);
}

void GameRuntime::queue_ui_setup_end_message()
{
    engine_messages_.push_back(make_engine_message(GS1_ENGINE_MESSAGE_END_UI_SETUP));
}

void GameRuntime::queue_clear_ui_setup_messages(Gs1UiSetupId setup_id)
{
    queue_close_ui_setup_if_open(setup_id);
}

void GameRuntime::queue_main_menu_ui_messages()
{
    queue_ui_setup_begin_message(
        GS1_UI_SETUP_MAIN_MENU,
        GS1_UI_SETUP_PRESENTATION_NORMAL,
        1U,
        0U);

    Gs1UiAction action {};
    action.type = GS1_UI_ACTION_START_NEW_CAMPAIGN;
    action.arg0 = 42ULL;
    action.arg1 = 30ULL;
    queue_ui_element_message(
        1U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        action,
        "Start New Campaign");

    queue_ui_setup_end_message();
}

void GameRuntime::queue_regional_map_menu_ui_messages()
{
    if (!campaign_.has_value() || app_state_ != GS1_APP_STATE_REGIONAL_MAP)
    {
        return;
    }

    queue_ui_setup_begin_message(
        GS1_UI_SETUP_REGIONAL_MAP_MENU,
        GS1_UI_SETUP_PRESENTATION_NORMAL,
        2U,
        0U);

    Gs1UiAction no_action {};
    const auto selected_faction_id = campaign_->regional_map_state.selected_tech_tree_faction_id.value != 0U
        ? campaign_->regional_map_state.selected_tech_tree_faction_id
        : FactionId {k_faction_village_committee};
    const auto faction_tab_label = [](FactionId faction_id) noexcept -> const char*
    {
        switch (faction_id.value)
        {
        case k_faction_village_committee:
            return "Village";
        case k_faction_forestry_bureau:
            return "Forestry";
        case k_faction_agricultural_university:
            return "University";
        default:
            return "Faction";
        }
    };
    const auto* selected_faction_def = find_faction_def(selected_faction_id);
    char summary_text[96] {};
    std::snprintf(
        summary_text,
        sizeof(summary_text),
        "%.*s Rep %d",
        selected_faction_def == nullptr ? 0 : static_cast<int>(selected_faction_def->display_name.size()),
        selected_faction_def == nullptr ? "" : selected_faction_def->display_name.data(),
        TechnologySystem::faction_reputation(*campaign_, selected_faction_id));
    queue_ui_element_message(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        summary_text);

    Gs1UiAction toggle_action {};
    toggle_action.type = campaign_->regional_map_state.tech_tree_open
        ? GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE
        : GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;
    queue_ui_element_message(
        2U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        toggle_action,
        campaign_->regional_map_state.tech_tree_open ? "Close Tech Tree" : "Open Tech Tree");

    queue_ui_setup_end_message();
}

void GameRuntime::queue_regional_map_selection_ui_messages()
{
    if (!campaign_.has_value() || !campaign_->regional_map_state.selected_site_id.has_value())
    {
        return;
    }

    const auto site_id = campaign_->regional_map_state.selected_site_id->value;
    const auto loadout_label_count = visible_loadout_slot_count(campaign_->loadout_planner_state);
    const bool has_support_contributors = campaign_->loadout_planner_state.support_quota > 0U;
    const bool has_aura_support = !campaign_->loadout_planner_state.active_nearby_aura_modifier_ids.empty();
    const std::uint32_t summary_label_count =
        (has_support_contributors ? 1U : 0U) + (has_aura_support ? 1U : 0U);

    queue_ui_setup_begin_message(
        GS1_UI_SETUP_REGIONAL_MAP_SELECTION,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        static_cast<std::uint32_t>(3U + summary_label_count + loadout_label_count),
        site_id);

    Gs1UiAction no_action {};
    char label_text[64] {};
    std::snprintf(label_text, sizeof(label_text), "Selected Site %u", static_cast<unsigned>(site_id));
    queue_ui_element_message(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        label_text);

    std::uint32_t next_element_id = 2U;
    if (has_support_contributors)
    {
        char support_text[64] {};
        std::snprintf(
            support_text,
            sizeof(support_text),
            "Adj Support x%u",
            static_cast<unsigned>(campaign_->loadout_planner_state.support_quota));
        queue_ui_element_message(
            next_element_id++,
            GS1_UI_ELEMENT_LABEL,
            GS1_UI_ELEMENT_FLAG_NONE,
            no_action,
            support_text);
    }

    if (has_aura_support)
    {
        char aura_text[64] {};
        std::snprintf(
            aura_text,
            sizeof(aura_text),
            "Aura Ready x%u",
            static_cast<unsigned>(campaign_->loadout_planner_state.active_nearby_aura_modifier_ids.size()));
        queue_ui_element_message(
            next_element_id++,
            GS1_UI_ELEMENT_LABEL,
            GS1_UI_ELEMENT_FLAG_NONE,
            no_action,
            aura_text);
    }

    for (const auto& slot : campaign_->loadout_planner_state.selected_loadout_slots)
    {
        if (!slot.occupied || slot.item_id.value == 0U || slot.quantity == 0U)
        {
            continue;
        }

        char loadout_text[64] {};
        if (const auto* item_def = find_item_def(slot.item_id); item_def != nullptr)
        {
            std::snprintf(
                loadout_text,
                sizeof(loadout_text),
                "%.*s x%u",
                static_cast<int>(item_def->display_name.size()),
                item_def->display_name.data(),
                static_cast<unsigned>(slot.quantity));
        }
        else
        {
            std::snprintf(
                loadout_text,
                sizeof(loadout_text),
                "Item %u x%u",
                static_cast<unsigned>(slot.item_id.value),
                static_cast<unsigned>(slot.quantity));
        }

        queue_ui_element_message(
            next_element_id++,
            GS1_UI_ELEMENT_LABEL,
            GS1_UI_ELEMENT_FLAG_NONE,
            no_action,
            loadout_text);
    }

    Gs1UiAction deploy_action {};
    deploy_action.type = GS1_UI_ACTION_START_SITE_ATTEMPT;
    deploy_action.target_id = site_id;
    char button_text[64] {};
    std::snprintf(button_text, sizeof(button_text), "Start Site %u", static_cast<unsigned>(site_id));
    queue_ui_element_message(
        next_element_id++,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        deploy_action,
        button_text);

    Gs1UiAction clear_selection_action {};
    clear_selection_action.type = GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    queue_ui_element_message(
        next_element_id,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_BACKGROUND_CLICK,
        clear_selection_action,
        "");

    queue_ui_setup_end_message();
}

void GameRuntime::queue_regional_map_tech_tree_ui_messages()
{
    if (!campaign_.has_value() ||
        !app_state_supports_technology_tree(app_state_) ||
        !campaign_->regional_map_state.tech_tree_open)
    {
        return;
    }

    const auto selected_faction_id = campaign_->regional_map_state.selected_tech_tree_faction_id.value != 0U
        ? campaign_->regional_map_state.selected_tech_tree_faction_id
        : FactionId {k_faction_village_committee};
    const auto faction_tab_label = [](FactionId faction_id) noexcept -> const char*
    {
        switch (faction_id.value)
        {
        case k_faction_village_committee:
            return "Village";
        case k_faction_forestry_bureau:
            return "Forestry";
        case k_faction_agricultural_university:
            return "University";
        default:
            return "Faction";
        }
    };
    const auto reputation_unlock_defs = all_reputation_unlock_defs();
    const auto technology_tier_defs = all_technology_tier_defs();
    const auto technology_node_defs = all_technology_node_defs();
    std::uint32_t element_count = 8U + static_cast<std::uint32_t>(k_prototype_faction_defs.size());
    element_count += static_cast<std::uint32_t>(reputation_unlock_defs.size());
    element_count += static_cast<std::uint32_t>(technology_tier_defs.size());
    element_count += static_cast<std::uint32_t>(technology_node_defs.size());
    element_count += 1U;

    queue_ui_setup_begin_message(
        GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        element_count,
        0U);

    Gs1UiAction no_action {};
    queue_ui_element_message(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        "Prototype Tech Tree");

    char summary_text[64] {};
    const float site_cash = active_site_run_.has_value()
        ? cash_value_from_cash_points(active_site_run_->economy.current_cash)
        : 0.0f;
    std::snprintf(
        summary_text,
        sizeof(summary_text),
        active_site_run_.has_value() ? "Total %d | Site $%.2f" : "Total %d | Auto Rep",
        campaign_->technology_state.total_reputation,
        site_cash);
    queue_ui_element_message(
        2U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        summary_text);

    char faction_text[96] {};
    std::snprintf(
        faction_text,
        sizeof(faction_text),
        "%s Rep %d",
        faction_tab_label(selected_faction_id),
        TechnologySystem::faction_reputation(*campaign_, selected_faction_id));
    queue_ui_element_message(
        3U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        faction_text);

    queue_ui_element_message(
        4U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        "Starter Plants");

    queue_ui_element_message(
        5U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        "Checkerboard | Wormwood");

    queue_ui_element_message(
        6U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        "Reputation Unlocks");

    queue_ui_element_message(
        7U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        "Tier Tech Tree");

    queue_ui_element_message(
        8U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        "32 linear tiers per faction");

    std::uint32_t next_element_id = 9U;

    for (const auto& unlock_def : reputation_unlock_defs)
    {
        char unlock_text[96] {};
        std::snprintf(
            unlock_text,
            sizeof(unlock_text),
            "%.*s (%.*s) Need Total %d | %s",
            static_cast<int>(unlock_def.display_name.size()),
            unlock_def.display_name.data(),
            static_cast<int>(reputation_unlock_kind_display_name(unlock_def.unlock_kind).size()),
            reputation_unlock_kind_display_name(unlock_def.unlock_kind).data(),
            unlock_def.reputation_requirement,
            campaign_->technology_state.total_reputation >= unlock_def.reputation_requirement
                ? "Ready"
                : "Locked");
        queue_ui_element_message(
            next_element_id++,
            GS1_UI_ELEMENT_LABEL,
            GS1_UI_ELEMENT_FLAG_NONE,
            no_action,
            unlock_text);
    }

    for (const auto& faction_def : k_prototype_faction_defs)
    {
        Gs1UiAction tab_action {};
        tab_action.type = GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB;
        tab_action.target_id = faction_def.faction_id.value;
        const auto tab_flags =
            faction_def.faction_id == selected_faction_id ? GS1_UI_ELEMENT_FLAG_PRIMARY : GS1_UI_ELEMENT_FLAG_NONE;
        char tab_text[64] {};
        std::snprintf(
            tab_text,
            sizeof(tab_text),
            "Tab: %s",
            faction_tab_label(faction_def.faction_id));
        queue_ui_element_message(
            next_element_id++,
            GS1_UI_ELEMENT_BUTTON,
            tab_flags,
            tab_action,
            tab_text);
    }

    for (const auto& tier_def : technology_tier_defs)
    {
        char tier_text[128] {};
        std::snprintf(
            tier_text,
            sizeof(tier_text),
            "Tier %u | %.*s",
            static_cast<unsigned>(tier_def.tier_index),
            static_cast<int>(tier_def.display_name.size()),
            tier_def.display_name.data());
        queue_ui_element_message(
            next_element_id++,
            GS1_UI_ELEMENT_LABEL,
            GS1_UI_ELEMENT_FLAG_NONE,
            no_action,
            tier_text);

        for (const auto& node_def : technology_node_defs)
        {
            if (node_def.tier_index != tier_def.tier_index)
            {
                continue;
            }

            const auto reputation_requirement =
                TechnologySystem::current_reputation_requirement(node_def);

            Gs1UiAction action {};
            action.target_id = node_def.tech_node_id.value;
            action.arg0 = 0U;

            char node_text[512] {};
            std::uint32_t flags = GS1_UI_ELEMENT_FLAG_DISABLED;
            if (TechnologySystem::node_purchased(*campaign_, node_def.tech_node_id))
            {
                std::snprintf(
                    node_text,
                    sizeof(node_text),
                    "TECHNODE|s=Unlocked");
            }
            else
            {
                std::snprintf(
                    node_text,
                    sizeof(node_text),
                    "TECHNODE|s=Need %dr",
                    reputation_requirement);
            }

            queue_ui_element_message(
                next_element_id++,
                GS1_UI_ELEMENT_BUTTON,
                flags,
                action,
                node_text);
        }
    }

    Gs1UiAction close_action {};
    close_action.type = GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE;
    queue_ui_element_message(
        next_element_id,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_BACKGROUND_CLICK,
        close_action,
        "");

    queue_ui_setup_end_message();
}

void GameRuntime::queue_site_result_ui_messages(std::uint32_t site_id, Gs1SiteAttemptResult result)
{
    queue_ui_setup_begin_message(
        GS1_UI_SETUP_SITE_RESULT,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        2U,
        site_id);

    Gs1UiAction no_action {};
    char label_text[64] {};
    const char* result_text = result == GS1_SITE_ATTEMPT_RESULT_COMPLETED ? "completed" : "failed";
    std::snprintf(
        label_text,
        sizeof(label_text),
        "Site %u %s",
        static_cast<unsigned>(site_id),
        result_text);
    queue_ui_element_message(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        label_text);

    Gs1UiAction return_action {};
    return_action.type = GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP;
    queue_ui_element_message(
        2U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        return_action,
        "OK");

    queue_ui_setup_end_message();
}

void GameRuntime::queue_site_protection_selector_ui_messages()
{
    if (!active_site_run_.has_value() || app_state_ != GS1_APP_STATE_SITE_ACTIVE)
    {
        return;
    }

    queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
    queue_ui_setup_begin_message(
        GS1_UI_SETUP_SITE_PROTECTION_SELECTOR,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        5U,
        active_site_run_->site_id.value);

    Gs1UiAction no_action {};
    queue_ui_element_message(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        "Protection Overlay");

    Gs1UiAction wind_action {};
    wind_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    wind_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_WIND;
    queue_ui_element_message(
        2U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        wind_action,
        "Wind Protection");

    Gs1UiAction heat_action {};
    heat_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    heat_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_HEAT;
    queue_ui_element_message(
        3U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        heat_action,
        "Heat Protection");

    Gs1UiAction dust_action {};
    dust_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    dust_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_DUST;
    queue_ui_element_message(
        4U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        dust_action,
        "Dust Protection");

    Gs1UiAction close_action {};
    close_action.type = GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI;
    queue_ui_element_message(
        5U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_BACKGROUND_CLICK,
        close_action,
        "");

    queue_ui_setup_end_message();
}

void GameRuntime::queue_regional_map_snapshot_messages()
{
    if (!campaign_.has_value())
    {
        return;
    }

    auto begin = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT);
    auto& begin_payload = begin.emplace_payload<Gs1EngineMessageRegionalMapSnapshotData>();
    begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    begin_payload.site_count = static_cast<std::uint32_t>(campaign_->sites.size());

    std::set<std::pair<std::uint32_t, std::uint32_t>> unique_links;
    for (const auto& site : campaign_->sites)
    {
        for (const auto& adjacent_site_id : site.adjacent_site_ids)
        {
            const auto low = (site.site_id.value < adjacent_site_id.value) ? site.site_id.value : adjacent_site_id.value;
            const auto high = (site.site_id.value < adjacent_site_id.value) ? adjacent_site_id.value : site.site_id.value;
            unique_links.insert({low, high});
        }
    }

    begin_payload.link_count = static_cast<std::uint32_t>(unique_links.size());
    begin_payload.selected_site_id =
        campaign_->regional_map_state.selected_site_id.has_value()
            ? campaign_->regional_map_state.selected_site_id->value
            : 0U;
    engine_messages_.push_back(begin);

    for (const auto& site : campaign_->sites)
    {
        queue_regional_map_site_upsert_message(site);
    }

    for (const auto& link : unique_links)
    {
        auto link_message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT);
        auto& payload = link_message.emplace_payload<Gs1EngineMessageRegionalMapLinkData>();
        payload.from_site_id = link.first;
        payload.to_site_id = link.second;
        engine_messages_.push_back(link_message);
    }

    engine_messages_.push_back(make_engine_message(GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT));
}

void GameRuntime::queue_regional_map_site_upsert_message(const SiteMetaState& site)
{
    if (!campaign_.has_value())
    {
        return;
    }

    auto site_message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT);
    auto& payload = site_message.emplace_payload<Gs1EngineMessageRegionalMapSiteData>();
    payload.site_id = site.site_id.value;
    payload.site_state = site.site_state;
    payload.site_archetype_id = site.site_archetype_id;
    payload.flags =
        (campaign_->regional_map_state.selected_site_id.has_value() &&
            campaign_->regional_map_state.selected_site_id->value == site.site_id.value)
        ? 1U
        : 0U;

    payload.map_x = site.regional_map_tile.x * k_regional_map_tile_spacing;
    payload.map_y = site.regional_map_tile.y * k_regional_map_tile_spacing;
    payload.support_package_id =
        site.has_support_package_id ? site.support_package_id : 0U;
    payload.support_preview_mask =
        site_contributes_to_selected_target(*campaign_, site)
            ? support_preview_mask_for_site(site)
            : 0U;
    engine_messages_.push_back(site_message);
}

void GameRuntime::queue_site_snapshot_begin_message(Gs1ProjectionMode mode)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto begin = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT);
    auto& begin_payload = begin.emplace_payload<Gs1EngineMessageSiteSnapshotData>();
    begin_payload.mode = mode;
    begin_payload.site_id = active_site_run_->site_id.value;
    begin_payload.site_archetype_id = active_site_run_->site_archetype_id;
    begin_payload.width = static_cast<std::uint16_t>(site_world_access::width(active_site_run_.value()));
    begin_payload.height = static_cast<std::uint16_t>(site_world_access::height(active_site_run_.value()));
    engine_messages_.push_back(begin);
}

void GameRuntime::queue_site_snapshot_end_message()
{
    engine_messages_.push_back(make_engine_message(GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT));
}

void GameRuntime::queue_site_tile_upsert_message(std::uint32_t x, std::uint32_t y)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    const TileCoord coord {
        static_cast<std::int32_t>(x),
        static_cast<std::int32_t>(y)};

    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return;
    }

    const auto tile = site_run.site_world->tile_at(coord);
    const auto total_protection = site_run.site_world->tile_total_weather_contribution(coord);
    const auto projected_state = capture_projected_tile_state(tile);
    const auto tile_index = site_world_access::tile_index(site_run, coord);
    if (tile_index >= site_run.last_projected_tile_states.size())
    {
        site_run.last_projected_tile_states.assign(site_run.site_world->tile_count(), ProjectedSiteTileState {});
    }

    if (tile_index < site_run.last_projected_tile_states.size() &&
        projected_tile_state_matches(
            site_run.last_projected_tile_states[tile_index],
            projected_state))
    {
        if (should_emit_site_one_runtime_probe(site_run, coord))
        {
            char log_text[64] {};
            std::snprintf(
                log_text,
                sizeof(log_text),
                "S1 pskip (%u,%u) d%.2f q%u",
                x,
                y,
                tile.ecology.plant_density,
                static_cast<unsigned>(projected_state.plant_density_quantized));
            queue_log_message(log_text, GS1_LOG_LEVEL_DEBUG);
        }
        return;
    }

    auto tile_message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    auto& payload = tile_message.emplace_payload<Gs1EngineMessageSiteTileData>();
    payload.x = static_cast<std::uint16_t>(x);
    payload.y = static_cast<std::uint16_t>(y);
    payload.terrain_type_id = tile.static_data.terrain_type_id;
    payload.plant_type_id = tile.ecology.plant_id.value;
    payload.structure_type_id = tile.device.structure_id.value;
    payload.ground_cover_type_id = tile.ecology.ground_cover_type_id;
    payload.plant_density = tile.ecology.plant_density;
    payload.sand_burial = tile.ecology.sand_burial;
    payload.local_wind = tile.local_weather.wind;
    payload.wind_protection = total_protection.wind_protection;
    payload.heat_protection = total_protection.heat_protection;
    payload.dust_protection = total_protection.dust_protection;
    payload.moisture = tile.ecology.moisture;
    payload.soil_fertility = tile.ecology.soil_fertility;
    payload.soil_salinity = tile.ecology.soil_salinity;
    payload.excavation_depth = projected_state.excavation_depth;
    payload.visible_excavation_depth = projected_state.visible_excavation_depth;
    payload.reserved0[0] = 0U;
    payload.reserved0[1] = 0U;
    engine_messages_.push_back(tile_message);

    if (should_emit_site_one_runtime_probe(site_run, coord))
    {
        char log_text[64] {};
        std::snprintf(
            log_text,
            sizeof(log_text),
            "S1 pup (%u,%u) p%u d%.2f q%u",
            x,
            y,
            payload.plant_type_id,
            payload.plant_density,
            static_cast<unsigned>(projected_state.plant_density_quantized));
        queue_log_message(log_text, GS1_LOG_LEVEL_DEBUG);
    }

    if (tile_index < site_run.last_projected_tile_states.size())
    {
        site_run.last_projected_tile_states[tile_index] = projected_state;
    }
}

void GameRuntime::queue_all_site_tile_upsert_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    const auto tile_count = site_run.site_world->tile_count();
    site_run.last_projected_tile_states.assign(tile_count, ProjectedSiteTileState {});

    const auto width = site_world_access::width(site_run);
    const auto height = site_world_access::height(site_run);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            queue_site_tile_upsert_message(x, y);
        }
    }
}

void GameRuntime::queue_pending_site_tile_upsert_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    if (site_run.pending_full_tile_projection_update || site_run.pending_tile_projection_updates.empty())
    {
        queue_all_site_tile_upsert_messages();
        return;
    }

    std::sort(
        site_run.pending_tile_projection_updates.begin(),
        site_run.pending_tile_projection_updates.end(),
        [](const TileCoord& lhs, const TileCoord& rhs) {
            if (lhs.y != rhs.y)
            {
                return lhs.y < rhs.y;
            }
            return lhs.x < rhs.x;
        });

    for (const auto& coord : site_run.pending_tile_projection_updates)
    {
        queue_site_tile_upsert_message(
            static_cast<std::uint32_t>(coord.x),
            static_cast<std::uint32_t>(coord.y));
    }
}

void GameRuntime::queue_site_worker_update_message()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto worker_position = site_world_access::worker_position(active_site_run_.value());
    const auto worker_conditions = site_world_access::worker_conditions(active_site_run_.value());

    auto worker_message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE);
    auto& worker_payload = worker_message.emplace_payload<Gs1EngineMessageWorkerData>();
    worker_payload.tile_x = worker_position.tile_x;
    worker_payload.tile_y = worker_position.tile_y;
    worker_payload.facing_degrees = worker_position.facing_degrees;
    worker_payload.health_normalized = worker_conditions.health / 100.0f;
    worker_payload.hydration_normalized = worker_conditions.hydration / 100.0f;
    worker_payload.energy_normalized =
        worker_conditions.energy_cap > 0.0f
            ? (worker_conditions.energy / worker_conditions.energy_cap)
            : 0.0f;
    worker_payload.current_action_kind =
        static_cast<Gs1SiteActionKind>(active_site_run_->site_action.action_kind);
    engine_messages_.push_back(worker_message);
}

void GameRuntime::queue_site_camp_update_message()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto camp_message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE);
    auto& camp_payload = camp_message.emplace_payload<Gs1EngineMessageCampData>();
    camp_payload.tile_x = active_site_run_->camp.camp_anchor_tile.x;
    camp_payload.tile_y = active_site_run_->camp.camp_anchor_tile.y;
    camp_payload.durability_normalized = active_site_run_->camp.camp_durability / 100.0f;
    camp_payload.flags =
        (active_site_run_->camp.delivery_point_operational ? 1U : 0U) |
        (active_site_run_->camp.shared_storage_access_enabled ? 2U : 0U);
    engine_messages_.push_back(camp_message);
}

void GameRuntime::queue_site_weather_update_message()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto weather_message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE);
    auto& weather_payload = weather_message.emplace_payload<Gs1EngineMessageWeatherData>();
    weather_payload.heat = active_site_run_->weather.weather_heat;
    weather_payload.wind = active_site_run_->weather.weather_wind;
    weather_payload.dust = active_site_run_->weather.weather_dust;
    weather_payload.wind_direction_degrees = active_site_run_->weather.weather_wind_direction_degrees;
    weather_payload.event_template_id =
        active_site_run_->event.active_event_template_id.has_value()
        ? active_site_run_->event.active_event_template_id->value
            : 0U;
    weather_payload.event_start_time_minutes =
        static_cast<float>(active_site_run_->event.start_time_minutes);
    weather_payload.event_peak_time_minutes =
        static_cast<float>(active_site_run_->event.peak_time_minutes);
    weather_payload.event_peak_duration_minutes =
        static_cast<float>(active_site_run_->event.peak_duration_minutes);
    weather_payload.event_end_time_minutes =
        static_cast<float>(active_site_run_->event.end_time_minutes);
    engine_messages_.push_back(weather_message);
}

void GameRuntime::queue_site_inventory_slot_upsert_message(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index,
    std::uint32_t storage_id,
    std::uint64_t container_owner_id,
    TileCoord container_tile)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    InventorySlot slot {};
    if (container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
    {
        if (slot_index >= site_run.inventory.worker_pack_slots.size())
        {
            return;
        }

        slot = site_run.inventory.worker_pack_slots[slot_index];
        storage_id = site_run.inventory.worker_pack_storage_id;
    }
    else if (container_kind == GS1_INVENTORY_CONTAINER_DEVICE_STORAGE)
    {
        auto container =
            storage_id != 0U
                ? inventory_storage::container_for_storage_id(site_run, storage_id)
                : inventory_storage::container_for_kind(site_run, container_kind, container_owner_id);
        if (!container.is_valid())
        {
            return;
        }

        if (storage_id == 0U)
        {
            storage_id = inventory_storage::storage_id_for_container(site_run, container);
        }
        container_owner_id = inventory_storage::owner_device_entity_id_for_container(site_run, container);
        container_tile = inventory_storage::container_tile_coord(site_run, container);
        const auto item_entity =
            inventory_storage::item_entity_for_slot(site_run, container, slot_index);
        inventory_storage::fill_projection_slot_from_entities(slot, item_entity);
    }
    else
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageInventorySlotData>();
    payload.item_id = slot.item_id.value;
    payload.item_instance_id = slot.item_instance_id;
    payload.condition = slot.item_condition;
    payload.freshness = slot.item_freshness;
    payload.storage_id = storage_id;
    payload.container_owner_id = static_cast<std::uint32_t>(container_owner_id);
    payload.quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(slot.item_quantity, 65535U));
    payload.slot_index = static_cast<std::uint16_t>(slot_index);
    payload.container_tile_x = static_cast<std::int16_t>(container_tile.x);
    payload.container_tile_y = static_cast<std::int16_t>(container_tile.y);
    payload.container_kind = container_kind;
    payload.flags = slot.occupied ? 1U : 0U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_site_inventory_storage_upsert_message(std::uint32_t storage_id)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto* storage_state =
        inventory_storage::storage_container_state_for_storage_id(active_site_run_.value(), storage_id);
    if (storage_state == nullptr)
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageInventoryStorageData>();
    payload.storage_id = storage_state->storage_id;
    payload.owner_entity_id = static_cast<std::uint32_t>(storage_state->owner_device_entity_id);
    payload.slot_count = static_cast<std::uint16_t>(std::min<std::size_t>(
        storage_state->slot_item_instance_ids.size(),
        65535U));
    payload.tile_x = static_cast<std::int16_t>(storage_state->tile_coord.x);
    payload.tile_y = static_cast<std::int16_t>(storage_state->tile_coord.y);
    payload.container_kind = storage_state->container_kind;
    payload.flags = static_cast<std::uint8_t>(storage_state->flags);
    engine_messages_.push_back(message);
}

void GameRuntime::queue_all_site_inventory_storage_upsert_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    for (const auto& storage : active_site_run_->inventory.storage_containers)
    {
        queue_site_inventory_storage_upsert_message(storage.storage_id);
    }
}

void GameRuntime::queue_site_inventory_view_state_message(
    std::uint32_t storage_id,
    Gs1InventoryViewEventKind event_kind,
    std::uint32_t slot_count)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageInventoryViewData>();
    payload.storage_id = storage_id;
    payload.slot_count = static_cast<std::uint16_t>(std::min<std::uint32_t>(slot_count, 65535U));
    payload.event_kind = event_kind;
    payload.flags = 0U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_all_site_inventory_slot_upsert_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    for (std::uint32_t index = 0; index < active_site_run_->inventory.worker_pack_slots.size(); ++index)
    {
        queue_site_inventory_slot_upsert_message(
            GS1_INVENTORY_CONTAINER_WORKER_PACK,
            index,
            active_site_run_->inventory.worker_pack_storage_id);
    }
}

void GameRuntime::queue_pending_site_inventory_slot_upsert_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    if (site_run.pending_inventory_storage_descriptor_projection_update ||
        site_run.pending_full_inventory_projection_update)
    {
        queue_all_site_inventory_storage_upsert_messages();
    }

    if (site_run.pending_full_inventory_projection_update ||
        site_run.pending_worker_pack_inventory_projection_updates.empty())
    {
        queue_all_site_inventory_slot_upsert_messages();
    }
    else
    {
        std::sort(
            site_run.pending_worker_pack_inventory_projection_updates.begin(),
            site_run.pending_worker_pack_inventory_projection_updates.end());
        for (const auto slot_index : site_run.pending_worker_pack_inventory_projection_updates)
        {
            queue_site_inventory_slot_upsert_message(
                GS1_INVENTORY_CONTAINER_WORKER_PACK,
                slot_index,
                site_run.inventory.worker_pack_storage_id);
        }
    }

    if (site_run.pending_inventory_view_state_projection_update)
    {
        const bool previous_worker_pack_open = site_run.inventory.last_projected_worker_pack_panel_open;
        const bool current_worker_pack_open = site_run.inventory.worker_pack_panel_open;
        if (previous_worker_pack_open && !current_worker_pack_open)
        {
            queue_site_inventory_view_state_message(
                site_run.inventory.worker_pack_storage_id,
                GS1_INVENTORY_VIEW_EVENT_CLOSE);
        }
        else if (!previous_worker_pack_open && current_worker_pack_open)
        {
            queue_site_inventory_view_state_message(
                site_run.inventory.worker_pack_storage_id,
                GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
                static_cast<std::uint32_t>(site_run.inventory.worker_pack_slots.size()));
        }

        const auto previous_storage_id = site_run.inventory.last_projected_opened_device_storage_id;
        const auto current_storage_id = site_run.inventory.opened_device_storage_id;
        if (previous_storage_id != 0U && previous_storage_id != current_storage_id)
        {
            queue_site_inventory_view_state_message(previous_storage_id, GS1_INVENTORY_VIEW_EVENT_CLOSE);
        }
        if (current_storage_id != 0U && current_storage_id != previous_storage_id)
        {
            const auto* storage_state =
                inventory_storage::storage_container_state_for_storage_id(site_run, current_storage_id);
            queue_site_inventory_view_state_message(
                current_storage_id,
                GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
                storage_state == nullptr
                    ? 0U
                    : static_cast<std::uint32_t>(storage_state->slot_item_instance_ids.size()));
        }
        site_run.inventory.last_projected_opened_device_storage_id = current_storage_id;
        site_run.inventory.last_projected_worker_pack_panel_open = current_worker_pack_open;
    }

    if (site_run.inventory.opened_device_storage_id == 0U)
    {
        return;
    }

    const auto* opened_storage =
        inventory_storage::storage_container_state_for_storage_id(
            site_run,
            site_run.inventory.opened_device_storage_id);
    if (opened_storage == nullptr)
    {
        return;
    }

    if (site_run.pending_opened_inventory_storage_full_projection_update)
    {
        for (std::uint32_t slot_index = 0U; slot_index < opened_storage->slot_item_instance_ids.size(); ++slot_index)
        {
            queue_site_inventory_slot_upsert_message(
                GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
                slot_index,
                opened_storage->storage_id,
                opened_storage->owner_device_entity_id,
                opened_storage->tile_coord);
        }
        return;
    }

    std::sort(
        site_run.pending_opened_inventory_storage_projection_updates.begin(),
        site_run.pending_opened_inventory_storage_projection_updates.end());
    for (const auto slot_index : site_run.pending_opened_inventory_storage_projection_updates)
    {
        queue_site_inventory_slot_upsert_message(
            GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
            slot_index,
            opened_storage->storage_id,
            opened_storage->owner_device_entity_id,
            opened_storage->tile_coord);
    }
}

void GameRuntime::queue_site_craft_context_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& presentation = active_site_run_->craft.context_presentation;
    auto begin = make_engine_message(GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN);
    auto& begin_payload = begin.emplace_payload<Gs1EngineMessageCraftContextData>();
    begin_payload.tile_x = presentation.tile_coord.x;
    begin_payload.tile_y = presentation.tile_coord.y;
    begin_payload.option_count = static_cast<std::uint16_t>(std::min<std::size_t>(
        presentation.options.size(),
        65535U));
    begin_payload.flags = presentation.occupied ? 1U : 0U;
    engine_messages_.push_back(begin);

    for (const auto& option : presentation.options)
    {
        auto option_message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT);
        auto& option_payload =
            option_message.emplace_payload<Gs1EngineMessageCraftContextOptionData>();
        option_payload.recipe_id = option.recipe_id;
        option_payload.output_item_id = option.output_item_id;
        option_payload.flags = 1U;
        option_payload.reserved0 = 0U;
        engine_messages_.push_back(option_message);
    }

    engine_messages_.push_back(make_engine_message(GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END));
}

void GameRuntime::queue_site_placement_preview_message()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& placement_mode = active_site_run_->site_action.placement_mode;
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW);
    auto& payload = message.emplace_payload<Gs1EngineMessagePlacementPreviewData>();
    if (placement_mode.active && placement_mode.target_tile.has_value())
    {
        payload.tile_x = placement_mode.target_tile->x;
        payload.tile_y = placement_mode.target_tile->y;
        payload.blocked_mask = placement_mode.blocked_mask;
        payload.item_id = placement_mode.item_id;
        payload.footprint_width = placement_mode.footprint_width;
        payload.footprint_height = placement_mode.footprint_height;
        payload.action_kind = static_cast<Gs1SiteActionKind>(placement_mode.action_kind);
        payload.flags = 1U;
        if (placement_mode.blocked_mask == 0ULL)
        {
            payload.flags |= 2U;
        }
    }
    else
    {
        payload.tile_x = 0;
        payload.tile_y = 0;
        payload.blocked_mask = 0ULL;
        payload.item_id = 0U;
        payload.footprint_width = 1U;
        payload.footprint_height = 1U;
        payload.action_kind = GS1_SITE_ACTION_NONE;
        payload.flags = 4U;
    }

    engine_messages_.push_back(message);
}

void GameRuntime::queue_site_placement_failure_message(const PlacementModeCommitRejectedMessage& payload)
{
    static std::uint32_t next_sequence_id = 1U;

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE);
    auto& engine_payload = message.emplace_payload<Gs1EngineMessagePlacementFailureData>();
    engine_payload.tile_x = payload.tile_x;
    engine_payload.tile_y = payload.tile_y;
    engine_payload.blocked_mask = payload.blocked_mask;
    engine_payload.sequence_id = next_sequence_id++;
    engine_payload.action_kind = payload.action_kind;
    engine_payload.flags = 1U;
    engine_payload.reserved0 = 0U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_site_task_upsert_message(std::size_t task_index)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& tasks = active_site_run_->task_board.visible_tasks;
    if (task_index >= tasks.size())
    {
        return;
    }

    const auto& task = tasks[task_index];
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageTaskData>();
    payload.task_instance_id = task.task_instance_id.value;
    payload.task_template_id = task.task_template_id.value;
    payload.publisher_faction_id = task.publisher_faction_id.value;
    payload.current_progress = static_cast<std::uint16_t>(std::min<std::uint32_t>(task.current_progress_amount, 65535U));
    payload.target_progress = static_cast<std::uint16_t>(std::min<std::uint32_t>(task.target_amount, 65535U));
    payload.list_kind = to_task_presentation_list_kind(task.runtime_list_kind);
    payload.flags = 1U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_all_site_task_upsert_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    for (std::size_t index = 0; index < active_site_run_->task_board.visible_tasks.size(); ++index)
    {
        queue_site_task_upsert_message(index);
    }
}

void GameRuntime::queue_site_modifier_list_begin_message(Gs1ProjectionMode mode)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteModifierListData>();
    payload.mode = mode;
    payload.reserved0 = 0U;
    payload.modifier_count = static_cast<std::uint16_t>(std::min<std::size_t>(
        active_site_run_->modifier.active_site_modifiers.size(),
        std::numeric_limits<std::uint16_t>::max()));
    engine_messages_.push_back(message);
}

void GameRuntime::queue_site_modifier_upsert_message(std::size_t modifier_index)
{
    if (!active_site_run_.has_value() ||
        modifier_index >= active_site_run_->modifier.active_site_modifiers.size())
    {
        return;
    }

    const auto& active_modifier =
        active_site_run_->modifier.active_site_modifiers[modifier_index];
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteModifierData>();
    payload.modifier_id = active_modifier.modifier_id.value;
    payload.remaining_game_hours =
        projected_remaining_game_hours(active_modifier.remaining_world_minutes);
    payload.flags =
        active_modifier.duration_world_minutes > 0.0 ? GS1_SITE_MODIFIER_FLAG_TIMED : 0U;
    payload.reserved0 = 0U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_all_site_modifier_upsert_messages(Gs1ProjectionMode mode)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    queue_site_modifier_list_begin_message(mode);
    for (std::size_t index = 0U; index < active_site_run_->modifier.active_site_modifiers.size(); ++index)
    {
        queue_site_modifier_upsert_message(index);
    }
}

void GameRuntime::queue_site_phone_listing_upsert_message(std::size_t listing_index)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& listings = active_site_run_->phone_panel.listings;
    if (listing_index >= listings.size())
    {
        return;
    }

    const auto& listing = listings[listing_index];
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessagePhoneListingData>();
    payload.listing_id = listing.listing_id;
    payload.item_or_unlockable_id = listing.item_id.value;
    payload.price = cash_value_from_cash_points(listing.price);
    payload.related_site_id = active_site_run_->site_id.value;
    payload.quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(listing.quantity, 65535U));
    payload.cart_quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(listing.cart_quantity, 65535U));
    payload.listing_kind = to_phone_listing_presentation_kind(listing.kind);
    payload.flags = listing.occupied ? 1U : 0U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_site_phone_panel_state_message()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& phone_panel = active_site_run_->phone_panel;
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    auto& payload = message.emplace_payload<Gs1EngineMessagePhonePanelData>();
    payload.active_section = to_phone_panel_section(phone_panel.active_section);
    payload.reserved0[0] = 0U;
    payload.reserved0[1] = 0U;
    payload.reserved0[2] = 0U;
    payload.visible_task_count = phone_panel.visible_task_count;
    payload.accepted_task_count = phone_panel.accepted_task_count;
    payload.completed_task_count = phone_panel.completed_task_count;
    payload.claimed_task_count = phone_panel.claimed_task_count;
    payload.buy_listing_count = phone_panel.buy_listing_count;
    payload.sell_listing_count = phone_panel.sell_listing_count;
    payload.service_listing_count = phone_panel.service_listing_count;
    payload.cart_item_count = phone_panel.cart_item_count;
    payload.flags = phone_panel.open ? GS1_PHONE_PANEL_FLAG_OPEN : 0U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_site_protection_overlay_state_message()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteProtectionOverlayData>();
    payload.mode = site_protection_overlay_mode_;
    payload.reserved0[0] = 0U;
    payload.reserved0[1] = 0U;
    payload.reserved0[2] = 0U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_site_phone_listing_remove_message(std::uint32_t listing_id)
{
    if (!active_site_run_.has_value() || listing_id == 0U)
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE);
    auto& payload = message.emplace_payload<Gs1EngineMessagePhoneListingData>();
    payload.listing_id = listing_id;
    payload.item_or_unlockable_id = 0U;
    payload.price = 0;
    payload.related_site_id = active_site_run_->site_id.value;
    payload.quantity = 0U;
    payload.cart_quantity = 0U;
    payload.listing_kind = GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM;
    payload.flags = 0U;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_all_site_phone_listing_upsert_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    std::vector<std::uint32_t> current_listing_ids {};
    current_listing_ids.reserve(active_site_run_->phone_panel.listings.size());
    for (const auto& listing : active_site_run_->phone_panel.listings)
    {
        current_listing_ids.push_back(listing.listing_id);
    }

    for (const auto previous_listing_id : last_emitted_phone_listing_ids_)
    {
        if (std::find(current_listing_ids.begin(), current_listing_ids.end(), previous_listing_id) ==
            current_listing_ids.end())
        {
            queue_site_phone_listing_remove_message(previous_listing_id);
        }
    }

    for (std::size_t index = 0; index < active_site_run_->phone_panel.listings.size(); ++index)
    {
        queue_site_phone_listing_upsert_message(index);
    }

    last_emitted_phone_listing_ids_ = std::move(current_listing_ids);
}

void GameRuntime::queue_site_bootstrap_messages()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    queue_site_snapshot_begin_message(GS1_PROJECTION_MODE_SNAPSHOT);
    queue_all_site_tile_upsert_messages();
    queue_site_worker_update_message();
    queue_site_camp_update_message();
    queue_site_weather_update_message();
    queue_all_site_inventory_storage_upsert_messages();
    queue_all_site_inventory_slot_upsert_messages();
    queue_all_site_task_upsert_messages();
    queue_all_site_modifier_upsert_messages(GS1_PROJECTION_MODE_SNAPSHOT);
    queue_site_phone_panel_state_message();
    queue_all_site_phone_listing_upsert_messages();
    queue_site_protection_overlay_state_message();
    queue_site_snapshot_end_message();

    active_site_run_->pending_projection_update_flags = 0U;
    clear_pending_site_tile_projection_updates();
    clear_pending_site_inventory_projection_updates();
}

void GameRuntime::queue_site_delta_messages(std::uint64_t dirty_flags)
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto site_dirty_flags =
        dirty_flags & (SITE_PROJECTION_UPDATE_TILES |
            SITE_PROJECTION_UPDATE_WORKER |
            SITE_PROJECTION_UPDATE_CAMP |
            SITE_PROJECTION_UPDATE_WEATHER |
            SITE_PROJECTION_UPDATE_INVENTORY |
            SITE_PROJECTION_UPDATE_CRAFT_CONTEXT |
            SITE_PROJECTION_UPDATE_PLACEMENT_PREVIEW |
            SITE_PROJECTION_UPDATE_TASKS |
            SITE_PROJECTION_UPDATE_MODIFIERS |
            SITE_PROJECTION_UPDATE_PHONE);
    if (site_dirty_flags == 0U)
    {
        return;
    }

    queue_site_snapshot_begin_message(GS1_PROJECTION_MODE_DELTA);

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
    {
        queue_pending_site_tile_upsert_messages();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_WORKER) != 0U)
    {
        queue_site_worker_update_message();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_CAMP) != 0U)
    {
        queue_site_camp_update_message();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_WEATHER) != 0U)
    {
        queue_site_weather_update_message();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
    {
        queue_pending_site_inventory_slot_upsert_messages();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_CRAFT_CONTEXT) != 0U)
    {
        queue_site_craft_context_messages();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_PLACEMENT_PREVIEW) != 0U)
    {
        queue_site_placement_preview_message();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_TASKS) != 0U)
    {
        queue_all_site_task_upsert_messages();
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_MODIFIERS) != 0U)
    {
        queue_all_site_modifier_upsert_messages(GS1_PROJECTION_MODE_DELTA);
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_PHONE) != 0U)
    {
        queue_site_phone_panel_state_message();
        queue_all_site_phone_listing_upsert_messages();
    }

    queue_site_snapshot_end_message();
}

void GameRuntime::queue_site_action_update_message()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto& action_state = active_site_run_->site_action;
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteActionData>();

    if (action_has_started(action_state))
    {
        const TileCoord target_tile = action_state.target_tile.value_or(TileCoord {});
        payload.action_id = action_state.current_action_id->value;
        payload.target_tile_x = target_tile.x;
        payload.target_tile_y = target_tile.y;
        payload.action_kind = static_cast<Gs1SiteActionKind>(action_state.action_kind);
        payload.flags = GS1_SITE_ACTION_PRESENTATION_FLAG_ACTIVE;
        payload.progress_normalized = action_progress_normalized(action_state);
        payload.duration_minutes = static_cast<float>(action_state.total_action_minutes);
    }
    else
    {
        payload.action_id = 0U;
        payload.target_tile_x = 0;
        payload.target_tile_y = 0;
        payload.action_kind = GS1_SITE_ACTION_NONE;
        payload.flags = GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR;
        payload.progress_normalized = 0.0f;
        payload.duration_minutes = 0.0f;
    }

    engine_messages_.push_back(message);
}

void GameRuntime::queue_hud_state_message()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto worker_conditions = site_world_access::worker_conditions(active_site_run_.value());

    auto hud_message = make_engine_message(GS1_ENGINE_MESSAGE_HUD_STATE);
    auto& payload = hud_message.emplace_payload<Gs1EngineMessageHudStateData>();
    payload.player_health = worker_conditions.health;
    payload.player_hydration = worker_conditions.hydration;
    payload.player_nourishment = worker_conditions.nourishment;
    payload.player_energy = worker_conditions.energy;
    payload.player_morale = worker_conditions.morale;
    payload.current_money = cash_value_from_cash_points(active_site_run_->economy.current_cash);
    payload.active_task_count =
        static_cast<std::uint16_t>(active_site_run_->task_board.accepted_task_ids.size());
    payload.current_action_kind =
        static_cast<Gs1SiteActionKind>(active_site_run_->site_action.action_kind);
    payload.site_completion_normalized = active_site_run_->counters.objective_progress_normalized;
    payload.warning_code = resolve_hud_warning_code(active_site_run_.value());
    engine_messages_.push_back(hud_message);
}

void GameRuntime::queue_campaign_resources_message()
{
    if (!campaign_.has_value())
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    auto& payload = message.emplace_payload<Gs1EngineMessageCampaignResourcesData>();
    payload.current_money = active_site_run_.has_value()
        ? cash_value_from_cash_points(active_site_run_->economy.current_cash)
        : 0.0f;
    payload.total_reputation = campaign_->technology_state.total_reputation;
    payload.village_reputation = TechnologySystem::faction_reputation(
        *campaign_,
        FactionId {k_faction_village_committee});
    payload.forestry_reputation = TechnologySystem::faction_reputation(
        *campaign_,
        FactionId {k_faction_forestry_bureau});
    payload.university_reputation = TechnologySystem::faction_reputation(
        *campaign_,
        FactionId {k_faction_agricultural_university});
    engine_messages_.push_back(message);
}

void GameRuntime::queue_site_result_ready_message(
    std::uint32_t site_id,
    Gs1SiteAttemptResult result,
    std::uint32_t newly_revealed_site_count)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_RESULT_READY);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteResultData>();
    payload.site_id = site_id;
    payload.result = result;
    payload.newly_revealed_site_count = static_cast<std::uint16_t>(newly_revealed_site_count);
    engine_messages_.push_back(message);
}

void GameRuntime::queue_task_reward_claimed_cue_message(
    std::uint32_t task_instance_id,
    std::uint32_t task_template_id,
    std::uint32_t reward_candidate_count)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE);
    auto& payload = message.emplace_payload<Gs1EngineMessageOneShotCueData>();
    payload.subject_id = task_instance_id;
    payload.world_x = 0.0f;
    payload.world_y = 0.0f;
    payload.arg0 = task_template_id;
    payload.arg1 = reward_candidate_count;
    payload.cue_kind = GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED;
    engine_messages_.push_back(message);
}

void GameRuntime::queue_campaign_unlock_cue_message(
    std::uint32_t subject_id,
    std::uint32_t detail_id,
    std::uint32_t detail_kind)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE);
    auto& payload = message.emplace_payload<Gs1EngineMessageOneShotCueData>();
    payload.subject_id = subject_id;
    payload.world_x = 0.0f;
    payload.world_y = 0.0f;
    payload.arg0 = detail_id;
    payload.arg1 = detail_kind;
    payload.cue_kind = GS1_ONE_SHOT_CUE_CAMPAIGN_UNLOCKED;
    engine_messages_.push_back(message);
}

void GameRuntime::sync_campaign_unlock_presentations()
{
    if (!campaign_.has_value())
    {
        last_campaign_unlock_snapshot_ = {};
        return;
    }

    auto unlocked_reputation_unlock_ids = capture_unlocked_reputation_unlock_ids(*campaign_);
    auto unlocked_technology_node_ids = capture_unlocked_technology_node_ids(*campaign_);
    std::sort(unlocked_reputation_unlock_ids.begin(), unlocked_reputation_unlock_ids.end());
    std::sort(unlocked_technology_node_ids.begin(), unlocked_technology_node_ids.end());

    for (const auto unlock_id : unlocked_reputation_unlock_ids)
    {
        if (!sorted_contains(last_campaign_unlock_snapshot_.unlocked_reputation_unlock_ids, unlock_id))
        {
            queue_campaign_unlock_cue_message(
                unlock_id,
                unlock_id,
                CAMPAIGN_UNLOCK_CUE_DETAIL_REPUTATION_UNLOCK);
        }
    }

    for (const auto tech_node_id : unlocked_technology_node_ids)
    {
        if (!sorted_contains(last_campaign_unlock_snapshot_.unlocked_technology_node_ids, tech_node_id))
        {
            queue_campaign_unlock_cue_message(
                tech_node_id,
                tech_node_id,
                CAMPAIGN_UNLOCK_CUE_DETAIL_TECHNOLOGY_NODE);
        }
    }

    last_campaign_unlock_snapshot_.unlocked_reputation_unlock_ids =
        std::move(unlocked_reputation_unlock_ids);
    last_campaign_unlock_snapshot_.unlocked_technology_node_ids =
        std::move(unlocked_technology_node_ids);
}

void GameRuntime::close_site_protection_ui() noexcept
{
    site_protection_selector_open_ = false;
    site_protection_overlay_mode_ = GS1_SITE_PROTECTION_OVERLAY_NONE;
}

void GameRuntime::mark_site_projection_update_dirty(std::uint64_t dirty_flags) noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    if ((dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
    {
        active_site_run_->pending_full_tile_projection_update = true;
    }
    if ((dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
    {
        active_site_run_->pending_full_inventory_projection_update = true;
        active_site_run_->pending_inventory_storage_descriptor_projection_update = true;
        if (active_site_run_->inventory.opened_device_storage_id != 0U)
        {
            active_site_run_->pending_opened_inventory_storage_full_projection_update = true;
        }
    }

    active_site_run_->pending_projection_update_flags |= dirty_flags;
}

void GameRuntime::mark_site_tile_projection_dirty(TileCoord coord) noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return;
    }

    const auto tile_count = site_world_access::tile_count(site_run);
    if (site_run.pending_tile_projection_update_mask.size() != tile_count)
    {
        site_run.pending_tile_projection_update_mask.assign(tile_count, 0U);
        site_run.pending_tile_projection_updates.clear();
    }

    const auto index = site_world_access::tile_index(site_run, coord);
    if (site_run.pending_tile_projection_update_mask[index] == 0U)
    {
        site_run.pending_tile_projection_update_mask[index] = 1U;
        site_run.pending_tile_projection_updates.push_back(coord);
    }

    site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_TILES;
}

void GameRuntime::clear_pending_site_tile_projection_updates() noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();
    for (const auto& coord : site_run.pending_tile_projection_updates)
    {
        if (!site_world_access::tile_coord_in_bounds(site_run, coord))
        {
            continue;
        }

        const auto index = site_world_access::tile_index(site_run, coord);
        if (index < site_run.pending_tile_projection_update_mask.size())
        {
            site_run.pending_tile_projection_update_mask[index] = 0U;
        }
    }

    site_run.pending_tile_projection_updates.clear();
    site_run.pending_full_tile_projection_update = false;
}

void GameRuntime::clear_pending_site_inventory_projection_updates() noexcept
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    auto& site_run = active_site_run_.value();

    for (const auto slot_index : site_run.pending_worker_pack_inventory_projection_updates)
    {
        if (slot_index < site_run.pending_worker_pack_inventory_projection_update_mask.size())
        {
            site_run.pending_worker_pack_inventory_projection_update_mask[slot_index] = 0U;
        }
    }
    site_run.pending_worker_pack_inventory_projection_updates.clear();

    for (const auto slot_index : site_run.pending_opened_inventory_storage_projection_updates)
    {
        if (slot_index < site_run.pending_opened_inventory_storage_projection_update_mask.size())
        {
            site_run.pending_opened_inventory_storage_projection_update_mask[slot_index] = 0U;
        }
    }
    site_run.pending_opened_inventory_storage_projection_updates.clear();

    site_run.pending_full_inventory_projection_update = false;
    site_run.pending_inventory_storage_descriptor_projection_update = false;
    site_run.pending_inventory_view_state_projection_update = false;
    site_run.pending_opened_inventory_storage_full_projection_update = false;
}

void GameRuntime::flush_site_presentation_if_dirty()
{
    if (!active_site_run_.has_value())
    {
        return;
    }

    const auto dirty_flags = active_site_run_->pending_projection_update_flags;
    if (dirty_flags == 0U)
    {
        return;
    }

    if ((dirty_flags & (SITE_PROJECTION_UPDATE_PHONE |
            SITE_PROJECTION_UPDATE_TASKS |
            SITE_PROJECTION_UPDATE_INVENTORY)) != 0U)
    {
        auto phone_panel_context = make_site_system_context<PhonePanelSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            SiteMoveDirectionInput {});
        PhonePanelSystem::run(phone_panel_context);
    }

    const auto projected_dirty_flags = active_site_run_->pending_projection_update_flags;
    if (projected_dirty_flags == 0U)
    {
        return;
    }

    queue_site_delta_messages(projected_dirty_flags);

    if ((projected_dirty_flags & (SITE_PROJECTION_UPDATE_HUD |
            SITE_PROJECTION_UPDATE_WORKER |
            SITE_PROJECTION_UPDATE_WEATHER)) != 0U)
    {
        queue_hud_state_message();
    }

    active_site_run_->pending_projection_update_flags = 0U;
    clear_pending_site_tile_projection_updates();
    clear_pending_site_inventory_projection_updates();
}

Gs1Status GameRuntime::translate_ui_action_to_message(const Gs1UiAction& action, GameMessage& out_message) const
{
    switch (action.type)
    {
    case GS1_UI_ACTION_START_NEW_CAMPAIGN:
        if (action.arg1 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::StartNewCampaign;
        out_message.set_payload(StartNewCampaignMessage {
            action.arg0,
            static_cast<std::uint32_t>(action.arg1)});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::SelectDeploymentSite;
        out_message.set_payload(SelectDeploymentSiteMessage {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
        out_message.type = GameMessageType::ClearDeploymentSiteSelection;
        out_message.set_payload(ClearDeploymentSiteSelectionMessage {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE:
        out_message.type = GameMessageType::OpenRegionalMapTechTree;
        out_message.set_payload(OpenRegionalMapTechTreeMessage {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE:
        out_message.type = GameMessageType::CloseRegionalMapTechTree;
        out_message.set_payload(CloseRegionalMapTechTreeMessage {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::SelectRegionalMapTechTreeFaction;
        out_message.set_payload(SelectRegionalMapTechTreeFactionMessage {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_START_SITE_ATTEMPT:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::StartSiteAttempt;
        out_message.set_payload(StartSiteAttemptMessage {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP:
        out_message.type = GameMessageType::ReturnToRegionalMap;
        out_message.set_payload(ReturnToRegionalMapMessage {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_ACCEPT_TASK:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::TaskAcceptRequested;
        out_message.set_payload(TaskAcceptRequestedMessage {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLAIM_TASK_REWARD:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::TaskRewardClaimRequested;
        out_message.set_payload(TaskRewardClaimRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLAIM_TECHNOLOGY_NODE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::TechnologyNodeClaimRequested;
        out_message.set_payload(TechnologyNodeClaimRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_REFUND_TECHNOLOGY_NODE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::TechnologyNodeRefundRequested;
        out_message.set_payload(TechnologyNodeRefundRequestedMessage {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_BUY_PHONE_LISTING:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::PhoneListingPurchaseRequested;
        out_message.set_payload(PhoneListingPurchaseRequestedMessage {
            action.target_id,
            static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0),
            0U});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_ADD_PHONE_LISTING_TO_CART:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::PhoneListingCartAddRequested;
        out_message.set_payload(PhoneListingCartAddRequestedMessage {
            action.target_id,
            static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0),
            0U});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_REMOVE_PHONE_LISTING_FROM_CART:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::PhoneListingCartRemoveRequested;
        out_message.set_payload(PhoneListingCartRemoveRequestedMessage {
            action.target_id,
            static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0),
            0U});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CHECKOUT_PHONE_CART:
        out_message.type = GameMessageType::PhoneCartCheckoutRequested;
        out_message.set_payload(PhoneCartCheckoutRequestedMessage {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SET_PHONE_PANEL_SECTION:
        if (action.arg0 > static_cast<std::uint64_t>(GS1_PHONE_PANEL_SECTION_CART))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::PhonePanelSectionRequested;
        out_message.set_payload(PhonePanelSectionRequestedMessage {
            static_cast<Gs1PhonePanelSection>(action.arg0),
            {0U, 0U, 0U}});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLOSE_PHONE_PANEL:
        out_message.type = GameMessageType::ClosePhonePanel;
        out_message.set_payload(ClosePhonePanelMessage {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR:
        out_message.type = GameMessageType::OpenSiteProtectionSelector;
        out_message.set_payload(OpenSiteProtectionSelectorMessage {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI:
        out_message.type = GameMessageType::CloseSiteProtectionUi;
        out_message.set_payload(CloseSiteProtectionUiMessage {});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE:
        if (action.arg0 > static_cast<std::uint64_t>(GS1_SITE_PROTECTION_OVERLAY_DUST))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::SetSiteProtectionOverlayMode;
        out_message.set_payload(SetSiteProtectionOverlayModeMessage {
            static_cast<Gs1SiteProtectionOverlayMode>(action.arg0),
            {0U, 0U, 0U}});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_END_SITE_MODIFIER:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::SiteModifierEndRequested;
        out_message.set_payload(SiteModifierEndRequestedMessage {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_SELL_PHONE_LISTING:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::PhoneListingSaleRequested;
        out_message.set_payload(PhoneListingSaleRequestedMessage {
            action.target_id,
            static_cast<std::uint16_t>(action.arg0 == 0ULL ? 1ULL : action.arg0),
            0U});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_HIRE_CONTRACTOR:
        if (action.target_id == 0U ||
            action.arg0 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::ContractorHireRequested;
        out_message.set_payload(ContractorHireRequestedMessage {
            action.target_id,
            static_cast<std::uint32_t>(action.arg0)});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::SiteUnlockablePurchaseRequested;
        out_message.set_payload(SiteUnlockablePurchaseRequestedMessage {action.target_id});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_USE_INVENTORY_ITEM:
        if (action.target_id == 0U)
        {
            return GS1_STATUS_INVALID_ARGUMENT;
        }
        out_message.type = GameMessageType::InventoryItemUseRequested;
        out_message.set_payload(InventoryItemUseRequestedMessage {
            action.target_id,
            unpack_inventory_storage_id(action.arg1),
            static_cast<std::uint16_t>(unpack_inventory_quantity(action.arg0) == 0U
                    ? 1U
                    : unpack_inventory_quantity(action.arg0)),
            static_cast<std::uint16_t>(unpack_inventory_slot_index(action.arg0))});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM:
        out_message.type = GameMessageType::InventoryTransferRequested;
        out_message.set_payload(InventoryTransferRequestedMessage {
            unpack_transfer_source_owner(action.arg1),
            unpack_transfer_destination_owner(action.arg1),
            static_cast<std::uint16_t>(unpack_transfer_source_slot(action.arg0)),
            0U,
            static_cast<std::uint16_t>(unpack_transfer_quantity(action.arg0)),
            k_inventory_transfer_flag_resolve_destination_in_dll,
            0U});
        return GS1_STATUS_OK;

    case GS1_UI_ACTION_NONE:
    default:
        return GS1_STATUS_INVALID_ARGUMENT;
    }
}

Gs1Status GameRuntime::translate_site_action_request_to_message(
    const Gs1HostEventSiteActionRequestData& action,
    GameMessage& out_message) const
{
    if (action.action_kind == GS1_SITE_ACTION_NONE)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_message.type = GameMessageType::StartSiteAction;
    out_message.set_payload(StartSiteActionMessage {
        action.action_kind,
        action.flags,
        action.quantity == 0U ? 1U : action.quantity,
        action.target_tile_x,
        action.target_tile_y,
        action.primary_subject_id,
        action.secondary_subject_id,
        action.item_id});
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::translate_site_action_cancel_to_message(
    const Gs1HostEventSiteActionCancelData& action,
    GameMessage& out_message) const
{
    if (action.action_id == 0U &&
        (action.flags & (GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION |
            GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE)) == 0U)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_message.type = GameMessageType::CancelSiteAction;
    out_message.set_payload(CancelSiteActionMessage {
        action.action_id,
        action.flags});
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::translate_site_storage_view_to_message(
    const Gs1HostEventSiteStorageViewData& request,
    GameMessage& out_message) const
{
    if (request.event_kind != GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT &&
        request.event_kind != GS1_INVENTORY_VIEW_EVENT_CLOSE)
    {
        return GS1_STATUS_INVALID_ARGUMENT;
    }

    out_message.type = GameMessageType::InventoryStorageViewRequest;
    out_message.set_payload(InventoryStorageViewRequestMessage {
        request.storage_id,
        request.event_kind,
        {0U, 0U, 0U}});
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::translate_site_context_request_to_message(
    const Gs1HostEventSiteContextRequestData& request,
    GameMessage& out_message) const
{
    const bool placement_mode_active =
        active_site_run_.has_value() &&
        active_site_run_->site_action.placement_mode.active;
    if (placement_mode_active)
    {
        out_message.type = GameMessageType::PlacementModeCursorMoved;
        out_message.set_payload(PlacementModeCursorMovedMessage {
            request.tile_x,
            request.tile_y,
            request.flags});
    }
    else
    {
        out_message.type = GameMessageType::InventoryCraftContextRequested;
        out_message.set_payload(CraftContextRequestedMessage {
            request.tile_x,
            request.tile_y,
            request.flags});
    }
    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::cancel_pending_device_storage_open_request()
{
    if (!active_site_run_.has_value() ||
        !active_site_run_->inventory.pending_device_storage_open.active ||
        active_site_run_->inventory.pending_device_storage_open.storage_id == 0U)
    {
        return GS1_STATUS_OK;
    }

    GameMessage message {};
    message.type = GameMessageType::InventoryStorageViewRequest;
    message.set_payload(InventoryStorageViewRequestMessage {
        active_site_run_->inventory.pending_device_storage_open.storage_id,
        GS1_INVENTORY_VIEW_EVENT_CLOSE,
        {0U, 0U, 0U}});
    message_queue_.push_back(message);
    return dispatch_queued_messages();
}

Gs1Status GameRuntime::dispatch_host_events(
    HostEventDispatchStage stage,
    std::uint32_t& out_processed_count)
{
    out_processed_count = 0U;

    while (!host_events_.empty())
    {
        const auto event = host_events_.front();
        host_events_.pop_front();
        out_processed_count += 1U;

        switch (event.type)
        {
        case GS1_HOST_EVENT_UI_ACTION:
        {
            GameMessage message {};
            const auto status = translate_ui_action_to_message(event.payload.ui_action.action, message);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }

            message_queue_.push_back(message);
            const auto dispatch_status = dispatch_queued_messages();
            if (dispatch_status != GS1_STATUS_OK)
            {
                return dispatch_status;
            }
            break;
        }

        case GS1_HOST_EVENT_SITE_MOVE_DIRECTION:
        {
            assert(stage == HostEventDispatchStage::Phase1);
            if (stage != HostEventDispatchStage::Phase1)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            assert(!phase1_site_move_direction_.present);
            if (phase1_site_move_direction_.present)
            {
                return GS1_STATUS_INVALID_STATE;
            }

            const auto& payload = event.payload.site_move_direction;
            const float move_length_squared =
                payload.world_move_x * payload.world_move_x +
                payload.world_move_y * payload.world_move_y +
                payload.world_move_z * payload.world_move_z;
            if (move_length_squared > 0.0001f)
            {
                const auto cancel_status = cancel_pending_device_storage_open_request();
                if (cancel_status != GS1_STATUS_OK)
                {
                    return cancel_status;
                }
            }
            if (!active_site_run_.has_value() || app_state_ != GS1_APP_STATE_SITE_ACTIVE)
            {
                break;
            }
            phase1_site_move_direction_.world_move_x = payload.world_move_x;
            phase1_site_move_direction_.world_move_y = payload.world_move_y;
            phase1_site_move_direction_.world_move_z = payload.world_move_z;
            phase1_site_move_direction_.present = true;
            break;
        }

        case GS1_HOST_EVENT_SITE_ACTION_REQUEST:
        {
            GameMessage message {};
            const auto status =
                translate_site_action_request_to_message(event.payload.site_action_request, message);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }

            message_queue_.push_back(message);
            const auto dispatch_status = dispatch_queued_messages();
            if (dispatch_status != GS1_STATUS_OK)
            {
                return dispatch_status;
            }
            break;
        }

        case GS1_HOST_EVENT_SITE_ACTION_CANCEL:
        {
            GameMessage message {};
            const auto status =
                translate_site_action_cancel_to_message(event.payload.site_action_cancel, message);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }

            message_queue_.push_back(message);
            const auto dispatch_status = dispatch_queued_messages();
            if (dispatch_status != GS1_STATUS_OK)
            {
                return dispatch_status;
            }
            break;
        }

        case GS1_HOST_EVENT_SITE_STORAGE_VIEW:
        {
            GameMessage message {};
            const auto status =
                translate_site_storage_view_to_message(event.payload.site_storage_view, message);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }

            message_queue_.push_back(message);
            const auto dispatch_status = dispatch_queued_messages();
            if (dispatch_status != GS1_STATUS_OK)
            {
                return dispatch_status;
            }
            break;
        }

        case GS1_HOST_EVENT_SITE_CONTEXT_REQUEST:
        {
            GameMessage message {};
            const auto status =
                translate_site_context_request_to_message(event.payload.site_context_request, message);
            if (status != GS1_STATUS_OK)
            {
                return status;
            }

            message_queue_.push_back(message);
            const auto dispatch_status = dispatch_queued_messages();
            if (dispatch_status != GS1_STATUS_OK)
            {
                return dispatch_status;
            }
            break;
        }

        default:
            return GS1_STATUS_INVALID_ARGUMENT;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_feedback_events(std::uint32_t& out_processed_count)
{
    out_processed_count = 0U;

    while (!feedback_events_.empty())
    {
        const auto event = feedback_events_.front();
        feedback_events_.pop_front();
        out_processed_count += 1U;

        const auto status = dispatch_subscribed_feedback_event(event);
        if (status != GS1_STATUS_OK)
        {
            return status;
        }
    }

    return GS1_STATUS_OK;
}

Gs1Status GameRuntime::dispatch_queued_messages()
{
    return MessageDispatcher::dispatch_all(*this);
}

void GameRuntime::run_fixed_step()
{
    if (!campaign_.has_value() || !active_site_run_.has_value())
    {
        return;
    }

    const auto fixed_step_started_at = std::chrono::steady_clock::now();
    const auto run_profiled_system = [this](Gs1RuntimeProfileSystemId system_id, auto&& run_fn)
    {
        if (!profiled_system_enabled(system_id))
        {
            return;
        }

        const auto started_at = std::chrono::steady_clock::now();
        run_fn();
        record_timing_sample(
            profiled_systems_[static_cast<std::size_t>(system_id)].run_timing,
            elapsed_milliseconds_since(started_at));
    };

    const SiteMoveDirectionInput move_direction {
        phase1_site_move_direction_.world_move_x,
        phase1_site_move_direction_.world_move_y,
        phase1_site_move_direction_.world_move_z,
        phase1_site_move_direction_.present};

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_TIME, [&]()
    {
        CampaignFixedStepContext campaign_context {*campaign_, fixed_step_seconds_};
        CampaignTimeSystem::run(campaign_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME, [&]()
    {
        auto site_time_context = make_site_system_context<SiteTimeSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        SiteTimeSystem::run(site_time_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW, [&]()
    {
        auto site_flow_context = make_site_system_context<SiteFlowSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        SiteFlowSystem::run(site_flow_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER, [&]()
    {
        auto modifier_context = make_site_system_context<ModifierSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        ModifierSystem::run(modifier_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT, [&]()
    {
        auto weather_event_context = make_site_system_context<WeatherEventSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        WeatherEventSystem::run(weather_event_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION, [&]()
    {
        auto action_execution_context = make_site_system_context<ActionExecutionSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        ActionExecutionSystem::run(action_execution_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE, [&]()
    {
        auto plant_weather_contribution_context =
            make_site_system_context<PlantWeatherContributionSystem>(
                *campaign_,
                *active_site_run_,
                message_queue_,
                fixed_step_seconds_,
                move_direction);
        PlantWeatherContributionSystem::run(plant_weather_contribution_context);

        auto device_weather_contribution_context =
            make_site_system_context<DeviceWeatherContributionSystem>(
                *campaign_,
                *active_site_run_,
                message_queue_,
                fixed_step_seconds_,
                move_direction);
        DeviceWeatherContributionSystem::run(device_weather_contribution_context);

        auto local_weather_context = make_site_system_context<LocalWeatherResolveSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        LocalWeatherResolveSystem::run(local_weather_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION, [&]()
    {
        auto worker_condition_context = make_site_system_context<WorkerConditionSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        WorkerConditionSystem::run(worker_condition_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY, [&]()
    {
        auto camp_durability_context = make_site_system_context<CampDurabilitySystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        CampDurabilitySystem::run(camp_durability_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE, [&]()
    {
        auto device_maintenance_context = make_site_system_context<DeviceMaintenanceSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        DeviceMaintenanceSystem::run(device_maintenance_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT, [&]()
    {
        auto device_support_context = make_site_system_context<DeviceSupportSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        DeviceSupportSystem::run(device_support_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY, [&]()
    {
        auto ecology_context = make_site_system_context<EcologySystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        EcologySystem::run(ecology_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY, [&]()
    {
        auto inventory_context = make_site_system_context<InventorySystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        InventorySystem::run(inventory_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_CRAFT, [&]()
    {
        auto craft_context = make_site_system_context<CraftSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        CraftSystem::run(craft_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD, [&]()
    {
        auto task_board_context = make_site_system_context<TaskBoardSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        TaskBoardSystem::run(task_board_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE, [&]()
    {
        auto economy_context = make_site_system_context<EconomyPhoneSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        EconomyPhoneSystem::run(economy_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_PHONE_PANEL, [&]()
    {
        auto phone_panel_context = make_site_system_context<PhonePanelSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        PhonePanelSystem::run(phone_panel_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_FAILURE_RECOVERY, [&]()
    {
        auto failure_context = make_site_system_context<FailureRecoverySystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        FailureRecoverySystem::run(failure_context);
    });

    run_profiled_system(GS1_RUNTIME_PROFILE_SYSTEM_SITE_COMPLETION, [&]()
    {
        auto completion_context = make_site_system_context<SiteCompletionSystem>(
            *campaign_,
            *active_site_run_,
            message_queue_,
            fixed_step_seconds_,
            move_direction);
        SiteCompletionSystem::run(completion_context);
    });

    record_timing_sample(fixed_step_timing_, elapsed_milliseconds_since(fixed_step_started_at));
}
}  // namespace gs1
