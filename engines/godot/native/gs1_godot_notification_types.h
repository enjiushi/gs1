#pragma once

#include "../../../include/gs1/types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>

enum Gs1GodotNotificationType : std::uint8_t
{
    GS1_GODOT_NOTIFICATION_NONE = 0,
    GS1_GODOT_NOTIFICATION_LOG_TEXT = 1,
    GS1_GODOT_NOTIFICATION_SET_APP_STATE = 2,
    GS1_GODOT_NOTIFICATION_PRESENTATION_DIRTY = 3,

    GS1_GODOT_NOTIFICATION_BEGIN_REGIONAL_MAP_SNAPSHOT = 10,
    GS1_GODOT_NOTIFICATION_REGIONAL_MAP_SITE_UPSERT = 11,
    GS1_GODOT_NOTIFICATION_REGIONAL_MAP_SITE_REMOVE = 12,
    GS1_GODOT_NOTIFICATION_REGIONAL_MAP_LINK_UPSERT = 13,
    GS1_GODOT_NOTIFICATION_REGIONAL_MAP_LINK_REMOVE = 14,
    GS1_GODOT_NOTIFICATION_END_REGIONAL_MAP_SNAPSHOT = 15,

    GS1_GODOT_NOTIFICATION_BEGIN_UI_SETUP = 16,
    GS1_GODOT_NOTIFICATION_UI_ELEMENT_UPSERT = 17,
    GS1_GODOT_NOTIFICATION_END_UI_SETUP = 18,
    GS1_GODOT_NOTIFICATION_CLOSE_UI_SETUP = 19,

    GS1_GODOT_NOTIFICATION_BEGIN_SITE_SNAPSHOT = 20,
    GS1_GODOT_NOTIFICATION_SITE_TILE_UPSERT = 21,
    GS1_GODOT_NOTIFICATION_SITE_WORKER_UPDATE = 22,
    GS1_GODOT_NOTIFICATION_SITE_CAMP_UPDATE = 23,
    GS1_GODOT_NOTIFICATION_SITE_WEATHER_UPDATE = 24,
    GS1_GODOT_NOTIFICATION_SITE_INVENTORY_SLOT_UPSERT = 25,
    GS1_GODOT_NOTIFICATION_SITE_TASK_UPSERT = 26,
    GS1_GODOT_NOTIFICATION_SITE_TASK_REMOVE = 27,
    GS1_GODOT_NOTIFICATION_SITE_PHONE_LISTING_UPSERT = 28,
    GS1_GODOT_NOTIFICATION_SITE_PHONE_LISTING_REMOVE = 29,
    GS1_GODOT_NOTIFICATION_END_SITE_SNAPSHOT = 30,
    GS1_GODOT_NOTIFICATION_SITE_ACTION_UPDATE = 31,
    GS1_GODOT_NOTIFICATION_SITE_INVENTORY_STORAGE_UPSERT = 32,
    GS1_GODOT_NOTIFICATION_SITE_INVENTORY_VIEW_STATE = 33,
    GS1_GODOT_NOTIFICATION_SITE_CRAFT_CONTEXT_BEGIN = 34,
    GS1_GODOT_NOTIFICATION_SITE_CRAFT_CONTEXT_OPTION_UPSERT = 35,
    GS1_GODOT_NOTIFICATION_SITE_CRAFT_CONTEXT_END = 36,
    GS1_GODOT_NOTIFICATION_SITE_PLACEMENT_PREVIEW = 37,
    GS1_GODOT_NOTIFICATION_SITE_PLACEMENT_PREVIEW_TILE_UPSERT = 38,
    GS1_GODOT_NOTIFICATION_SITE_PLACEMENT_FAILURE = 39,
    GS1_GODOT_NOTIFICATION_SITE_PHONE_PANEL_STATE = 40,
    GS1_GODOT_NOTIFICATION_SITE_PROTECTION_OVERLAY_STATE = 41,
    GS1_GODOT_NOTIFICATION_SITE_MODIFIER_LIST_BEGIN = 42,
    GS1_GODOT_NOTIFICATION_SITE_MODIFIER_UPSERT = 43,
    GS1_GODOT_NOTIFICATION_HUD_STATE = 44,
    GS1_GODOT_NOTIFICATION_PUSH = 45,
    GS1_GODOT_NOTIFICATION_PLAY_ONE_SHOT_CUE = 47,
    GS1_GODOT_NOTIFICATION_CAMPAIGN_RESOURCES = 48,
    GS1_GODOT_NOTIFICATION_SITE_PLANT_VISUAL_UPSERT = 49,
    GS1_GODOT_NOTIFICATION_SITE_PLANT_VISUAL_REMOVE = 50,
    GS1_GODOT_NOTIFICATION_SITE_DEVICE_VISUAL_UPSERT = 51,
    GS1_GODOT_NOTIFICATION_SITE_DEVICE_VISUAL_REMOVE = 52,

    GS1_GODOT_NOTIFICATION_BEGIN_PROGRESSION_VIEW = 60,
    GS1_GODOT_NOTIFICATION_PROGRESSION_ENTRY_UPSERT = 61,
    GS1_GODOT_NOTIFICATION_END_PROGRESSION_VIEW = 62,
    GS1_GODOT_NOTIFICATION_CLOSE_PROGRESSION_VIEW = 63,

    GS1_GODOT_NOTIFICATION_BEGIN_REGIONAL_MAP_HUD_SNAPSHOT = 65,
    GS1_GODOT_NOTIFICATION_REGIONAL_MAP_HUD_SITE_UPSERT = 66,
    GS1_GODOT_NOTIFICATION_REGIONAL_MAP_HUD_SITE_REMOVE = 67,
    GS1_GODOT_NOTIFICATION_REGIONAL_MAP_HUD_LINK_UPSERT = 68,
    GS1_GODOT_NOTIFICATION_REGIONAL_MAP_HUD_LINK_REMOVE = 69,
    GS1_GODOT_NOTIFICATION_END_REGIONAL_MAP_HUD_SNAPSHOT = 70,

    GS1_GODOT_NOTIFICATION_BEGIN_REGIONAL_SUMMARY_SNAPSHOT = 71,
    GS1_GODOT_NOTIFICATION_REGIONAL_SUMMARY_SITE_UPSERT = 72,
    GS1_GODOT_NOTIFICATION_REGIONAL_SUMMARY_SITE_REMOVE = 73,
    GS1_GODOT_NOTIFICATION_REGIONAL_SUMMARY_LINK_UPSERT = 74,
    GS1_GODOT_NOTIFICATION_REGIONAL_SUMMARY_LINK_REMOVE = 75,
    GS1_GODOT_NOTIFICATION_END_REGIONAL_SUMMARY_SNAPSHOT = 76,

    GS1_GODOT_NOTIFICATION_BEGIN_REGIONAL_SELECTION_SNAPSHOT = 77,
    GS1_GODOT_NOTIFICATION_REGIONAL_SELECTION_SITE_UPSERT = 78,
    GS1_GODOT_NOTIFICATION_REGIONAL_SELECTION_SITE_REMOVE = 79,
    GS1_GODOT_NOTIFICATION_REGIONAL_SELECTION_LINK_UPSERT = 80,
    GS1_GODOT_NOTIFICATION_REGIONAL_SELECTION_LINK_REMOVE = 81,
    GS1_GODOT_NOTIFICATION_END_REGIONAL_SELECTION_SNAPSHOT = 82,

    GS1_GODOT_NOTIFICATION_BEGIN_SITE_SCENE_SNAPSHOT = 83,
    GS1_GODOT_NOTIFICATION_SITE_SCENE_TILE_UPSERT = 84,
    GS1_GODOT_NOTIFICATION_SITE_SCENE_WORKER_UPDATE = 85,
    GS1_GODOT_NOTIFICATION_SITE_SCENE_PLANT_VISUAL_UPSERT = 86,
    GS1_GODOT_NOTIFICATION_SITE_SCENE_PLANT_VISUAL_REMOVE = 87,
    GS1_GODOT_NOTIFICATION_SITE_SCENE_DEVICE_VISUAL_UPSERT = 88,
    GS1_GODOT_NOTIFICATION_SITE_SCENE_DEVICE_VISUAL_REMOVE = 89,
    GS1_GODOT_NOTIFICATION_END_SITE_SCENE_SNAPSHOT = 90,

    GS1_GODOT_NOTIFICATION_BEGIN_SITE_INVENTORY_PANEL_SNAPSHOT = 91,
    GS1_GODOT_NOTIFICATION_SITE_INVENTORY_PANEL_STORAGE_UPSERT = 92,
    GS1_GODOT_NOTIFICATION_SITE_INVENTORY_PANEL_SLOT_UPSERT = 93,
    GS1_GODOT_NOTIFICATION_SITE_INVENTORY_PANEL_VIEW_STATE = 94,
    GS1_GODOT_NOTIFICATION_END_SITE_INVENTORY_PANEL_SNAPSHOT = 95,

    GS1_GODOT_NOTIFICATION_BEGIN_SITE_PHONE_PANEL_SNAPSHOT = 96,
    GS1_GODOT_NOTIFICATION_SITE_PHONE_PANEL_LISTING_UPSERT = 97,
    GS1_GODOT_NOTIFICATION_SITE_PHONE_PANEL_LISTING_REMOVE = 98,
    GS1_GODOT_NOTIFICATION_END_SITE_PHONE_PANEL_SNAPSHOT = 99,

    GS1_GODOT_NOTIFICATION_BEGIN_SITE_TASK_PANEL_SNAPSHOT = 100,
    GS1_GODOT_NOTIFICATION_SITE_TASK_PANEL_TASK_UPSERT = 101,
    GS1_GODOT_NOTIFICATION_SITE_TASK_PANEL_TASK_REMOVE = 102,
    GS1_GODOT_NOTIFICATION_SITE_TASK_PANEL_MODIFIER_LIST_BEGIN = 103,
    GS1_GODOT_NOTIFICATION_SITE_TASK_PANEL_MODIFIER_UPSERT = 104,
    GS1_GODOT_NOTIFICATION_END_SITE_TASK_PANEL_SNAPSHOT = 105,

    GS1_GODOT_NOTIFICATION_BEGIN_SITE_SUMMARY_SNAPSHOT = 106,
    GS1_GODOT_NOTIFICATION_END_SITE_SUMMARY_SNAPSHOT = 107,

    GS1_GODOT_NOTIFICATION_BEGIN_SITE_HUD_SNAPSHOT = 108,
    GS1_GODOT_NOTIFICATION_SITE_HUD_STORAGE_UPSERT = 109,
    GS1_GODOT_NOTIFICATION_END_SITE_HUD_SNAPSHOT = 110
};

struct Gs1GodotAppStateNotification
{
    Gs1AppState app_state;
};

struct Gs1GodotPresentationDirtyNotification
{
    std::uint32_t dirty_flags;
    std::uint32_t reserved0;
    std::uint64_t revision;
};

struct Gs1GodotRegionalMapSiteProjection
{
    std::uint32_t site_id;
    std::uint32_t site_archetype_id;
    std::int32_t map_x;
    std::int32_t map_y;
    std::uint32_t support_package_id;
    std::uint16_t support_preview_mask;
    Gs1SiteState site_state;
    std::uint8_t flags;
};

struct Gs1GodotRegionalMapLinkProjection
{
    std::uint32_t from_site_id;
    std::uint32_t to_site_id;
    std::uint8_t flags;
};

struct Gs1GodotSiteTileProjection
{
    std::uint16_t x;
    std::uint16_t y;
    std::uint32_t terrain_type_id;
    std::uint32_t plant_type_id;
    std::uint32_t structure_type_id;
    std::uint32_t ground_cover_type_id;
    float plant_density;
    float sand_burial;
    float local_wind;
    float wind_protection;
    float heat_protection;
    float dust_protection;
    float moisture;
    float soil_fertility;
    float soil_salinity;
    std::uint16_t device_integrity_quantized;
    std::uint8_t excavation_depth;
    std::uint8_t visible_excavation_depth;
};

struct Gs1GodotWorkerProjection
{
    std::uint64_t entity_id;
    float tile_x;
    float tile_y;
    float facing_degrees;
    float health_normalized;
    float hydration_normalized;
    float energy_normalized;
    std::uint8_t flags;
    Gs1SiteActionKind current_action_kind;
    std::uint16_t reserved0;
};

struct Gs1GodotCampProjection
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    float durability_normalized;
    std::uint8_t flags;
};

struct Gs1GodotWeatherProjection
{
    float heat;
    float wind;
    float dust;
    float wind_direction_degrees;
    std::uint32_t event_template_id;
    float event_start_time_minutes;
    float event_peak_time_minutes;
    float event_peak_duration_minutes;
    float event_end_time_minutes;
};

struct Gs1GodotInventoryStorageProjection
{
    std::uint32_t storage_id;
    std::uint32_t owner_entity_id;
    std::uint16_t slot_count;
    std::int16_t tile_x;
    std::int16_t tile_y;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t flags;
};

struct Gs1GodotInventorySlotProjection
{
    std::uint32_t item_id;
    std::uint32_t item_instance_id;
    float condition;
    float freshness;
    std::uint32_t storage_id;
    std::uint32_t container_owner_id;
    std::uint16_t quantity;
    std::uint16_t slot_index;
    std::int16_t container_tile_x;
    std::int16_t container_tile_y;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t flags;
};

struct Gs1GodotCraftContextOptionProjection
{
    std::uint32_t recipe_id;
    std::uint32_t output_item_id;
    std::uint16_t flags;
    std::uint16_t reserved0;
};

struct Gs1GodotPlacementPreviewProjection
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint64_t blocked_mask;
    std::uint32_t item_id;
    std::uint32_t preview_tile_count;
    std::uint8_t footprint_width;
    std::uint8_t footprint_height;
    Gs1SiteActionKind action_kind;
    std::uint8_t flags;
};

struct Gs1GodotPlacementPreviewTileProjection
{
    std::int16_t x;
    std::int16_t y;
    std::uint8_t flags;
    std::uint8_t reserved0[3];
    float wind_protection;
    float heat_protection;
    float dust_protection;
    float final_wind_protection;
    float final_heat_protection;
    float final_dust_protection;
    float occupant_condition;
};

struct Gs1GodotPlacementFailureNotification
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint64_t blocked_mask;
    std::uint32_t sequence_id;
    Gs1SiteActionKind action_kind;
    std::uint8_t flags;
    std::uint16_t reserved0;
};

struct Gs1GodotTaskProjection
{
    std::uint32_t task_instance_id;
    std::uint32_t task_template_id;
    std::uint32_t publisher_faction_id;
    std::uint16_t current_progress;
    std::uint16_t target_progress;
    Gs1TaskPresentationListKind list_kind;
    std::uint8_t flags;
};

struct Gs1GodotPhoneListingProjection
{
    std::uint32_t listing_id;
    std::uint32_t item_or_unlockable_id;
    float price;
    std::uint32_t related_site_id;
    std::uint16_t quantity;
    std::uint16_t cart_quantity;
    Gs1PhoneListingPresentationKind listing_kind;
    std::uint8_t flags;
};

struct Gs1GodotSiteModifierProjection
{
    std::uint32_t modifier_id;
    std::uint16_t remaining_game_hours;
    std::uint8_t flags;
    std::uint8_t reserved0;
};

struct Gs1GodotSitePlantVisualProjection
{
    std::uint64_t visual_id;
    std::uint32_t plant_type_id;
    float anchor_tile_x;
    float anchor_tile_y;
    float height_scale;
    std::uint16_t density_quantized;
    std::uint8_t footprint_width;
    std::uint8_t footprint_height;
    std::uint8_t height_class;
    std::uint8_t focus;
    std::uint8_t flags;
    std::uint8_t reserved0;
};

struct Gs1GodotSiteDeviceVisualProjection
{
    std::uint64_t visual_id;
    std::uint32_t structure_type_id;
    float anchor_tile_x;
    float anchor_tile_y;
    float integrity_normalized;
    float height_scale;
    std::uint8_t footprint_width;
    std::uint8_t footprint_height;
    std::uint8_t flags;
    std::uint8_t reserved0;
};

struct alignas(GS1_MESSAGE_CACHE_LINE_SIZE) Gs1GodotNotification final
{
    unsigned char payload[GS1_MESSAGE_PAYLOAD_BYTE_COUNT];
    Gs1GodotNotificationType type {GS1_GODOT_NOTIFICATION_NONE};

    template <typename PayloadData>
    [[nodiscard]] PayloadData& emplace_payload() noexcept
    {
        static_assert(std::is_trivial_v<PayloadData>);
        static_assert(std::is_standard_layout_v<PayloadData>);
        static_assert(sizeof(PayloadData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
        auto* ptr = std::construct_at(reinterpret_cast<PayloadData*>(payload), PayloadData {});
        return *std::launder(ptr);
    }

    template <typename PayloadData>
    [[nodiscard]] const PayloadData& payload_as() const noexcept
    {
        static_assert(std::is_trivial_v<PayloadData>);
        static_assert(std::is_standard_layout_v<PayloadData>);
        static_assert(sizeof(PayloadData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
        return *std::launder(reinterpret_cast<const PayloadData*>(payload));
    }
};

static_assert(sizeof(Gs1GodotNotification) == GS1_MESSAGE_CACHE_LINE_SIZE);
static_assert(alignof(Gs1GodotNotification) == GS1_MESSAGE_CACHE_LINE_SIZE);
static_assert(offsetof(Gs1GodotNotification, payload) == 0U);
static_assert(offsetof(Gs1GodotNotification, type) == GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
