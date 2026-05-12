#include "app/game_presentation_coordinator.h"

#include "campaign/systems/technology_system.h"
#include "content/defs/faction_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/technology_defs.h"
#include "site/inventory_storage.h"
#include "site/site_projection_update_flags.h"
#include "site/site_world_access.h"
#include "site/systems/phone_panel_system.h"
#include "site/systems/site_system_context.h"
#include "site/tile_footprint.h"
#include "site/weather_contribution_logic.h"
#include "runtime/system_interface.h"
#include "support/currency.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <string_view>
#include <vector>

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
constexpr float k_visible_tile_device_integrity_projection_step = 100.0f / 128.0f;
constexpr double k_site_one_probe_window_minutes = 0.25;
constexpr std::int32_t k_regional_map_tile_spacing = 160;

struct RegionalMapSnapshotMessageFamily final
{
    Gs1EngineMessageType begin_message;
    Gs1EngineMessageType site_upsert_message;
    Gs1EngineMessageType link_upsert_message;
    Gs1EngineMessageType end_message;
};

inline constexpr RegionalMapSnapshotMessageFamily k_regional_map_scene_snapshot_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT};
inline constexpr RegionalMapSnapshotMessageFamily k_regional_map_hud_snapshot_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_HUD_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_HUD_SNAPSHOT};
inline constexpr RegionalMapSnapshotMessageFamily k_regional_summary_snapshot_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SUMMARY_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SUMMARY_SNAPSHOT};
inline constexpr RegionalMapSnapshotMessageFamily k_regional_selection_snapshot_family {
    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_SNAPSHOT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_UPSERT,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_UPSERT,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_SNAPSHOT};

[[nodiscard]] const char* regional_site_state_label(Gs1SiteState state) noexcept
{
    switch (state)
    {
    case GS1_SITE_STATE_AVAILABLE:
        return "Ready";
    case GS1_SITE_STATE_COMPLETED:
        return "Restored";
    case GS1_SITE_STATE_LOCKED:
    default:
        return "Locked";
    }
}

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

void queue_storage_open_request(
    std::deque<GameMessage>& message_queue,
    std::uint32_t storage_id)
{
    if (storage_id == 0U)
    {
        return;
    }

    GameMessage open_storage {};
    open_storage.type = GameMessageType::InventoryStorageViewRequest;
    open_storage.set_payload(InventoryStorageViewRequestMessage {
        storage_id,
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
        {0U, 0U, 0U}});
    message_queue.push_back(open_storage);
}

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

[[nodiscard]] std::uint16_t quantize_device_integrity_projection(float normalized_integrity) noexcept
{
    const float clamped = std::clamp(normalized_integrity, 0.0f, 1.0f);
    const float scaled = std::floor((clamped * 128.0f) + 0.0001f);
    return static_cast<std::uint16_t>(std::clamp(scaled, 0.0f, 128.0f));
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
        tile.device.structure_id.value != 0U
            ? quantize_device_integrity_projection(tile.device.device_integrity)
            : 0U,
        static_cast<std::uint8_t>(tile.excavation.depth),
        static_cast<std::uint8_t>(visible_excavation_depth),
        true};
}

[[nodiscard]] std::uint16_t quantize_visual_density(float density) noexcept
{
    return quantize_projected_tile_channel(
        density,
        100.0f,
        k_visible_tile_density_projection_step);
}

[[nodiscard]] float resolve_plant_visual_height_scale(const PlantDef& plant_def, float density) noexcept
{
    const float density_factor = std::clamp(density / 100.0f, 0.25f, 1.0f);
    float base_height = 0.45f;
    switch (plant_def.height_class)
    {
    case PlantHeightClass::None:
        base_height = 0.18f;
        break;
    case PlantHeightClass::Low:
        base_height = 0.45f;
        break;
    case PlantHeightClass::Medium:
        base_height = 0.75f;
        break;
    case PlantHeightClass::Tall:
        base_height = 1.05f;
        break;
    default:
        break;
    }

    return std::clamp(base_height * (0.55f + (density_factor * 0.55f)), 0.16f, 1.3f);
}

[[nodiscard]] std::uint8_t resolve_plant_visual_flags(const PlantDef& plant_def) noexcept
{
    std::uint8_t flags = 0U;
    if (plant_focus_has_aura(plant_def.focus))
    {
        flags |= GS1_SITE_PLANT_VISUAL_FLAG_HAS_AURA;
    }
    if (plant_focus_has_wind_projection(plant_def.focus))
    {
        flags |= GS1_SITE_PLANT_VISUAL_FLAG_HAS_WIND_PROJECTION;
    }
    if (plant_def.growable)
    {
        flags |= GS1_SITE_PLANT_VISUAL_FLAG_GROWABLE;
    }
    return flags;
}

[[nodiscard]] float resolve_device_visual_height_scale(const StructureDef& structure_def) noexcept
{
    float height = 0.85f;
    if (structure_def.grants_storage)
    {
        height += 0.15f;
    }
    if (structure_def.crafting_station_kind != CraftingStationKind::None)
    {
        height += 0.2f;
    }
    return std::clamp(height, 0.6f, 1.4f);
}

[[nodiscard]] std::uint8_t resolve_device_visual_flags(
    const StructureDef& structure_def,
    const SiteWorld::TileDeviceData& device) noexcept
{
    std::uint8_t flags = 0U;
    if (structure_def.grants_storage)
    {
        flags |= GS1_SITE_DEVICE_VISUAL_FLAG_HAS_STORAGE;
    }
    if (structure_def.crafting_station_kind != CraftingStationKind::None)
    {
        flags |= GS1_SITE_DEVICE_VISUAL_FLAG_IS_CRAFTING_STATION;
    }
    if (device.fixed_integrity)
    {
        flags |= GS1_SITE_DEVICE_VISUAL_FLAG_FIXED_INTEGRITY;
    }
    return flags;
}

[[nodiscard]] std::uint64_t plant_visual_id_for_anchor(const SiteRunState& site_run, TileCoord anchor) noexcept
{
    return site_run.site_world == nullptr ? 0U : site_run.site_world->tile_entity_id(anchor);
}

[[nodiscard]] std::uint64_t device_visual_id_for_anchor(const SiteRunState& site_run, TileCoord anchor) noexcept
{
    return site_run.site_world == nullptr ? 0U : site_run.site_world->device_entity_id(anchor);
}

struct PlacementPreviewTileProjectionState final
{
    TileCoord coord {};
    float wind_protection {0.0f};
    float heat_protection {0.0f};
    float dust_protection {0.0f};
    float final_wind_protection {0.0f};
    float final_heat_protection {0.0f};
    float final_dust_protection {0.0f};
    float occupant_condition {0.0f};
    std::uint8_t flags {0U};
};

struct PreviewOccupant final
{
    enum class Kind : std::uint8_t
    {
        None = 0,
        Plant = 1,
        Structure = 2
    };

    Kind kind {Kind::None};
    std::uint32_t content_id {0U};
    TileCoord anchor {};
    gs1::TileFootprint footprint {1U, 1U};
    float occupant_condition {0.0f};
    float wind_support {0.0f};
    float heat_support {0.0f};
    float dust_support {0.0f};
    float protection_ratio {1.0f};
    std::uint8_t aura_distance {0U};
    std::uint8_t wind_distance {0U};
};

[[nodiscard]] bool placement_mode_supports_preview_tiles(
    const PlacementModeState& placement_mode) noexcept
{
    return placement_mode.active &&
        placement_mode.target_tile.has_value() &&
        placement_mode.action_kind != ActionKind::None &&
        placement_mode.blocked_mask == 0ULL;
}

[[nodiscard]] std::uint8_t resolve_effective_preview_aura_distance(const PlantDef& plant_def) noexcept
{
    return gs1::scale_tile_distance_by_footprint_multiple(
        plant_def.aura_size,
        gs1::TileFootprint {plant_def.footprint_width, plant_def.footprint_height});
}

[[nodiscard]] std::uint8_t resolve_effective_preview_wind_distance(const PlantDef& plant_def) noexcept
{
    return gs1::scale_tile_distance_by_footprint_multiple(
        plant_def.wind_protection_range,
        gs1::TileFootprint {plant_def.footprint_width, plant_def.footprint_height});
}

[[nodiscard]] bool source_instance_projects_full_wind_shelter_to_target_instance(
    TileCoord source_anchor,
    gs1::TileFootprint source_footprint,
    TileCoord target_anchor,
    gs1::TileFootprint target_footprint,
    const WeatherDirectionStep& wind_direction,
    std::uint8_t protection_range) noexcept
{
    const std::int32_t source_min_x = source_anchor.x;
    const std::int32_t source_max_x = source_anchor.x + static_cast<std::int32_t>(source_footprint.width) - 1;
    const std::int32_t source_min_y = source_anchor.y;
    const std::int32_t source_max_y = source_anchor.y + static_cast<std::int32_t>(source_footprint.height) - 1;
    const std::int32_t target_min_x = target_anchor.x;
    const std::int32_t target_max_x = target_anchor.x + static_cast<std::int32_t>(target_footprint.width) - 1;
    const std::int32_t target_min_y = target_anchor.y;
    const std::int32_t target_max_y = target_anchor.y + static_cast<std::int32_t>(target_footprint.height) - 1;

    if (wind_direction.x > 0)
    {
        const std::int32_t gap = target_min_x - source_max_x;
        if (gap < 1 || gap > static_cast<std::int32_t>(protection_range))
        {
            return false;
        }

        return target_min_y <= source_max_y && target_max_y >= source_min_y;
    }

    if (wind_direction.x < 0)
    {
        const std::int32_t gap = source_min_x - target_max_x;
        if (gap < 1 || gap > static_cast<std::int32_t>(protection_range))
        {
            return false;
        }

        return target_min_y <= source_max_y && target_max_y >= source_min_y;
    }

    if (wind_direction.y > 0)
    {
        const std::int32_t gap = target_min_y - source_max_y;
        if (gap < 1 || gap > static_cast<std::int32_t>(protection_range))
        {
            return false;
        }

        return target_min_x <= source_max_x && target_max_x >= source_min_x;
    }

    if (wind_direction.y < 0)
    {
        const std::int32_t gap = source_min_y - target_max_y;
        if (gap < 1 || gap > static_cast<std::int32_t>(protection_range))
        {
            return false;
        }

        return target_min_x <= source_max_x && target_max_x >= source_min_x;
    }

    return false;
}

[[nodiscard]] PreviewOccupant resolve_preview_occupant(const SiteRunState& site_run) noexcept
{
    PreviewOccupant preview {};
    const auto& placement_mode = site_run.site_action.placement_mode;
    if (!placement_mode_supports_preview_tiles(placement_mode))
    {
        return preview;
    }

    if (placement_mode.action_kind == ActionKind::Plant)
    {
        const auto* item_def = find_item_def(ItemId {placement_mode.item_id});
        if (item_def == nullptr || item_def->linked_plant_id.value == 0U)
        {
            return preview;
        }

        const auto* plant_def = find_plant_def(item_def->linked_plant_id);
        if (plant_def == nullptr)
        {
            return preview;
        }

        preview.kind = PreviewOccupant::Kind::Plant;
        preview.content_id = plant_def->plant_id.value;
        preview.anchor = placement_mode.target_tile.value();
        preview.footprint = gs1::TileFootprint {plant_def->footprint_width, plant_def->footprint_height};
        preview.occupant_condition = 100.0f;
        preview.wind_support = resolve_density_scaled_support_value(plant_def->wind_resistance, 1.0f);
        preview.heat_support = resolve_density_scaled_support_value(plant_def->heat_tolerance, 1.0f);
        preview.dust_support = resolve_density_scaled_support_value(plant_def->dust_tolerance, 1.0f);
        preview.protection_ratio = std::clamp(plant_def->protection_ratio, 0.0f, 1.0f);
        preview.aura_distance = resolve_effective_preview_aura_distance(*plant_def);
        preview.wind_distance = resolve_effective_preview_wind_distance(*plant_def);
        return preview;
    }

    if (placement_mode.action_kind == ActionKind::Build)
    {
        const auto* item_def = find_item_def(ItemId {placement_mode.item_id});
        if (item_def == nullptr || item_def->linked_structure_id.value == 0U)
        {
            return preview;
        }

        const auto* structure_def = find_structure_def(item_def->linked_structure_id);
        if (structure_def == nullptr)
        {
            return preview;
        }

        preview.kind = PreviewOccupant::Kind::Structure;
        preview.content_id = structure_def->structure_id.value;
        preview.anchor = placement_mode.target_tile.value();
        preview.footprint = gs1::resolve_structure_tile_footprint(structure_def->structure_id);
        preview.occupant_condition = 100.0f;
        preview.wind_support = std::clamp(structure_def->wind_protection_value, 0.0f, 100.0f);
        preview.heat_support = std::clamp(structure_def->heat_protection_value, 0.0f, 100.0f);
        preview.dust_support = std::clamp(structure_def->dust_protection_value, 0.0f, 100.0f);
        preview.protection_ratio = 1.0f;
        preview.aura_distance = structure_def->aura_size;
        preview.wind_distance = structure_def->wind_protection_range;
        return preview;
    }

    return preview;
}

[[nodiscard]] bool preview_occupies_tile(const PreviewOccupant& preview, TileCoord coord) noexcept
{
    if (preview.kind == PreviewOccupant::Kind::None)
    {
        return false;
    }

    return coord.x >= preview.anchor.x &&
        coord.y >= preview.anchor.y &&
        coord.x < preview.anchor.x + static_cast<std::int32_t>(preview.footprint.width) &&
        coord.y < preview.anchor.y + static_cast<std::int32_t>(preview.footprint.height);
}

[[nodiscard]] std::vector<PlacementPreviewTileProjectionState> build_placement_preview_tile_projections(
    const SiteRunState& site_run) noexcept
{
    std::vector<PlacementPreviewTileProjectionState> projections {};
    const auto preview = resolve_preview_occupant(site_run);
    if (preview.kind == PreviewOccupant::Kind::None || site_run.site_world == nullptr)
    {
        return projections;
    }

    std::set<std::pair<std::int32_t, std::int32_t>> unique_tiles;
    const auto wind_direction = resolve_wind_direction_step(site_run.weather.weather_wind_direction_degrees);
    const auto max_distance = std::max(preview.aura_distance, preview.wind_distance);

    for (std::int32_t offset_y = 0; offset_y < static_cast<std::int32_t>(preview.footprint.height); ++offset_y)
    {
        for (std::int32_t offset_x = 0; offset_x < static_cast<std::int32_t>(preview.footprint.width); ++offset_x)
        {
            const TileCoord source_coord {preview.anchor.x + offset_x, preview.anchor.y + offset_y};
            for (std::int32_t dy = -static_cast<std::int32_t>(max_distance);
                dy <= static_cast<std::int32_t>(max_distance);
                ++dy)
            {
                for (std::int32_t dx = -static_cast<std::int32_t>(max_distance);
                    dx <= static_cast<std::int32_t>(max_distance);
                    ++dx)
                {
                    const int manhattan_distance = std::abs(dx) + std::abs(dy);
                    if (manhattan_distance > static_cast<int>(max_distance))
                    {
                        continue;
                    }

                    const TileCoord target_coord {source_coord.x + dx, source_coord.y + dy};
                    if (!site_world_access::tile_coord_in_bounds(site_run, target_coord))
                    {
                        continue;
                    }

                    unique_tiles.insert({target_coord.x, target_coord.y});
                }
            }
        }
    }

    projections.reserve(unique_tiles.size());
    for (const auto& coord_key : unique_tiles)
    {
        const TileCoord coord {coord_key.first, coord_key.second};
        auto total = site_run.site_world->tile_total_weather_contribution(coord);

        float preview_heat = 0.0f;
        float preview_wind = 0.0f;
        float preview_dust = 0.0f;
        const bool occupied_by_preview = preview_occupies_tile(preview, coord);

        for (std::int32_t offset_y = 0; offset_y < static_cast<std::int32_t>(preview.footprint.height); ++offset_y)
        {
            for (std::int32_t offset_x = 0; offset_x < static_cast<std::int32_t>(preview.footprint.width); ++offset_x)
            {
                const TileCoord source_coord {preview.anchor.x + offset_x, preview.anchor.y + offset_y};
                const int dx = coord.x - source_coord.x;
                const int dy = coord.y - source_coord.y;
                const int manhattan_distance = std::abs(dx) + std::abs(dy);
                if (manhattan_distance > static_cast<int>(max_distance))
                {
                    continue;
                }

                const float contribution_scale = resolve_contribution_scale(manhattan_distance);
                const bool same_instance = occupied_by_preview;
                const bool within_shared_aura =
                    manhattan_distance == 0 || manhattan_distance <= static_cast<int>(preview.aura_distance);
                if (within_shared_aura && !same_instance)
                {
                    preview_heat += preview.heat_support * preview.protection_ratio * contribution_scale;
                    preview_dust += preview.dust_support * preview.protection_ratio * contribution_scale;
                }

                if (!same_instance &&
                    manhattan_distance > 0 &&
                    manhattan_distance <= static_cast<int>(preview.wind_distance))
                {
                    const float shadow_scale = compute_directional_wind_shadow_scale(
                        source_coord.x,
                        source_coord.y,
                        coord.x,
                        coord.y,
                        preview.wind_distance,
                        wind_direction);
                    const bool full_instance_cover =
                        wind_direction.x != 0 && wind_direction.y == 0 &&
                        source_instance_projects_full_wind_shelter_to_target_instance(
                            preview.anchor,
                            preview.footprint,
                            coord,
                            occupied_by_preview ? preview.footprint : gs1::TileFootprint {1U, 1U},
                            wind_direction,
                            preview.wind_distance);
                    const float resolved_shadow_scale = full_instance_cover ? 1.0f : shadow_scale;
                    if (resolved_shadow_scale > k_weather_contribution_epsilon)
                    {
                        preview_wind = std::max(
                            preview_wind,
                            preview.wind_support * preview.protection_ratio * resolved_shadow_scale);
                    }
                }
            }
        }

        total.heat_protection = std::clamp(total.heat_protection + preview_heat, 0.0f, 100.0f);
        total.wind_protection = std::clamp(total.wind_protection + preview_wind, 0.0f, 100.0f);
        total.dust_protection = std::clamp(total.dust_protection + preview_dust, 0.0f, 100.0f);

        PlacementPreviewTileProjectionState projection {};
        projection.coord = coord;
        projection.wind_protection = total.wind_protection;
        projection.heat_protection = total.heat_protection;
        projection.dust_protection = total.dust_protection;
        projection.final_wind_protection = total.wind_protection;
        projection.final_heat_protection = total.heat_protection;
        projection.final_dust_protection = total.dust_protection;
        if (occupied_by_preview)
        {
            projection.flags |= GS1_PLACEMENT_PREVIEW_TILE_FLAG_OCCUPIED;
            projection.occupant_condition = preview.occupant_condition;
            if (preview.kind == PreviewOccupant::Kind::Plant)
            {
                projection.flags |= GS1_PLACEMENT_PREVIEW_TILE_FLAG_PLANT;
                projection.final_wind_protection =
                    std::clamp(total.wind_protection + preview.wind_support, 0.0f, 100.0f);
                projection.final_heat_protection =
                    std::clamp(total.heat_protection + preview.heat_support, 0.0f, 100.0f);
                projection.final_dust_protection =
                    std::clamp(total.dust_protection + preview.dust_support, 0.0f, 100.0f);
            }
            else if (preview.kind == PreviewOccupant::Kind::Structure)
            {
                projection.flags |= GS1_PLACEMENT_PREVIEW_TILE_FLAG_STRUCTURE;
            }
        }
        projections.push_back(projection);
    }

    std::sort(
        projections.begin(),
        projections.end(),
        [](const PlacementPreviewTileProjectionState& lhs, const PlacementPreviewTileProjectionState& rhs) {
            if (lhs.coord.y != rhs.coord.y)
            {
                return lhs.coord.y < rhs.coord.y;
            }
            return lhs.coord.x < rhs.coord.x;
        });
    return projections;
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
        lhs.device_integrity_quantized == rhs.device_integrity_quantized &&
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

[[nodiscard]] bool is_site_revealed(
    const CampaignState& campaign,
    SiteId site_id) noexcept
{
    for (const auto revealed_site_id : campaign.regional_map_state.revealed_site_ids)
    {
        if (revealed_site_id == site_id)
        {
            return true;
        }
    }

    return false;
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

Gs1RuntimeMessage make_engine_message(
    Gs1EngineMessageType type)
{
    Gs1RuntimeMessage message {};
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

}  // namespace

bool GamePresentationCoordinator::subscribes_to_host_message(Gs1HostMessageType type) noexcept
{
    return type == GS1_HOST_EVENT_SITE_SCENE_READY ||
        type == GS1_HOST_EVENT_UI_ACTION;
}

Gs1Status GamePresentationCoordinator::process_host_message(
    GamePresentationRuntimeContext& context,
    const Gs1HostMessage& message)
{
    active_context_ = &context;
    if (message.type == GS1_HOST_EVENT_SITE_SCENE_READY)
    {
        activate_loaded_site_scene(context);
        return GS1_STATUS_OK;
    }

    if (message.type == GS1_HOST_EVENT_UI_ACTION)
    {
        const auto& action = message.payload.ui_action.action;
        switch (action.type)
        {
        case GS1_UI_ACTION_START_NEW_CAMPAIGN:
            queue_clear_ui_panel_messages(GS1_UI_PANEL_MAIN_MENU);
            queue_app_state_message(app_state());
            queue_campaign_resources_message();
            queue_regional_map_snapshot_messages();
            queue_regional_map_menu_ui_messages();
            queue_regional_map_selection_ui_messages();
            queue_log_message("Started new GS1 campaign.");
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE:
            queue_regional_map_snapshot_messages();
            queue_regional_map_menu_ui_messages();
            queue_regional_map_selection_ui_messages();
            queue_log_message("Selected deployment site.");
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
            queue_regional_map_snapshot_messages();
            queue_regional_map_menu_ui_messages();
            queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
            queue_log_message("Cleared deployment site selection.");
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE:
            queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
            queue_site_protection_overlay_state_message();
            queue_regional_map_menu_ui_messages();
            queue_regional_map_tech_tree_ui_messages();
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE:
            queue_regional_map_menu_ui_messages();
            queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
            queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB:
            queue_regional_map_menu_ui_messages();
            queue_regional_map_tech_tree_ui_messages();
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_START_SITE_ATTEMPT:
            queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
            queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
            queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP);
            queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
            queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
            queue_app_state_message(app_state());
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP:
            queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
            queue_app_state_message(app_state());
            queue_campaign_resources_message();
            queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_RESULT);
            queue_regional_map_snapshot_messages();
            queue_regional_map_menu_ui_messages();
            queue_regional_map_selection_ui_messages();
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_SET_PHONE_PANEL_SECTION:
            queue_site_protection_overlay_state_message();
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR:
            queue_site_protection_selector_ui_messages();
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI:
        case GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE:
            queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
            queue_site_protection_overlay_state_message();
            return GS1_STATUS_OK;

        case GS1_UI_ACTION_CHECKOUT_PHONE_CART:
        case GS1_UI_ACTION_BUY_PHONE_LISTING:
        case GS1_UI_ACTION_SELL_PHONE_LISTING:
        case GS1_UI_ACTION_HIRE_CONTRACTOR:
        case GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE:
            queue_campaign_resources_message();
            return GS1_STATUS_OK;

        default:
            return GS1_STATUS_OK;
        }
    }

    return GS1_STATUS_OK;
}

void GamePresentationCoordinator::on_message_processed(GamePresentationRuntimeContext& context, const GameMessage& message)
{
    active_context_ = &context;
    switch (message.type)
    {
    case GameMessageType::OpenMainMenu:
        queue_close_active_normal_ui_if_open();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP);
        queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        queue_app_state_message(app_state());
        queue_main_menu_ui_messages();
        queue_log_message("Entered main menu.");
        break;

    case GameMessageType::StartNewCampaign:
        queue_clear_ui_panel_messages(GS1_UI_PANEL_MAIN_MENU);
        queue_app_state_message(app_state());
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
        queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        queue_log_message("Cleared deployment site selection.");
        break;

    case GameMessageType::OpenRegionalMapTechTree:
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        queue_site_protection_overlay_state_message();
        queue_regional_map_menu_ui_messages();
        queue_regional_map_tech_tree_ui_messages();
        break;

    case GameMessageType::CloseRegionalMapTechTree:
        queue_regional_map_menu_ui_messages();
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
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
        if (app_state_supports_technology_tree(app_state()))
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
        queue_site_protection_overlay_state_message();
        break;

    case GameMessageType::ClosePhonePanel:
        break;

    case GameMessageType::InventoryStorageViewRequest:
    {
        const auto& payload = message.payload_as<InventoryStorageViewRequestMessage>();
        if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            queue_site_protection_overlay_state_message();
        }
        break;
    }

    case GameMessageType::StartSiteAttempt:
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        queue_app_state_message(app_state());
        break;

    case GameMessageType::SiteRunStarted:
        break;

    case GameMessageType::ReturnToRegionalMap:
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        queue_app_state_message(app_state());
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
            active_site_run().has_value() ? active_site_run()->result_newly_revealed_site_count : 0U;
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);
        queue_close_progression_view_if_open(GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE);
        queue_app_state_message(app_state());
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
        flush_site_presentation_if_dirty(context);
        queue_task_reward_claimed_cue_message(
            payload.task_instance_id,
            payload.task_template_id,
            payload.reward_candidate_count);
        break;
    }

    case GameMessageType::InventoryCraftCompleted:
    {
        const auto& payload = message.payload_as<InventoryCraftCompletedMessage>();
        if (active_site_run().has_value() && payload.output_storage_id != 0U)
        {
            queue_storage_open_request(message_queue(), payload.output_storage_id);
            flush_site_presentation_if_dirty(context);
            queue_craft_output_stored_cue_message(
                payload.output_storage_id,
                payload.output_item_id,
                payload.output_quantity == 0U ? 1U : payload.output_quantity);
        }
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
        queue_site_protection_selector_ui_messages();
        break;

    case GameMessageType::CloseSiteProtectionUi:
    case GameMessageType::SetSiteProtectionOverlayMode:
        queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
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
        break;

    case GameMessageType::InventoryDeliveryRequested:
    case GameMessageType::InventoryItemUseRequested:
    case GameMessageType::InventoryItemConsumeRequested:
    case GameMessageType::InventoryTransferRequested:
    case GameMessageType::InventoryItemSubmitRequested:
    case GameMessageType::InventoryCraftContextRequested:
        break;

    case GameMessageType::PhoneListingPurchaseRequested:
    case GameMessageType::PhoneListingSaleRequested:
    case GameMessageType::ContractorHireRequested:
    case GameMessageType::SiteUnlockablePurchaseRequested:
        queue_campaign_resources_message();
        [[fallthrough]];
    default:
        break;
    }
}


void GamePresentationCoordinator::queue_log_message(const char* message, Gs1LogLevel level)
{
    auto engine_message = make_engine_message(GS1_ENGINE_MESSAGE_LOG_TEXT);
    auto& payload = engine_message.emplace_payload<Gs1EngineMessageLogTextData>();
    payload.level = level;
    strncpy_s(
        payload.text,
        sizeof(payload.text),
        message,
        _TRUNCATE);
    engine_messages().push_back(engine_message);
}

void GamePresentationCoordinator::queue_app_state_message(Gs1AppState app_state)
{
    if (last_emitted_app_state_.has_value() && last_emitted_app_state_.value() == app_state)
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SET_APP_STATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageSetAppStateData>();
    payload.app_state = app_state;
    engine_messages().push_back(message);
    last_emitted_app_state_ = app_state;
}

void GamePresentationCoordinator::queue_ui_setup_begin_message(
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
    engine_messages().push_back(message);

    active_ui_setups_[setup_id] = presentation_type;
    if (presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
    {
        active_normal_ui_setup_ = setup_id;
    }
}

void GamePresentationCoordinator::queue_ui_setup_close_message(
    Gs1UiSetupId setup_id,
    Gs1UiSetupPresentationType presentation_type)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP);
    auto& payload = message.emplace_payload<Gs1EngineMessageCloseUiSetupData>();
    payload.setup_id = setup_id;
    payload.presentation_type = presentation_type;
    engine_messages().push_back(message);

    active_ui_setups_.erase(setup_id);
    if (active_normal_ui_setup_.has_value() && active_normal_ui_setup_.value() == setup_id)
    {
        active_normal_ui_setup_.reset();
    }
}

void GamePresentationCoordinator::queue_ui_panel_begin_message(
    Gs1UiPanelId panel_id,
    std::uint32_t context_id,
    std::uint32_t text_line_count,
    std::uint32_t slot_action_count,
    std::uint32_t list_item_count,
    std::uint32_t list_action_count)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelData>();
    payload.panel_id = panel_id;
    payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    payload.context_id = context_id;
    payload.text_line_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(text_line_count, 255U));
    payload.slot_action_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(slot_action_count, 255U));
    payload.list_item_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(list_item_count, 255U));
    payload.list_action_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(list_action_count, 255U));
    engine_messages().push_back(message);
    if (panel_id == GS1_UI_PANEL_REGIONAL_MAP_SELECTION)
    {
        queue_ui_surface_visibility_message(GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, true);
    }
    active_ui_panels_.insert(panel_id);
}

void GamePresentationCoordinator::queue_ui_panel_close_message(Gs1UiPanelId panel_id)
{
    auto message = make_engine_message(
        panel_id == GS1_UI_PANEL_REGIONAL_MAP_SELECTION
            ? GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL
            : GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL);
    auto& payload = message.emplace_payload<Gs1EngineMessageCloseUiPanelData>();
    payload.panel_id = panel_id;
    engine_messages().push_back(message);
    if (panel_id == GS1_UI_PANEL_REGIONAL_MAP_SELECTION)
    {
        queue_ui_surface_visibility_message(GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, false);
    }
    active_ui_panels_.erase(panel_id);
}

void GamePresentationCoordinator::queue_ui_surface_visibility_message(Gs1UiSurfaceId surface_id, bool visible)
{
    if (surface_id == GS1_UI_SURFACE_NONE)
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiSurfaceVisibilityData>();
    payload.surface_id = surface_id;
    payload.visible = visible ? 1U : 0U;
    payload.reserved0 = 0U;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_progression_view_begin_message(
    Gs1ProgressionViewId view_id,
    std::uint32_t entry_count,
    std::uint32_t context_id)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW);
    auto& payload = message.emplace_payload<Gs1EngineMessageProgressionViewData>();
    payload.view_id = view_id;
    payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    payload.entry_count = static_cast<std::uint16_t>(std::min<std::uint32_t>(entry_count, 65535U));
    payload.context_id = context_id;
    engine_messages().push_back(message);
    if (view_id == GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE)
    {
        queue_ui_surface_visibility_message(GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY, true);
    }
}

void GamePresentationCoordinator::queue_progression_view_close_message(Gs1ProgressionViewId view_id)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW);
    auto& payload = message.emplace_payload<Gs1EngineMessageCloseProgressionViewData>();
    payload.view_id = view_id;
    engine_messages().push_back(message);
    if (view_id == GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE)
    {
        queue_ui_surface_visibility_message(GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY, false);
    }
}

void GamePresentationCoordinator::queue_close_ui_setup_if_open(Gs1UiSetupId setup_id)
{
    const auto it = active_ui_setups_.find(setup_id);
    if (it == active_ui_setups_.end())
    {
        return;
    }

    queue_ui_setup_close_message(setup_id, it->second);
}

void GamePresentationCoordinator::queue_close_ui_panel_if_open(Gs1UiPanelId panel_id)
{
    if (!active_ui_panels_.contains(panel_id))
    {
        return;
    }

    queue_ui_panel_close_message(panel_id);
}

void GamePresentationCoordinator::queue_close_progression_view_if_open(Gs1ProgressionViewId view_id)
{
    queue_progression_view_close_message(view_id);
}

void GamePresentationCoordinator::queue_close_active_normal_ui_if_open()
{
    if (!active_normal_ui_setup_.has_value())
    {
        return;
    }

    queue_close_ui_setup_if_open(active_normal_ui_setup_.value());
}

void GamePresentationCoordinator::queue_ui_element_message(
    std::uint32_t element_id,
    Gs1UiElementType element_type,
    std::uint32_t flags,
    const Gs1UiAction& action,
    std::uint8_t content_kind,
    std::uint32_t primary_id,
    std::uint32_t secondary_id,
    std::uint32_t quantity)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiElementData>();
    payload.element_id = element_id;
    payload.element_type = element_type;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.action = action;
    payload.content_kind = content_kind;
    payload.primary_id = primary_id;
    payload.secondary_id = secondary_id;
    payload.quantity = quantity;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_progression_entry_message(const Gs1EngineMessageProgressionEntryData& payload)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT);
    message.emplace_payload<Gs1EngineMessageProgressionEntryData>() = payload;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_ui_panel_text_message(
    std::uint16_t line_id,
    std::uint32_t flags,
    std::uint8_t text_kind,
    std::uint32_t primary_id,
    std::uint32_t secondary_id,
    std::uint32_t quantity,
    std::uint32_t aux_value)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelTextData>();
    payload.line_id = line_id;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.text_kind = text_kind;
    payload.primary_id = primary_id;
    payload.secondary_id = secondary_id;
    payload.quantity = quantity;
    payload.aux_value = aux_value;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_ui_panel_slot_action_message(
    Gs1UiPanelSlotId slot_id,
    std::uint32_t flags,
    const Gs1UiAction& action,
    std::uint8_t label_kind,
    std::uint32_t primary_id,
    std::uint32_t secondary_id,
    std::uint32_t quantity)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelSlotActionData>();
    payload.slot_id = slot_id;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.action = action;
    payload.label_kind = label_kind;
    payload.primary_id = primary_id;
    payload.secondary_id = secondary_id;
    payload.quantity = quantity;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_ui_panel_list_item_message(
    Gs1UiPanelListId list_id,
    std::uint32_t item_id,
    std::uint32_t flags,
    std::uint8_t primary_kind,
    std::uint8_t secondary_kind,
    std::uint32_t primary_id,
    std::uint32_t secondary_id,
    std::uint32_t quantity,
    std::int32_t map_x,
    std::int32_t map_y)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelListItemData>();
    payload.item_id = item_id;
    payload.list_id = list_id;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.primary_kind = primary_kind;
    payload.secondary_kind = secondary_kind;
    payload.primary_id = primary_id;
    payload.secondary_id = secondary_id;
    payload.quantity = quantity;
    payload.map_x = map_x;
    payload.map_y = map_y;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_ui_panel_list_action_message(
    Gs1UiPanelListId list_id,
    std::uint32_t item_id,
    Gs1UiPanelListActionRole role,
    std::uint32_t flags,
    const Gs1UiAction& action)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT);
    auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelListActionData>();
    payload.item_id = item_id;
    payload.list_id = list_id;
    payload.role = role;
    payload.flags = static_cast<std::uint8_t>(flags);
    payload.action = action;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_ui_setup_end_message()
{
    engine_messages().push_back(make_engine_message(GS1_ENGINE_MESSAGE_END_UI_SETUP));
}

void GamePresentationCoordinator::queue_progression_view_end_message()
{
    engine_messages().push_back(make_engine_message(GS1_ENGINE_MESSAGE_END_PROGRESSION_VIEW));
}

void GamePresentationCoordinator::queue_ui_panel_end_message()
{
    engine_messages().push_back(make_engine_message(GS1_ENGINE_MESSAGE_END_UI_PANEL));
}

void GamePresentationCoordinator::queue_clear_ui_setup_messages(Gs1UiSetupId setup_id)
{
    queue_close_ui_setup_if_open(setup_id);
}

void GamePresentationCoordinator::queue_clear_ui_panel_messages(Gs1UiPanelId panel_id)
{
    queue_close_ui_panel_if_open(panel_id);
}

void GamePresentationCoordinator::queue_main_menu_ui_messages()
{
    Gs1UiAction action {};
    action.type = GS1_UI_ACTION_START_NEW_CAMPAIGN;
    action.arg0 = 42ULL;
    action.arg1 = 30ULL;
    queue_ui_panel_begin_message(
        GS1_UI_PANEL_MAIN_MENU,
        0U,
        0U,
        1U,
        0U,
        0U);
    queue_ui_panel_slot_action_message(
        GS1_UI_PANEL_SLOT_PRIMARY,
        GS1_UI_PANEL_SLOT_FLAG_PRIMARY,
        action,
        10U);
    queue_ui_panel_end_message();
}

void GamePresentationCoordinator::queue_regional_map_menu_ui_messages()
{
    if (!campaign().has_value() || app_state() != GS1_APP_STATE_REGIONAL_MAP)
    {
        queue_clear_ui_panel_messages(GS1_UI_PANEL_REGIONAL_MAP);
        return;
    }

    const auto selected_faction_id = campaign()->regional_map_state.selected_tech_tree_faction_id.value != 0U
        ? campaign()->regional_map_state.selected_tech_tree_faction_id
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
    Gs1UiAction toggle_action {};
    toggle_action.type = campaign()->regional_map_state.tech_tree_open
        ? GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE
        : GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;

    std::uint32_t site_row_count = 0U;
    for (const auto revealed_site_id : campaign()->regional_map_state.revealed_site_ids)
    {
        if (find_site_meta(*campaign(), revealed_site_id.value) != nullptr)
        {
            ++site_row_count;
        }
    }

    queue_ui_panel_begin_message(
        GS1_UI_PANEL_REGIONAL_MAP,
        0U,
        1U,
        1U,
        site_row_count,
        site_row_count);
    queue_ui_panel_text_message(
        1U,
        0U,
        1U,
        selected_faction_id.value,
        static_cast<std::uint32_t>(TechnologySystem::faction_reputation(*campaign(), selected_faction_id)));
    queue_ui_panel_slot_action_message(
        GS1_UI_PANEL_SLOT_PRIMARY,
        GS1_UI_PANEL_SLOT_FLAG_PRIMARY,
        toggle_action,
        campaign()->regional_map_state.tech_tree_open ? 2U : 1U);

    for (const auto revealed_site_id : campaign()->regional_map_state.revealed_site_ids)
    {
        const SiteMetaState* site = find_site_meta(*campaign(), revealed_site_id.value);
        if (site == nullptr)
        {
            continue;
        }

        const bool selected = campaign()->regional_map_state.selected_site_id.has_value() &&
            campaign()->regional_map_state.selected_site_id->value == site->site_id.value;
        const std::uint32_t item_flags = selected ? GS1_UI_PANEL_LIST_ITEM_FLAG_SELECTED : GS1_UI_PANEL_LIST_ITEM_FLAG_NONE;

        queue_ui_panel_list_item_message(
            GS1_UI_PANEL_LIST_REGIONAL_SITES,
            site->site_id.value,
            item_flags,
            1U,
            2U,
            site->site_id.value,
            static_cast<std::uint32_t>(site->site_state),
            0U,
            static_cast<std::int32_t>(site->regional_map_tile.x),
            static_cast<std::int32_t>(site->regional_map_tile.y));

        Gs1UiAction select_action {};
        select_action.type = GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE;
        select_action.target_id = site->site_id.value;
        queue_ui_panel_list_action_message(
            GS1_UI_PANEL_LIST_REGIONAL_SITES,
            site->site_id.value,
            GS1_UI_PANEL_LIST_ACTION_ROLE_PRIMARY,
            site->site_state == GS1_SITE_STATE_LOCKED ? GS1_UI_PANEL_LIST_ITEM_FLAG_DISABLED : GS1_UI_PANEL_LIST_ITEM_FLAG_NONE,
            select_action);
    }

    queue_ui_panel_end_message();
}

void GamePresentationCoordinator::queue_regional_map_selection_ui_messages()
{
    if (!campaign().has_value() || !campaign()->regional_map_state.selected_site_id.has_value())
    {
        if (active_ui_panels_.contains(GS1_UI_PANEL_REGIONAL_MAP_SELECTION))
        {
            auto close = make_engine_message(GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL);
            auto& close_payload = close.emplace_payload<Gs1EngineMessageCloseUiPanelData>();
            close_payload.panel_id = GS1_UI_PANEL_REGIONAL_MAP_SELECTION;
            engine_messages().push_back(close);
            queue_ui_surface_visibility_message(GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, false);
            active_ui_panels_.erase(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);
        }
        return;
    }

    const auto site_id = campaign()->regional_map_state.selected_site_id->value;
    const auto loadout_item_count = visible_loadout_slot_count(campaign()->loadout_planner_state);
    const bool has_support_contributors = campaign()->loadout_planner_state.support_quota > 0U;
    const bool has_aura_support = !campaign()->loadout_planner_state.active_nearby_aura_modifier_ids.empty();
    const std::uint32_t summary_label_count =
        (has_support_contributors ? 1U : 0U) + (has_aura_support ? 1U : 0U);

    const std::uint32_t text_line_count = 1U + summary_label_count;
    auto begin = make_engine_message(GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_UI_PANEL);
    auto& begin_payload = begin.emplace_payload<Gs1EngineMessageUiPanelData>();
    begin_payload.panel_id = GS1_UI_PANEL_REGIONAL_MAP_SELECTION;
    begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
    begin_payload.context_id = site_id;
    begin_payload.text_line_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(text_line_count, 255U));
    begin_payload.slot_action_count = 2U;
    begin_payload.list_item_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(loadout_item_count, 255U));
    begin_payload.list_action_count = 0U;
    engine_messages().push_back(begin);
    queue_ui_surface_visibility_message(GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL, true);
    active_ui_panels_.insert(GS1_UI_PANEL_REGIONAL_MAP_SELECTION);

    const auto queue_selection_text_message = [this](
                                                std::uint16_t line_id,
                                                std::uint32_t flags,
                                                std::uint8_t text_kind,
                                                std::uint32_t primary_id = 0U,
                                                std::uint32_t secondary_id = 0U,
                                                std::uint32_t quantity = 0U,
                                                std::uint32_t aux_value = 0U)
    {
        auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT);
        auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelTextData>();
        payload.line_id = line_id;
        payload.flags = static_cast<std::uint8_t>(flags);
        payload.text_kind = text_kind;
        payload.primary_id = primary_id;
        payload.secondary_id = secondary_id;
        payload.quantity = quantity;
        payload.aux_value = aux_value;
        engine_messages().push_back(message);
    };
    const auto queue_selection_slot_action_message = [this](
                                                       Gs1UiPanelSlotId slot_id,
                                                       std::uint32_t flags,
                                                       const Gs1UiAction& action,
                                                       std::uint8_t label_kind,
                                                       std::uint32_t primary_id = 0U,
                                                       std::uint32_t secondary_id = 0U,
                                                       std::uint32_t quantity = 0U)
    {
        auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT);
        auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelSlotActionData>();
        payload.slot_id = slot_id;
        payload.flags = static_cast<std::uint8_t>(flags);
        payload.action = action;
        payload.label_kind = label_kind;
        payload.primary_id = primary_id;
        payload.secondary_id = secondary_id;
        payload.quantity = quantity;
        engine_messages().push_back(message);
    };
    const auto queue_selection_list_item_message = [this](
                                                     Gs1UiPanelListId list_id,
                                                     std::uint32_t item_id,
                                                     std::uint32_t flags,
                                                     std::uint8_t primary_kind,
                                                     std::uint8_t secondary_kind,
                                                     std::uint32_t primary_id = 0U,
                                                     std::uint32_t secondary_id = 0U,
                                                     std::uint32_t quantity = 0U,
                                                     std::int32_t map_x = 0,
                                                     std::int32_t map_y = 0)
    {
        auto message = make_engine_message(GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT);
        auto& payload = message.emplace_payload<Gs1EngineMessageUiPanelListItemData>();
        payload.item_id = item_id;
        payload.list_id = list_id;
        payload.flags = static_cast<std::uint8_t>(flags);
        payload.primary_kind = primary_kind;
        payload.secondary_kind = secondary_kind;
        payload.primary_id = primary_id;
        payload.secondary_id = secondary_id;
        payload.quantity = quantity;
        payload.map_x = map_x;
        payload.map_y = map_y;
        engine_messages().push_back(message);
    };

    std::uint16_t next_line_id = 1U;
    queue_selection_text_message(next_line_id++, 0U, 3U, site_id);

    if (has_support_contributors)
    {
        queue_selection_text_message(
            next_line_id++,
            0U,
            4U,
            0U,
            0U,
            campaign()->loadout_planner_state.support_quota);
    }

    if (has_aura_support)
    {
        queue_selection_text_message(
            next_line_id++,
            0U,
            5U,
            0U,
            0U,
            static_cast<std::uint32_t>(campaign()->loadout_planner_state.active_nearby_aura_modifier_ids.size()));
    }

    std::uint32_t next_loadout_item_id = 1U;
    for (const auto& slot : campaign()->loadout_planner_state.selected_loadout_slots)
    {
        if (!slot.occupied || slot.item_id.value == 0U || slot.quantity == 0U)
        {
            continue;
        }

        queue_selection_list_item_message(
            GS1_UI_PANEL_LIST_REGIONAL_LOADOUT,
            next_loadout_item_id++,
            GS1_UI_PANEL_LIST_ITEM_FLAG_NONE,
            6U,
            0U,
            slot.item_id.value,
            0U,
            slot.quantity,
            0,
            0);
    }

    Gs1UiAction deploy_action {};
    deploy_action.type = GS1_UI_ACTION_START_SITE_ATTEMPT;
    deploy_action.target_id = site_id;
    queue_selection_slot_action_message(
        GS1_UI_PANEL_SLOT_PRIMARY,
        GS1_UI_PANEL_SLOT_FLAG_PRIMARY,
        deploy_action,
        3U,
        site_id);

    Gs1UiAction clear_selection_action {};
    clear_selection_action.type = GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    queue_selection_slot_action_message(
        GS1_UI_PANEL_SLOT_SECONDARY,
        GS1_UI_PANEL_SLOT_FLAG_NONE,
        clear_selection_action,
        4U);

    engine_messages().push_back(make_engine_message(GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL));
}

void GamePresentationCoordinator::queue_regional_map_tech_tree_ui_messages()
{
    if (!campaign().has_value() ||
        !app_state_supports_technology_tree(app_state()) ||
        !campaign()->regional_map_state.tech_tree_open)
    {
        return;
    }

    queue_close_ui_setup_if_open(GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE);

    const auto reputation_unlock_defs = all_reputation_unlock_defs();
    const auto technology_node_defs = all_technology_node_defs();
    const std::uint32_t entry_count =
        static_cast<std::uint32_t>(reputation_unlock_defs.size() + technology_node_defs.size() + 1U);
    queue_progression_view_begin_message(
        GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE,
        entry_count,
        0U);

    Gs1EngineMessageProgressionEntryData starter_entry {};
    starter_entry.entry_id = 0U;
    starter_entry.reputation_requirement = 0U;
    starter_entry.entry_kind = GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK;
    starter_entry.flags = GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED;
    queue_progression_entry_message(starter_entry);

    for (const auto& unlock_def : reputation_unlock_defs)
    {
        Gs1EngineMessageProgressionEntryData entry {};
        entry.entry_id = static_cast<std::uint16_t>(unlock_def.unlock_id);
        entry.reputation_requirement = static_cast<std::uint16_t>(unlock_def.reputation_requirement);
        entry.content_id = static_cast<std::uint16_t>(unlock_def.content_id);
        entry.entry_kind = GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK;
        entry.content_kind = static_cast<std::uint8_t>(unlock_def.unlock_kind);
        entry.flags = campaign()->technology_state.total_reputation >= unlock_def.reputation_requirement
            ? GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED
            : GS1_PROGRESSION_ENTRY_FLAG_LOCKED;
        queue_progression_entry_message(entry);
    }

    for (const auto& node_def : technology_node_defs)
    {
        const auto reputation_requirement = TechnologySystem::current_reputation_requirement(node_def);

        Gs1EngineMessageProgressionEntryData entry {};
        entry.entry_id = static_cast<std::uint16_t>(node_def.tech_node_id.value);
        entry.reputation_requirement = static_cast<std::uint16_t>(reputation_requirement);
        entry.tech_node_id = static_cast<std::uint16_t>(node_def.tech_node_id.value);
        entry.faction_id = static_cast<std::uint8_t>(node_def.faction_id.value);
        entry.entry_kind = GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE;
        entry.tier_index = node_def.tier_index;
        entry.action.type = GS1_UI_ACTION_NONE;
        entry.action.target_id = node_def.tech_node_id.value;
        entry.action.arg0 = node_def.faction_id.value;
        entry.flags = TechnologySystem::node_purchased(*campaign(), node_def.tech_node_id)
            ? GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED
            : GS1_PROGRESSION_ENTRY_FLAG_LOCKED;
        queue_progression_entry_message(entry);
    }

    queue_progression_view_end_message();
}

void GamePresentationCoordinator::queue_site_result_ui_messages(std::uint32_t site_id, Gs1SiteAttemptResult result)
{
    queue_ui_setup_begin_message(
        GS1_UI_SETUP_SITE_RESULT,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        2U,
        site_id);

    Gs1UiAction no_action {};
    queue_ui_element_message(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        1U,
        site_id,
        static_cast<std::uint32_t>(result));

    Gs1UiAction return_action {};
    return_action.type = GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP;
    queue_ui_element_message(
        2U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        return_action,
        2U);

    queue_ui_setup_end_message();
}

void GamePresentationCoordinator::queue_site_protection_selector_ui_messages()
{
    if (!active_site_run().has_value() || app_state() != GS1_APP_STATE_SITE_ACTIVE)
    {
        return;
    }

    queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
    queue_ui_setup_begin_message(
        GS1_UI_SETUP_SITE_PROTECTION_SELECTOR,
        GS1_UI_SETUP_PRESENTATION_OVERLAY,
        6U,
        active_site_run()->site_id.value);

    Gs1UiAction no_action {};
    queue_ui_element_message(
        1U,
        GS1_UI_ELEMENT_LABEL,
        GS1_UI_ELEMENT_FLAG_NONE,
        no_action,
        3U);

    Gs1UiAction wind_action {};
    wind_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    wind_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_WIND;
    queue_ui_element_message(
        2U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        wind_action,
        4U,
        GS1_SITE_PROTECTION_OVERLAY_WIND);

    Gs1UiAction heat_action {};
    heat_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    heat_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_HEAT;
    queue_ui_element_message(
        3U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        heat_action,
        4U,
        GS1_SITE_PROTECTION_OVERLAY_HEAT);

    Gs1UiAction dust_action {};
    dust_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    dust_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_DUST;
    queue_ui_element_message(
        4U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        dust_action,
        4U,
        GS1_SITE_PROTECTION_OVERLAY_DUST);

    Gs1UiAction occupant_condition_action {};
    occupant_condition_action.type = GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    occupant_condition_action.arg0 = GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION;
    queue_ui_element_message(
        5U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_PRIMARY,
        occupant_condition_action,
        4U,
        GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION);

    Gs1UiAction close_action {};
    close_action.type = GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI;
    queue_ui_element_message(
        6U,
        GS1_UI_ELEMENT_BUTTON,
        GS1_UI_ELEMENT_FLAG_BACKGROUND_CLICK,
        close_action,
        5U);

    queue_ui_setup_end_message();
}

void GamePresentationCoordinator::queue_regional_map_snapshot_messages()
{
    if (!campaign().has_value())
    {
        return;
    }

    std::set<std::pair<std::uint32_t, std::uint32_t>> unique_links;
    for (const auto& site : campaign()->sites)
    {
        for (const auto& adjacent_site_id : site.adjacent_site_ids)
        {
            const auto low = (site.site_id.value < adjacent_site_id.value) ? site.site_id.value : adjacent_site_id.value;
            const auto high = (site.site_id.value < adjacent_site_id.value) ? adjacent_site_id.value : site.site_id.value;
            unique_links.insert({low, high});
        }
    }

    std::uint32_t revealed_site_count = 0U;
    std::uint32_t revealed_link_count = 0U;
    for (const auto& site : campaign()->sites)
    {
        if (is_site_revealed(*campaign(), site.site_id))
        {
            revealed_site_count += 1U;
        }
    }
    for (const auto& link : unique_links)
    {
        if (!is_site_revealed(*campaign(), SiteId {link.first}) ||
            !is_site_revealed(*campaign(), SiteId {link.second}))
        {
            continue;
        }

        revealed_link_count += 1U;
    }

    const auto queue_family = [this, &unique_links, revealed_link_count, revealed_site_count](
                                  const RegionalMapSnapshotMessageFamily& family)
    {
        auto begin = make_engine_message(family.begin_message);
        auto& begin_payload = begin.emplace_payload<Gs1EngineMessageRegionalMapSnapshotData>();
        begin_payload.mode = GS1_PROJECTION_MODE_SNAPSHOT;
        begin_payload.link_count = revealed_link_count;
        begin_payload.selected_site_id =
            campaign()->regional_map_state.selected_site_id.has_value()
                ? campaign()->regional_map_state.selected_site_id->value
                : 0U;
        begin_payload.site_count = revealed_site_count;
        engine_messages().push_back(begin);

        for (const auto& site : campaign()->sites)
        {
            if (!is_site_revealed(*campaign(), site.site_id))
            {
                continue;
            }

            queue_regional_map_site_upsert_message(site, family.site_upsert_message);
        }

        for (const auto& link : unique_links)
        {
            if (!is_site_revealed(*campaign(), SiteId {link.first}) ||
                !is_site_revealed(*campaign(), SiteId {link.second}))
            {
                continue;
            }

            auto link_message = make_engine_message(family.link_upsert_message);
            auto& payload = link_message.emplace_payload<Gs1EngineMessageRegionalMapLinkData>();
            payload.from_site_id = link.first;
            payload.to_site_id = link.second;
            engine_messages().push_back(link_message);
        }

        engine_messages().push_back(make_engine_message(family.end_message));
    };

    queue_family(k_regional_map_scene_snapshot_family);
    queue_family(k_regional_map_hud_snapshot_family);
    queue_family(k_regional_summary_snapshot_family);
    queue_family(k_regional_selection_snapshot_family);
}

void GamePresentationCoordinator::queue_regional_map_site_upsert_message(
    const SiteMetaState& site,
    Gs1EngineMessageType message_type)
{
    if (!campaign().has_value())
    {
        return;
    }

    auto site_message = make_engine_message(message_type);
    auto& payload = site_message.emplace_payload<Gs1EngineMessageRegionalMapSiteData>();
    payload.site_id = site.site_id.value;
    payload.site_state = site.site_state;
    payload.site_archetype_id = site.site_archetype_id;
    payload.flags =
        (campaign()->regional_map_state.selected_site_id.has_value() &&
            campaign()->regional_map_state.selected_site_id->value == site.site_id.value)
        ? 1U
        : 0U;

    payload.map_x = site.regional_map_tile.x * k_regional_map_tile_spacing;
    payload.map_y = site.regional_map_tile.y * k_regional_map_tile_spacing;
    payload.support_package_id =
        site.has_support_package_id ? site.support_package_id : 0U;
    payload.support_preview_mask =
        site_contributes_to_selected_target(*campaign(), site)
            ? support_preview_mask_for_site(site)
            : 0U;
    engine_messages().push_back(site_message);
}

void GamePresentationCoordinator::queue_site_snapshot_begin_message(
    Gs1ProjectionMode mode,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto begin = make_engine_message(message_type);
    auto& begin_payload = begin.emplace_payload<Gs1EngineMessageSiteSnapshotData>();
    begin_payload.mode = mode;
    begin_payload.site_id = active_site_run()->site_id.value;
    begin_payload.site_archetype_id = active_site_run()->site_archetype_id;
    begin_payload.width = static_cast<std::uint16_t>(site_world_access::width(active_site_run().value()));
    begin_payload.height = static_cast<std::uint16_t>(site_world_access::height(active_site_run().value()));
    engine_messages().push_back(begin);
}

void GamePresentationCoordinator::queue_site_snapshot_end_message(Gs1EngineMessageType message_type)
{
    engine_messages().push_back(make_engine_message(message_type));
}

void GamePresentationCoordinator::queue_site_tile_upsert_message(
    std::uint32_t x,
    std::uint32_t y,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto& site_run = active_site_run().value();
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

    auto tile_message = make_engine_message(message_type);
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
    payload.device_integrity_quantized = projected_state.device_integrity_quantized;
    payload.excavation_depth = projected_state.excavation_depth;
    payload.visible_excavation_depth = projected_state.visible_excavation_depth;
    engine_messages().push_back(tile_message);

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

void GamePresentationCoordinator::queue_all_site_tile_upsert_messages(Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto& site_run = active_site_run().value();
    const auto tile_count = site_run.site_world->tile_count();
    site_run.last_projected_tile_states.assign(tile_count, ProjectedSiteTileState {});

    const auto width = site_world_access::width(site_run);
    const auto height = site_world_access::height(site_run);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            queue_site_tile_upsert_message(x, y, message_type);
        }
    }
}

void GamePresentationCoordinator::queue_pending_site_tile_upsert_messages(Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto& site_run = active_site_run().value();
    if (site_run.pending_full_tile_projection_update || site_run.pending_tile_projection_updates.empty())
    {
        queue_all_site_tile_upsert_messages(message_type);
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
            static_cast<std::uint32_t>(coord.y),
            message_type);
    }
}

void GamePresentationCoordinator::queue_site_worker_update_message(Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto worker_position = site_world_access::worker_position(active_site_run().value());
    const auto worker_conditions = site_world_access::worker_conditions(active_site_run().value());

    auto worker_message = make_engine_message(message_type);
    auto& worker_payload = worker_message.emplace_payload<Gs1EngineMessageWorkerData>();
    worker_payload.entity_id = active_site_run()->site_world != nullptr
        ? active_site_run()->site_world->worker_entity_id()
        : 0U;
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
        static_cast<Gs1SiteActionKind>(active_site_run()->site_action.action_kind);
    worker_payload.reserved0 = 0U;
    engine_messages().push_back(worker_message);
}

void GamePresentationCoordinator::queue_site_plant_visual_upsert_message(
    TileCoord coord,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value() || active_site_run()->site_world == nullptr)
    {
        return;
    }

    auto& site_run = active_site_run().value();
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return;
    }

    const auto ecology = site_world_access::tile_ecology(site_run, coord);
    if (ecology.plant_id.value == 0U)
    {
        return;
    }
    const PlantDef* plant_def = find_plant_def(ecology.plant_id);
    if (plant_def == nullptr)
    {
        return;
    }

    const TileFootprint footprint = resolve_plant_tile_footprint(ecology.plant_id);
    const TileCoord anchor = align_tile_anchor_to_footprint(coord, footprint);
    if (anchor.x != coord.x || anchor.y != coord.y)
    {
        return;
    }

    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessageSitePlantVisualData>();
    payload.visual_id = plant_visual_id_for_anchor(site_run, anchor);
    payload.plant_type_id = ecology.plant_id.value;
    payload.anchor_tile_x = static_cast<float>(anchor.x);
    payload.anchor_tile_y = static_cast<float>(anchor.y);
    payload.height_scale = resolve_plant_visual_height_scale(*plant_def, ecology.plant_density);
    payload.density_quantized = quantize_visual_density(ecology.plant_density);
    payload.footprint_width = footprint.width;
    payload.footprint_height = footprint.height;
    payload.height_class = static_cast<std::uint8_t>(plant_def->height_class);
    payload.focus = static_cast<std::uint8_t>(plant_def->focus);
    payload.flags = resolve_plant_visual_flags(*plant_def);
    payload.reserved0 = 0U;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_plant_visual_remove_message(
    std::uint64_t visual_id,
    Gs1EngineMessageType message_type)
{
    if (visual_id == 0U)
    {
        return;
    }

    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteVisualRemoveData>();
    payload.visual_id = visual_id;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_all_site_plant_visual_messages(Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value() || active_site_run()->site_world == nullptr)
    {
        return;
    }

    auto& site_run = active_site_run().value();
    const auto width = site_world_access::width(site_run);
    const auto height = site_world_access::height(site_run);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            queue_site_plant_visual_upsert_message(
                TileCoord {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)},
                message_type);
        }
    }
}

void GamePresentationCoordinator::queue_pending_site_plant_visual_messages(
    Gs1EngineMessageType upsert_message_type,
    Gs1EngineMessageType remove_message_type)
{
    if (!active_site_run().has_value() || active_site_run()->site_world == nullptr)
    {
        return;
    }

    auto& site_run = active_site_run().value();
    if (site_run.pending_full_tile_projection_update || site_run.pending_tile_projection_updates.empty())
    {
        queue_all_site_plant_visual_messages(upsert_message_type);
        return;
    }

    std::set<std::uint64_t> emitted_visual_ids {};
    for (const TileCoord coord : site_run.pending_tile_projection_updates)
    {
        if (!site_world_access::tile_coord_in_bounds(site_run, coord))
        {
            continue;
        }

        const auto tile_index = site_world_access::tile_index(site_run, coord);
        const ProjectedSiteTileState previous_projection =
            tile_index < site_run.last_projected_tile_states.size()
                ? site_run.last_projected_tile_states[tile_index]
                : ProjectedSiteTileState {};

        const auto ecology = site_world_access::tile_ecology(site_run, coord);
        if (ecology.plant_id.value != 0U)
        {
            const TileFootprint footprint = resolve_plant_tile_footprint(ecology.plant_id);
            const TileCoord anchor = align_tile_anchor_to_footprint(coord, footprint);
            const std::uint64_t visual_id = plant_visual_id_for_anchor(site_run, anchor);
            if (emitted_visual_ids.insert(visual_id).second)
            {
                queue_site_plant_visual_upsert_message(anchor, upsert_message_type);
            }

            if (previous_projection.plant_type_id != 0U && previous_projection.plant_type_id != ecology.plant_id.value)
            {
                const TileFootprint previous_footprint =
                    resolve_plant_tile_footprint(PlantId {previous_projection.plant_type_id});
                const TileCoord previous_anchor = align_tile_anchor_to_footprint(coord, previous_footprint);
                const std::uint64_t previous_visual_id = plant_visual_id_for_anchor(site_run, previous_anchor);
                if (previous_visual_id != visual_id && emitted_visual_ids.insert(previous_visual_id).second)
                {
                    queue_site_plant_visual_remove_message(previous_visual_id, remove_message_type);
                }
            }
            continue;
        }

        if (previous_projection.plant_type_id == 0U)
        {
            continue;
        }

        const TileFootprint previous_footprint =
            resolve_plant_tile_footprint(PlantId {previous_projection.plant_type_id});
        const TileCoord previous_anchor = align_tile_anchor_to_footprint(coord, previous_footprint);
        const std::uint64_t visual_id = plant_visual_id_for_anchor(site_run, previous_anchor);
        if (emitted_visual_ids.insert(visual_id).second)
        {
            queue_site_plant_visual_remove_message(visual_id, remove_message_type);
        }
    }
}

void GamePresentationCoordinator::queue_site_device_visual_upsert_message(
    TileCoord coord,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value() || active_site_run()->site_world == nullptr)
    {
        return;
    }

    auto& site_run = active_site_run().value();
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return;
    }

    const auto device = site_world_access::tile_device(site_run, coord);
    if (device.structure_id.value == 0U)
    {
        return;
    }
    const StructureDef* structure_def = find_structure_def(device.structure_id);
    if (structure_def == nullptr)
    {
        return;
    }

    const TileFootprint footprint = resolve_structure_tile_footprint(device.structure_id);
    const TileCoord anchor = align_tile_anchor_to_footprint(coord, footprint);
    if (anchor.x != coord.x || anchor.y != coord.y)
    {
        return;
    }

    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteDeviceVisualData>();
    payload.visual_id = device_visual_id_for_anchor(site_run, anchor);
    payload.structure_type_id = device.structure_id.value;
    payload.anchor_tile_x = static_cast<float>(anchor.x);
    payload.anchor_tile_y = static_cast<float>(anchor.y);
    payload.integrity_normalized = std::clamp(device.device_integrity, 0.0f, 1.0f);
    payload.height_scale = resolve_device_visual_height_scale(*structure_def);
    payload.footprint_width = footprint.width;
    payload.footprint_height = footprint.height;
    payload.flags = resolve_device_visual_flags(*structure_def, device);
    payload.reserved0 = 0U;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_device_visual_remove_message(
    std::uint64_t visual_id,
    Gs1EngineMessageType message_type)
{
    if (visual_id == 0U)
    {
        return;
    }

    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteVisualRemoveData>();
    payload.visual_id = visual_id;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_all_site_device_visual_messages(Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value() || active_site_run()->site_world == nullptr)
    {
        return;
    }

    auto& site_run = active_site_run().value();
    const auto width = site_world_access::width(site_run);
    const auto height = site_world_access::height(site_run);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        for (std::uint32_t x = 0; x < width; ++x)
        {
            queue_site_device_visual_upsert_message(
                TileCoord {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)},
                message_type);
        }
    }
}

void GamePresentationCoordinator::queue_pending_site_device_visual_messages(
    Gs1EngineMessageType upsert_message_type,
    Gs1EngineMessageType remove_message_type)
{
    if (!active_site_run().has_value() || active_site_run()->site_world == nullptr)
    {
        return;
    }

    auto& site_run = active_site_run().value();
    if (site_run.pending_full_tile_projection_update || site_run.pending_tile_projection_updates.empty())
    {
        queue_all_site_device_visual_messages(upsert_message_type);
        return;
    }

    std::set<std::uint64_t> emitted_visual_ids {};
    for (const TileCoord coord : site_run.pending_tile_projection_updates)
    {
        if (!site_world_access::tile_coord_in_bounds(site_run, coord))
        {
            continue;
        }

        const auto tile_index = site_world_access::tile_index(site_run, coord);
        const ProjectedSiteTileState previous_projection =
            tile_index < site_run.last_projected_tile_states.size()
                ? site_run.last_projected_tile_states[tile_index]
                : ProjectedSiteTileState {};

        const auto device = site_world_access::tile_device(site_run, coord);
        if (device.structure_id.value != 0U)
        {
            const TileFootprint footprint = resolve_structure_tile_footprint(device.structure_id);
            const TileCoord anchor = align_tile_anchor_to_footprint(coord, footprint);
            const std::uint64_t visual_id = device_visual_id_for_anchor(site_run, anchor);
            if (emitted_visual_ids.insert(visual_id).second)
            {
                queue_site_device_visual_upsert_message(anchor, upsert_message_type);
            }

            if (previous_projection.structure_type_id != 0U &&
                previous_projection.structure_type_id != device.structure_id.value)
            {
                const TileFootprint previous_footprint =
                    resolve_structure_tile_footprint(StructureId {previous_projection.structure_type_id});
                const TileCoord previous_anchor = align_tile_anchor_to_footprint(coord, previous_footprint);
                const std::uint64_t previous_visual_id = device_visual_id_for_anchor(site_run, previous_anchor);
                if (previous_visual_id != visual_id && emitted_visual_ids.insert(previous_visual_id).second)
                {
                    queue_site_device_visual_remove_message(previous_visual_id, remove_message_type);
                }
            }
            continue;
        }

        if (previous_projection.structure_type_id == 0U)
        {
            continue;
        }

        const TileFootprint previous_footprint =
            resolve_structure_tile_footprint(StructureId {previous_projection.structure_type_id});
        const TileCoord previous_anchor = align_tile_anchor_to_footprint(coord, previous_footprint);
        const std::uint64_t visual_id = device_visual_id_for_anchor(site_run, previous_anchor);
        if (emitted_visual_ids.insert(visual_id).second)
        {
            queue_site_device_visual_remove_message(visual_id, remove_message_type);
        }
    }
}

void GamePresentationCoordinator::queue_site_camp_update_message()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto camp_message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE);
    auto& camp_payload = camp_message.emplace_payload<Gs1EngineMessageCampData>();
    camp_payload.tile_x = active_site_run()->camp.camp_anchor_tile.x;
    camp_payload.tile_y = active_site_run()->camp.camp_anchor_tile.y;
    camp_payload.durability_normalized = active_site_run()->camp.camp_durability / 100.0f;
    camp_payload.flags =
        (active_site_run()->camp.delivery_point_operational ? 1U : 0U) |
        (active_site_run()->camp.shared_storage_access_enabled ? 2U : 0U);
    engine_messages().push_back(camp_message);
}

void GamePresentationCoordinator::queue_site_weather_update_message()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto weather_message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE);
    auto& weather_payload = weather_message.emplace_payload<Gs1EngineMessageWeatherData>();
    weather_payload.heat = active_site_run()->weather.weather_heat;
    weather_payload.wind = active_site_run()->weather.weather_wind;
    weather_payload.dust = active_site_run()->weather.weather_dust;
    weather_payload.wind_direction_degrees = active_site_run()->weather.weather_wind_direction_degrees;
    weather_payload.event_template_id =
        active_site_run()->event.active_event_template_id.has_value()
        ? active_site_run()->event.active_event_template_id->value
            : 0U;
    weather_payload.event_start_time_minutes =
        static_cast<float>(active_site_run()->event.start_time_minutes);
    weather_payload.event_peak_time_minutes =
        static_cast<float>(active_site_run()->event.peak_time_minutes);
    weather_payload.event_peak_duration_minutes =
        static_cast<float>(active_site_run()->event.peak_duration_minutes);
    weather_payload.event_end_time_minutes =
        static_cast<float>(active_site_run()->event.end_time_minutes);
    engine_messages().push_back(weather_message);
}

void GamePresentationCoordinator::queue_site_inventory_slot_upsert_message(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index,
    std::uint32_t storage_id,
    std::uint64_t container_owner_id,
    TileCoord container_tile,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto& site_run = active_site_run().value();
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

    auto message = make_engine_message(message_type);
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
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_inventory_storage_upsert_message(
    std::uint32_t storage_id,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto* storage_state =
        inventory_storage::storage_container_state_for_storage_id(active_site_run().value(), storage_id);
    if (storage_state == nullptr)
    {
        return;
    }

    auto message = make_engine_message(message_type);
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
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_all_site_inventory_storage_upsert_messages(Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    for (const auto& storage : active_site_run()->inventory.storage_containers)
    {
        queue_site_inventory_storage_upsert_message(storage.storage_id, message_type);
    }
}

void GamePresentationCoordinator::queue_site_inventory_view_state_message(
    std::uint32_t storage_id,
    Gs1InventoryViewEventKind event_kind,
    std::uint32_t slot_count,
    Gs1EngineMessageType message_type)
{
    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessageInventoryViewData>();
    payload.storage_id = storage_id;
    payload.slot_count = static_cast<std::uint16_t>(std::min<std::uint32_t>(slot_count, 65535U));
    payload.event_kind = event_kind;
    payload.flags = 0U;
    engine_messages().push_back(message);
    queue_ui_surface_visibility_message(
        GS1_UI_SURFACE_SITE_INVENTORY_PANEL,
        event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
}

void GamePresentationCoordinator::queue_all_site_inventory_slot_upsert_messages(Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    for (std::uint32_t index = 0; index < active_site_run()->inventory.worker_pack_slots.size(); ++index)
    {
        queue_site_inventory_slot_upsert_message(
            GS1_INVENTORY_CONTAINER_WORKER_PACK,
            index,
            active_site_run()->inventory.worker_pack_storage_id,
            0U,
            TileCoord {},
            message_type);
    }
}

void GamePresentationCoordinator::queue_pending_site_inventory_slot_upsert_messages(
    Gs1EngineMessageType storage_message_type,
    Gs1EngineMessageType slot_message_type,
    Gs1EngineMessageType view_state_message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto& site_run = active_site_run().value();
    if (site_run.pending_inventory_storage_descriptor_projection_update ||
        site_run.pending_full_inventory_projection_update)
    {
        queue_all_site_inventory_storage_upsert_messages(storage_message_type);
    }

    if (site_run.pending_full_inventory_projection_update ||
        site_run.pending_worker_pack_inventory_projection_updates.empty())
    {
        queue_all_site_inventory_slot_upsert_messages(slot_message_type);
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
                site_run.inventory.worker_pack_storage_id,
                0U,
                TileCoord {},
                slot_message_type);
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
                GS1_INVENTORY_VIEW_EVENT_CLOSE,
                0U,
                view_state_message_type);
        }
        else if (!previous_worker_pack_open && current_worker_pack_open)
        {
            queue_site_inventory_view_state_message(
                site_run.inventory.worker_pack_storage_id,
                GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
                static_cast<std::uint32_t>(site_run.inventory.worker_pack_slots.size()),
                view_state_message_type);
        }

        const auto previous_storage_id = site_run.inventory.last_projected_opened_device_storage_id;
        const auto current_storage_id = site_run.inventory.opened_device_storage_id;
        if (previous_storage_id != 0U && previous_storage_id != current_storage_id)
        {
            queue_site_inventory_view_state_message(
                previous_storage_id,
                GS1_INVENTORY_VIEW_EVENT_CLOSE,
                0U,
                view_state_message_type);
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
                    : static_cast<std::uint32_t>(storage_state->slot_item_instance_ids.size()),
                view_state_message_type);
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
                opened_storage->tile_coord,
                slot_message_type);
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
            opened_storage->tile_coord,
            slot_message_type);
    }
}

void GamePresentationCoordinator::queue_site_craft_context_messages()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto& presentation = active_site_run()->craft.context_presentation;
    auto begin = make_engine_message(GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN);
    auto& begin_payload = begin.emplace_payload<Gs1EngineMessageCraftContextData>();
    begin_payload.tile_x = presentation.tile_coord.x;
    begin_payload.tile_y = presentation.tile_coord.y;
    begin_payload.option_count = static_cast<std::uint16_t>(std::min<std::size_t>(
        presentation.options.size(),
        65535U));
    begin_payload.flags = presentation.occupied ? 1U : 0U;
    engine_messages().push_back(begin);

    for (const auto& option : presentation.options)
    {
        auto option_message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT);
        auto& option_payload =
            option_message.emplace_payload<Gs1EngineMessageCraftContextOptionData>();
        option_payload.recipe_id = option.recipe_id;
        option_payload.output_item_id = option.output_item_id;
        option_payload.flags = 1U;
        option_payload.reserved0 = 0U;
        engine_messages().push_back(option_message);
    }

    engine_messages().push_back(make_engine_message(GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END));
}

void GamePresentationCoordinator::queue_site_placement_preview_message()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto& placement_mode = active_site_run()->site_action.placement_mode;
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW);
    auto& payload = message.emplace_payload<Gs1EngineMessagePlacementPreviewData>();
    if (placement_mode.active && placement_mode.target_tile.has_value())
    {
        payload.tile_x = placement_mode.target_tile->x;
        payload.tile_y = placement_mode.target_tile->y;
        payload.blocked_mask = placement_mode.blocked_mask;
        payload.item_id = placement_mode.item_id;
        const auto preview_tiles = build_placement_preview_tile_projections(active_site_run().value());
        payload.preview_tile_count = static_cast<std::uint32_t>(std::min<std::size_t>(preview_tiles.size(), 1024U));
        payload.footprint_width = placement_mode.footprint_width;
        payload.footprint_height = placement_mode.footprint_height;
        payload.action_kind = static_cast<Gs1SiteActionKind>(placement_mode.action_kind);
        payload.flags = 1U;
        if (placement_mode.blocked_mask == 0ULL)
        {
            payload.flags |= 2U;
        }
        engine_messages().push_back(message);
        for (std::size_t index = 0; index < preview_tiles.size(); ++index)
        {
            Gs1EngineMessagePlacementPreviewTileData tile_payload {};
            tile_payload.x = static_cast<std::int16_t>(preview_tiles[index].coord.x);
            tile_payload.y = static_cast<std::int16_t>(preview_tiles[index].coord.y);
            tile_payload.flags = preview_tiles[index].flags;
            tile_payload.wind_protection = preview_tiles[index].wind_protection;
            tile_payload.heat_protection = preview_tiles[index].heat_protection;
            tile_payload.dust_protection = preview_tiles[index].dust_protection;
            tile_payload.final_wind_protection = preview_tiles[index].final_wind_protection;
            tile_payload.final_heat_protection = preview_tiles[index].final_heat_protection;
            tile_payload.final_dust_protection = preview_tiles[index].final_dust_protection;
            tile_payload.occupant_condition = preview_tiles[index].occupant_condition;
            queue_site_placement_preview_tile_upsert_message(tile_payload);
        }
        return;
    }
    else
    {
        payload.tile_x = 0;
        payload.tile_y = 0;
        payload.blocked_mask = 0ULL;
        payload.item_id = 0U;
        payload.preview_tile_count = 0U;
        payload.footprint_width = 1U;
        payload.footprint_height = 1U;
        payload.action_kind = GS1_SITE_ACTION_NONE;
        payload.flags = 4U;
    }

    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_placement_preview_tile_upsert_message(
    const Gs1EngineMessagePlacementPreviewTileData& payload)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT);
    message.emplace_payload<Gs1EngineMessagePlacementPreviewTileData>() = payload;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_placement_failure_message(const PlacementModeCommitRejectedMessage& payload)
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
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_task_upsert_message(
    std::size_t task_index,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto& tasks = active_site_run()->task_board.visible_tasks;
    if (task_index >= tasks.size())
    {
        return;
    }

    const auto& task = tasks[task_index];
    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessageTaskData>();
    payload.task_instance_id = task.task_instance_id.value;
    payload.task_template_id = task.task_template_id.value;
    payload.publisher_faction_id = task.publisher_faction_id.value;
    payload.current_progress = static_cast<std::uint16_t>(std::min<std::uint32_t>(task.current_progress_amount, 65535U));
    payload.target_progress = static_cast<std::uint16_t>(std::min<std::uint32_t>(task.target_amount, 65535U));
    payload.list_kind = to_task_presentation_list_kind(task.runtime_list_kind);
    payload.flags = 1U;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_all_site_task_upsert_messages(Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    for (std::size_t index = 0; index < active_site_run()->task_board.visible_tasks.size(); ++index)
    {
        queue_site_task_upsert_message(index, message_type);
    }
}

void GamePresentationCoordinator::queue_site_modifier_list_begin_message(
    Gs1ProjectionMode mode,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteModifierListData>();
    payload.mode = mode;
    payload.reserved0 = 0U;
    payload.modifier_count = static_cast<std::uint16_t>(std::min<std::size_t>(
        active_site_run()->modifier.active_site_modifiers.size(),
        std::numeric_limits<std::uint16_t>::max()));
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_modifier_upsert_message(
    std::size_t modifier_index,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value() ||
        modifier_index >= active_site_run()->modifier.active_site_modifiers.size())
    {
        return;
    }

    const auto& active_modifier =
        active_site_run()->modifier.active_site_modifiers[modifier_index];
    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteModifierData>();
    payload.modifier_id = active_modifier.modifier_id.value;
    payload.remaining_game_hours =
        projected_remaining_game_hours(active_modifier.remaining_world_minutes);
    payload.flags =
        active_modifier.duration_world_minutes > 0.0 ? GS1_SITE_MODIFIER_FLAG_TIMED : 0U;
    payload.reserved0 = 0U;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_all_site_modifier_upsert_messages(
    Gs1ProjectionMode mode,
    Gs1EngineMessageType begin_message_type,
    Gs1EngineMessageType upsert_message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    queue_site_modifier_list_begin_message(mode, begin_message_type);
    for (std::size_t index = 0U; index < active_site_run()->modifier.active_site_modifiers.size(); ++index)
    {
        queue_site_modifier_upsert_message(index, upsert_message_type);
    }
}

void GamePresentationCoordinator::queue_site_phone_listing_upsert_message(
    std::size_t listing_index,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto& listings = active_site_run()->phone_panel.listings;
    if (listing_index >= listings.size())
    {
        return;
    }

    const auto& listing = listings[listing_index];
    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessagePhoneListingData>();
    payload.listing_id = listing.listing_id;
    payload.item_or_unlockable_id = listing.item_id.value;
    payload.price = cash_value_from_cash_points(listing.price);
    payload.related_site_id = active_site_run()->site_id.value;
    payload.quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(listing.quantity, 65535U));
    payload.cart_quantity = static_cast<std::uint16_t>(std::min<std::uint32_t>(listing.cart_quantity, 65535U));
    payload.listing_kind = to_phone_listing_presentation_kind(listing.kind);
    payload.flags = listing.occupied ? 1U : 0U;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_phone_panel_state_message()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto& phone_panel = active_site_run()->phone_panel;
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
    payload.flags = phone_panel.badge_flags |
        (phone_panel.open ? GS1_PHONE_PANEL_FLAG_OPEN : 0U);
    engine_messages().push_back(message);
    queue_ui_surface_visibility_message(
        GS1_UI_SURFACE_SITE_PHONE_PANEL,
        phone_panel.open);
}

void GamePresentationCoordinator::queue_site_protection_overlay_state_message()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteProtectionOverlayData>();
    payload.mode = site_protection_overlay_mode();
    payload.reserved0[0] = 0U;
    payload.reserved0[1] = 0U;
    payload.reserved0[2] = 0U;
    engine_messages().push_back(message);
    queue_ui_surface_visibility_message(
        GS1_UI_SURFACE_SITE_OVERLAY_PANEL,
        site_protection_overlay_mode() != GS1_SITE_PROTECTION_OVERLAY_NONE);
}

void GamePresentationCoordinator::queue_site_phone_listing_remove_message(
    std::uint32_t listing_id,
    Gs1EngineMessageType message_type)
{
    if (!active_site_run().has_value() || listing_id == 0U)
    {
        return;
    }

    auto message = make_engine_message(message_type);
    auto& payload = message.emplace_payload<Gs1EngineMessagePhoneListingData>();
    payload.listing_id = listing_id;
    payload.item_or_unlockable_id = 0U;
    payload.price = 0;
    payload.related_site_id = active_site_run()->site_id.value;
    payload.quantity = 0U;
    payload.cart_quantity = 0U;
    payload.listing_kind = GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM;
    payload.flags = 0U;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_all_site_phone_listing_upsert_messages(
    Gs1EngineMessageType upsert_message_type,
    Gs1EngineMessageType remove_message_type)
{
    if (!active_site_run().has_value())
    {
        return;
    }

    std::vector<std::uint32_t> current_listing_ids {};
    current_listing_ids.reserve(active_site_run()->phone_panel.listings.size());
    for (const auto& listing : active_site_run()->phone_panel.listings)
    {
        current_listing_ids.push_back(listing.listing_id);
    }

    for (const auto previous_listing_id : last_emitted_phone_listing_ids_)
    {
        if (std::find(current_listing_ids.begin(), current_listing_ids.end(), previous_listing_id) ==
            current_listing_ids.end())
        {
            queue_site_phone_listing_remove_message(previous_listing_id, remove_message_type);
        }
    }

    for (std::size_t index = 0; index < active_site_run()->phone_panel.listings.size(); ++index)
    {
        queue_site_phone_listing_upsert_message(index, upsert_message_type);
    }

    last_emitted_phone_listing_ids_ = std::move(current_listing_ids);
}

void GamePresentationCoordinator::queue_site_bootstrap_messages()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    queue_site_snapshot_begin_message(GS1_PROJECTION_MODE_SNAPSHOT);
    queue_all_site_tile_upsert_messages();
    queue_site_worker_update_message();
    queue_site_camp_update_message();
    queue_site_weather_update_message();
    queue_all_site_plant_visual_messages();
    queue_all_site_device_visual_messages();
    queue_all_site_inventory_storage_upsert_messages();
    queue_all_site_inventory_slot_upsert_messages();
    queue_all_site_task_upsert_messages();
    queue_all_site_modifier_upsert_messages(GS1_PROJECTION_MODE_SNAPSHOT);
    queue_site_phone_panel_state_message();
    queue_all_site_phone_listing_upsert_messages();
    queue_site_protection_overlay_state_message();
    queue_site_snapshot_end_message();

    queue_site_snapshot_begin_message(
        GS1_PROJECTION_MODE_SNAPSHOT,
        GS1_ENGINE_MESSAGE_BEGIN_SITE_SCENE_SNAPSHOT);
    queue_all_site_tile_upsert_messages(GS1_ENGINE_MESSAGE_SITE_SCENE_TILE_UPSERT);
    queue_site_worker_update_message(GS1_ENGINE_MESSAGE_SITE_SCENE_WORKER_UPDATE);
    queue_all_site_plant_visual_messages(GS1_ENGINE_MESSAGE_SITE_SCENE_PLANT_VISUAL_UPSERT);
    queue_all_site_device_visual_messages(GS1_ENGINE_MESSAGE_SITE_SCENE_DEVICE_VISUAL_UPSERT);
    queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_SCENE_SNAPSHOT);

    queue_site_snapshot_begin_message(
        GS1_PROJECTION_MODE_SNAPSHOT,
        GS1_ENGINE_MESSAGE_BEGIN_SITE_INVENTORY_PANEL_SNAPSHOT);
    queue_all_site_inventory_storage_upsert_messages(GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_STORAGE_UPSERT);
    queue_all_site_inventory_slot_upsert_messages(GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_SLOT_UPSERT);
    if (active_site_run()->inventory.worker_pack_panel_open)
    {
        queue_site_inventory_view_state_message(
            active_site_run()->inventory.worker_pack_storage_id,
            GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
            static_cast<std::uint32_t>(active_site_run()->inventory.worker_pack_slots.size()),
            GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_VIEW_STATE);
    }
    if (active_site_run()->inventory.opened_device_storage_id != 0U)
    {
        const auto* opened_storage =
            inventory_storage::storage_container_state_for_storage_id(
                active_site_run().value(),
                active_site_run()->inventory.opened_device_storage_id);
        queue_site_inventory_view_state_message(
            active_site_run()->inventory.opened_device_storage_id,
            GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
            opened_storage == nullptr
                ? 0U
                : static_cast<std::uint32_t>(opened_storage->slot_item_instance_ids.size()),
            GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_VIEW_STATE);
    }
    queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_INVENTORY_PANEL_SNAPSHOT);

    queue_site_snapshot_begin_message(
        GS1_PROJECTION_MODE_SNAPSHOT,
        GS1_ENGINE_MESSAGE_BEGIN_SITE_PHONE_PANEL_SNAPSHOT);
    queue_all_site_phone_listing_upsert_messages(
        GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_LISTING_UPSERT,
        GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_LISTING_REMOVE);
    queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_PHONE_PANEL_SNAPSHOT);

    queue_site_snapshot_begin_message(
        GS1_PROJECTION_MODE_SNAPSHOT,
        GS1_ENGINE_MESSAGE_BEGIN_SITE_TASK_PANEL_SNAPSHOT);
    queue_all_site_task_upsert_messages(GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_UPSERT);
    queue_all_site_modifier_upsert_messages(
        GS1_PROJECTION_MODE_SNAPSHOT,
        GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_LIST_BEGIN,
        GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_UPSERT);
    queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_TASK_PANEL_SNAPSHOT);

    queue_site_snapshot_begin_message(
        GS1_PROJECTION_MODE_SNAPSHOT,
        GS1_ENGINE_MESSAGE_BEGIN_SITE_SUMMARY_SNAPSHOT);
    queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_SUMMARY_SNAPSHOT);

    queue_site_snapshot_begin_message(
        GS1_PROJECTION_MODE_SNAPSHOT,
        GS1_ENGINE_MESSAGE_BEGIN_SITE_HUD_SNAPSHOT);
    queue_all_site_inventory_storage_upsert_messages(GS1_ENGINE_MESSAGE_SITE_HUD_STORAGE_UPSERT);
    queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_HUD_SNAPSHOT);

    active_site_run()->pending_projection_update_flags = 0U;
    clear_pending_site_tile_projection_updates();
    clear_pending_site_inventory_projection_updates();
}

void GamePresentationCoordinator::queue_site_ready_bootstrap_messages()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    queue_site_bootstrap_messages();
    queue_campaign_resources_message();
    queue_site_action_update_message();
    queue_hud_state_message();
    queue_log_message("Started site attempt.");
}

void GamePresentationCoordinator::queue_site_delta_messages(std::uint64_t dirty_flags)
{
    if (!active_site_run().has_value())
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
        queue_pending_site_plant_visual_messages();
        queue_pending_site_device_visual_messages();
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

    if ((site_dirty_flags & (SITE_PROJECTION_UPDATE_TILES | SITE_PROJECTION_UPDATE_WORKER)) != 0U)
    {
        queue_site_snapshot_begin_message(
            GS1_PROJECTION_MODE_DELTA,
            GS1_ENGINE_MESSAGE_BEGIN_SITE_SCENE_SNAPSHOT);
        if ((site_dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
        {
            queue_pending_site_plant_visual_messages(
                GS1_ENGINE_MESSAGE_SITE_SCENE_PLANT_VISUAL_UPSERT,
                GS1_ENGINE_MESSAGE_SITE_SCENE_PLANT_VISUAL_REMOVE);
            queue_pending_site_device_visual_messages(
                GS1_ENGINE_MESSAGE_SITE_SCENE_DEVICE_VISUAL_UPSERT,
                GS1_ENGINE_MESSAGE_SITE_SCENE_DEVICE_VISUAL_REMOVE);
            queue_pending_site_tile_upsert_messages(GS1_ENGINE_MESSAGE_SITE_SCENE_TILE_UPSERT);
        }
        if ((site_dirty_flags & SITE_PROJECTION_UPDATE_WORKER) != 0U)
        {
            queue_site_worker_update_message(GS1_ENGINE_MESSAGE_SITE_SCENE_WORKER_UPDATE);
        }
        queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_SCENE_SNAPSHOT);
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
    {
        queue_site_snapshot_begin_message(
            GS1_PROJECTION_MODE_DELTA,
            GS1_ENGINE_MESSAGE_BEGIN_SITE_INVENTORY_PANEL_SNAPSHOT);
        queue_pending_site_inventory_slot_upsert_messages(
            GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_STORAGE_UPSERT,
            GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_SLOT_UPSERT,
            GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_VIEW_STATE);
        queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_INVENTORY_PANEL_SNAPSHOT);

        queue_site_snapshot_begin_message(
            GS1_PROJECTION_MODE_DELTA,
            GS1_ENGINE_MESSAGE_BEGIN_SITE_HUD_SNAPSHOT);
        queue_all_site_inventory_storage_upsert_messages(GS1_ENGINE_MESSAGE_SITE_HUD_STORAGE_UPSERT);
        queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_HUD_SNAPSHOT);
    }

    if ((site_dirty_flags & SITE_PROJECTION_UPDATE_PHONE) != 0U)
    {
        queue_site_snapshot_begin_message(
            GS1_PROJECTION_MODE_DELTA,
            GS1_ENGINE_MESSAGE_BEGIN_SITE_PHONE_PANEL_SNAPSHOT);
        queue_all_site_phone_listing_upsert_messages(
            GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_LISTING_UPSERT,
            GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_LISTING_REMOVE);
        queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_PHONE_PANEL_SNAPSHOT);
    }

    if ((site_dirty_flags & (SITE_PROJECTION_UPDATE_TASKS | SITE_PROJECTION_UPDATE_MODIFIERS)) != 0U)
    {
        queue_site_snapshot_begin_message(
            GS1_PROJECTION_MODE_DELTA,
            GS1_ENGINE_MESSAGE_BEGIN_SITE_TASK_PANEL_SNAPSHOT);
        if ((site_dirty_flags & SITE_PROJECTION_UPDATE_TASKS) != 0U)
        {
            queue_all_site_task_upsert_messages(GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_UPSERT);
        }
        if ((site_dirty_flags & SITE_PROJECTION_UPDATE_MODIFIERS) != 0U)
        {
            queue_all_site_modifier_upsert_messages(
                GS1_PROJECTION_MODE_DELTA,
                GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_LIST_BEGIN,
                GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_UPSERT);
        }
        queue_site_snapshot_end_message(GS1_ENGINE_MESSAGE_END_SITE_TASK_PANEL_SNAPSHOT);
    }
}

void GamePresentationCoordinator::queue_site_action_update_message()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto& action_state = active_site_run()->site_action;
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

    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_hud_state_message()
{
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto worker_conditions = site_world_access::worker_conditions(active_site_run().value());

    auto hud_message = make_engine_message(GS1_ENGINE_MESSAGE_HUD_STATE);
    auto& payload = hud_message.emplace_payload<Gs1EngineMessageHudStateData>();
    payload.player_health = worker_conditions.health;
    payload.player_hydration = worker_conditions.hydration;
    payload.player_nourishment = worker_conditions.nourishment;
    payload.player_energy = worker_conditions.energy;
    payload.player_morale = worker_conditions.morale;
    payload.current_money = cash_value_from_cash_points(active_site_run()->economy.current_cash);
    payload.active_task_count =
        static_cast<std::uint16_t>(active_site_run()->task_board.accepted_task_ids.size());
    payload.current_action_kind =
        static_cast<Gs1SiteActionKind>(active_site_run()->site_action.action_kind);
    payload.site_completion_normalized = active_site_run()->counters.objective_progress_normalized;
    payload.warning_code = resolve_hud_warning_code(active_site_run().value());
    engine_messages().push_back(hud_message);
}

void GamePresentationCoordinator::queue_campaign_resources_message()
{
    if (!campaign().has_value())
    {
        return;
    }

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES);
    auto& payload = message.emplace_payload<Gs1EngineMessageCampaignResourcesData>();
    payload.current_money = active_site_run().has_value()
        ? cash_value_from_cash_points(active_site_run()->economy.current_cash)
        : 0.0f;
    payload.total_reputation = campaign()->technology_state.total_reputation;
    payload.village_reputation = TechnologySystem::faction_reputation(
        *campaign(),
        FactionId {k_faction_village_committee});
    payload.forestry_reputation = TechnologySystem::faction_reputation(
        *campaign(),
        FactionId {k_faction_forestry_bureau});
    payload.university_reputation = TechnologySystem::faction_reputation(
        *campaign(),
        FactionId {k_faction_agricultural_university});
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_site_result_ready_message(
    std::uint32_t site_id,
    Gs1SiteAttemptResult result,
    std::uint32_t newly_revealed_site_count)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_RESULT_READY);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteResultData>();
    payload.site_id = site_id;
    payload.result = result;
    payload.newly_revealed_site_count = static_cast<std::uint16_t>(newly_revealed_site_count);
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_task_reward_claimed_cue_message(
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
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_craft_output_stored_cue_message(
    std::uint32_t storage_id,
    std::uint32_t item_id,
    std::uint32_t quantity)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE);
    auto& payload = message.emplace_payload<Gs1EngineMessageOneShotCueData>();
    payload.subject_id = storage_id;
    payload.world_x = 0.0f;
    payload.world_y = 0.0f;
    payload.arg0 = item_id;
    payload.arg1 = quantity;
    payload.cue_kind = GS1_ONE_SHOT_CUE_CRAFT_OUTPUT_STORED;
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::queue_campaign_unlock_cue_message(
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
    engine_messages().push_back(message);
}

void GamePresentationCoordinator::sync_campaign_unlock_presentations()
{
    if (!campaign().has_value())
    {
        last_campaign_unlock_snapshot_ = {};
        return;
    }

    auto unlocked_reputation_unlock_ids = capture_unlocked_reputation_unlock_ids(*campaign());
    auto unlocked_technology_node_ids = capture_unlocked_technology_node_ids(*campaign());
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

void GamePresentationCoordinator::activate_loaded_site_scene(GamePresentationRuntimeContext& context)
{
    active_context_ = &context;
    if (!active_site_run().has_value() || app_state() != GS1_APP_STATE_SITE_LOADING)
    {
        return;
    }

    app_state() = GS1_APP_STATE_SITE_ACTIVE;
    if (campaign().has_value())
    {
        campaign()->app_state = app_state();
    }

    queue_app_state_message(app_state());
    queue_site_ready_bootstrap_messages();
}


void GamePresentationCoordinator::mark_site_projection_update_dirty(GamePresentationRuntimeContext& context, std::uint64_t dirty_flags) noexcept
{
    active_context_ = &context;
    if (!active_site_run().has_value())
    {
        return;
    }

    if ((dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
    {
        active_site_run()->pending_full_tile_projection_update = true;
    }
    if ((dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
    {
        active_site_run()->pending_full_inventory_projection_update = true;
        active_site_run()->pending_inventory_storage_descriptor_projection_update = true;
        if (active_site_run()->inventory.opened_device_storage_id != 0U)
        {
            active_site_run()->pending_opened_inventory_storage_full_projection_update = true;
        }
    }

    active_site_run()->pending_projection_update_flags |= dirty_flags;
}


void GamePresentationCoordinator::mark_site_tile_projection_dirty(GamePresentationRuntimeContext& context, TileCoord coord) noexcept
{
    active_context_ = &context;
    if (!active_site_run().has_value())
    {
        return;
    }

    auto& site_run = active_site_run().value();
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


void GamePresentationCoordinator::clear_pending_site_tile_projection_updates() noexcept
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto& site_run = active_site_run().value();
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

void GamePresentationCoordinator::clear_pending_site_inventory_projection_updates() noexcept
{
    if (!active_site_run().has_value())
    {
        return;
    }

    auto& site_run = active_site_run().value();

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


void GamePresentationCoordinator::flush_site_presentation_if_dirty(GamePresentationRuntimeContext& context)
{
    active_context_ = &context;
    if (!active_site_run().has_value())
    {
        return;
    }

    const auto dirty_flags = active_site_run()->pending_projection_update_flags;
    if (dirty_flags == 0U)
    {
        return;
    }

    if ((dirty_flags & (SITE_PROJECTION_UPDATE_PHONE |
            SITE_PROJECTION_UPDATE_TASKS |
            SITE_PROJECTION_UPDATE_INVENTORY)) != 0U)
    {
        RuntimeInvocation invocation {
            app_state(),
            campaign(),
            active_site_run(),
            engine_messages(),
            message_queue(),
            fixed_step_seconds()};
        PhonePanelSystem system {};
        system.run(invocation);
    }

    const auto projected_dirty_flags = active_site_run()->pending_projection_update_flags;
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

    active_site_run()->pending_projection_update_flags = 0U;
    clear_pending_site_tile_projection_updates();
    clear_pending_site_inventory_projection_updates();
}

}  // namespace gs1

