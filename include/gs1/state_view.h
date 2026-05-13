#pragma once

#include "gs1/types.h"

#include <cstdint>
#include <type_traits>

struct Gs1FactionProgressView
{
    std::uint32_t faction_id;
    std::int32_t faction_reputation;
    std::uint32_t unlocked_assistant_package_id;
    std::uint8_t has_unlocked_assistant_package;
    std::uint8_t onboarding_completed;
    std::uint8_t tutorial_completed;
    std::uint8_t reserved0;
};

struct Gs1LoadoutSlotView
{
    std::uint32_t item_id;
    std::uint32_t quantity;
    std::uint8_t occupied;
    std::uint8_t reserved0[3];
};

struct Gs1CampaignSiteView
{
    std::uint32_t site_id;
    Gs1SiteState site_state;
    std::uint8_t reserved0[3];
    std::int32_t regional_map_tile_x;
    std::int32_t regional_map_tile_y;
    std::uint32_t site_archetype_id;
    std::uint32_t featured_faction_id;
    std::uint32_t attempt_count;
    std::uint32_t support_package_id;
    std::uint32_t completion_reputation_reward;
    std::int32_t completion_faction_reputation_reward;
    std::uint32_t adjacent_site_id_offset;
    std::uint32_t adjacent_site_id_count;
    std::uint32_t exported_support_item_offset;
    std::uint32_t exported_support_item_count;
    std::uint32_t nearby_aura_modifier_id_offset;
    std::uint32_t nearby_aura_modifier_id_count;
    std::uint8_t has_support_package_id;
    std::uint8_t reserved1[3];
};

struct Gs1ProgressionEntryView
{
    std::uint16_t entry_id;
    std::uint16_t reputation_requirement;
    std::uint16_t content_id;
    std::uint16_t tech_node_id;
    std::uint8_t faction_id;
    Gs1ProgressionEntryKind entry_kind;
    std::uint16_t flags;
    std::uint8_t content_kind;
    std::uint8_t tier_index;
};

struct Gs1CampaignStateView
{
    std::uint32_t campaign_id;
    std::uint64_t campaign_seed;
    double campaign_clock_minutes_elapsed;
    std::uint32_t campaign_days_total;
    std::uint32_t campaign_days_remaining;
    std::uint32_t total_reputation;
    std::int32_t selected_site_id;
    std::int32_t active_site_id;
    const Gs1FactionProgressView* faction_progress;
    std::uint32_t faction_progress_count;
    const Gs1CampaignSiteView* sites;
    std::uint32_t site_count;
    const std::uint32_t* revealed_site_ids;
    std::uint32_t revealed_site_count;
    const std::uint32_t* available_site_ids;
    std::uint32_t available_site_count;
    const std::uint32_t* completed_site_ids;
    std::uint32_t completed_site_count;
    const std::uint32_t* site_adjacency_ids;
    std::uint32_t site_adjacency_id_count;
    const Gs1LoadoutSlotView* site_exported_support_items;
    std::uint32_t site_exported_support_item_count;
    const std::uint32_t* site_nearby_aura_modifier_ids;
    std::uint32_t site_nearby_aura_modifier_id_count;
    const Gs1LoadoutSlotView* baseline_deployment_items;
    std::uint32_t baseline_deployment_item_count;
    const Gs1LoadoutSlotView* available_exported_support_items;
    std::uint32_t available_exported_support_item_count;
    const Gs1LoadoutSlotView* selected_loadout_slots;
    std::uint32_t selected_loadout_slot_count;
    const std::uint32_t* active_nearby_aura_modifier_ids;
    std::uint32_t active_nearby_aura_modifier_id_count;
    std::uint32_t support_quota_per_contributor;
    std::uint32_t support_quota;
    const Gs1ProgressionEntryView* progression_entries;
    std::uint32_t progression_entry_count;
};

struct Gs1InventoryStorageView
{
    std::uint32_t storage_id;
    std::uint64_t container_entity_id;
    std::uint64_t owner_device_entity_id;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t reserved0[3];
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint32_t flags;
    std::uint32_t slot_offset;
    std::uint32_t slot_count;
};

struct Gs1InventorySlotView
{
    std::uint32_t storage_id;
    Gs1InventoryContainerKind container_kind;
    std::uint16_t slot_index;
    std::uint8_t occupied;
    std::uint8_t reserved0;
    std::uint32_t item_instance_id;
    std::uint32_t item_id;
    std::uint32_t quantity;
    float condition;
    float freshness;
};

struct Gs1TaskRewardDraftOptionView
{
    std::uint32_t reward_candidate_id;
    std::uint8_t selected;
    std::uint8_t reserved0[3];
};

struct Gs1TaskView
{
    std::uint32_t task_instance_id;
    std::uint32_t task_template_id;
    std::uint32_t publisher_faction_id;
    std::uint32_t task_tier_id;
    std::uint32_t target_amount;
    std::uint32_t required_count;
    std::uint32_t current_progress_amount;
    std::uint32_t item_id;
    std::uint32_t plant_id;
    std::uint32_t recipe_id;
    std::uint32_t structure_id;
    std::uint32_t secondary_structure_id;
    std::uint32_t tertiary_structure_id;
    Gs1SiteActionKind action_kind;
    std::uint8_t runtime_list_kind;
    std::uint8_t requirement_mask;
    std::uint8_t reserved0;
    float threshold_value;
    float expected_task_hours_in_game;
    float risk_multiplier;
    std::uint32_t direct_cost_cash_points;
    std::uint32_t time_cost_cash_points;
    std::uint32_t risk_cost_cash_points;
    std::uint32_t difficulty_cash_points;
    std::uint32_t reward_budget_cash_points;
    std::uint32_t reward_draft_option_offset;
    std::uint32_t reward_draft_option_count;
    std::uint32_t chain_id;
    std::uint32_t chain_step_index;
    std::uint32_t follow_up_task_template_id;
    double progress_accumulator;
    std::uint8_t has_chain;
    std::uint8_t has_follow_up_task_template;
    std::uint8_t reserved1[2];
};

struct Gs1SiteModifierView
{
    std::uint32_t modifier_id;
    std::uint32_t source_item_id;
    double duration_world_minutes;
    double remaining_world_minutes;
    std::uint8_t timed;
    std::uint8_t reserved0[7];
};

struct Gs1PhoneListingView
{
    std::uint32_t listing_id;
    std::uint32_t item_or_unlockable_id;
    std::int32_t price_cash_points;
    std::uint32_t quantity;
    std::uint32_t cart_quantity;
    std::uint32_t stock_refresh_generation;
    std::uint8_t listing_kind;
    std::uint8_t occupied;
    std::uint8_t generated_from_stock;
    std::uint8_t reserved0;
};

struct Gs1WorkerStateView
{
    std::uint64_t worker_entity_id;
    std::int32_t tile_x;
    std::int32_t tile_y;
    float world_x;
    float world_y;
    float facing_degrees;
    float health;
    float hydration;
    float nourishment;
    float energy_cap;
    float energy;
    float morale;
    float work_efficiency;
    std::uint8_t is_sheltered;
    std::uint8_t reserved0[3];
};

struct Gs1CampStateView
{
    std::int32_t camp_anchor_tile_x;
    std::int32_t camp_anchor_tile_y;
    std::int32_t starter_storage_tile_x;
    std::int32_t starter_storage_tile_y;
    std::int32_t delivery_box_tile_x;
    std::int32_t delivery_box_tile_y;
    float camp_durability;
    std::uint8_t camp_protection_resolved;
    std::uint8_t delivery_point_operational;
    std::uint8_t shared_storage_access_enabled;
    std::uint8_t reserved0;
};

struct Gs1WeatherStateView
{
    float heat;
    float wind;
    float dust;
    float wind_direction_degrees;
    std::uint32_t forecast_profile_id;
    float site_weather_bias;
};

struct Gs1SiteEventView
{
    std::uint32_t active_event_template_id;
    std::uint8_t has_active_event;
    std::uint8_t reserved0[3];
    double start_time_minutes;
    double peak_time_minutes;
    double peak_duration_minutes;
    double end_time_minutes;
    double minutes_until_next_wave;
    float event_heat_pressure;
    float event_wind_pressure;
    float event_dust_pressure;
    std::uint32_t wave_sequence_index;
};

struct Gs1SiteActionView
{
    std::uint32_t current_action_id;
    std::uint8_t has_current_action;
    Gs1SiteActionKind action_kind;
    std::uint16_t quantity;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint8_t has_target_tile;
    std::uint8_t reserved0[3];
    std::int32_t approach_tile_x;
    std::int32_t approach_tile_y;
    std::uint8_t has_approach_tile;
    std::uint8_t reserved1[3];
    std::uint32_t primary_subject_id;
    std::uint32_t secondary_subject_id;
    std::uint32_t item_id;
    std::uint32_t placement_reservation_token;
    std::uint8_t request_flags;
    std::uint8_t awaiting_placement_reservation;
    std::uint8_t reactivate_placement_mode_on_completion;
    std::uint8_t reserved2;
    double total_action_minutes;
    double remaining_action_minutes;
    double started_at_world_minute;
    std::uint8_t has_started_at_world_minute;
    std::uint8_t reserved3[7];
};

struct Gs1SiteObjectiveView
{
    std::uint8_t objective_type;
    std::uint8_t target_edge;
    std::uint8_t target_band_width;
    std::uint8_t has_hold_baseline;
    double time_limit_minutes;
    double completion_hold_minutes;
    double completion_hold_progress_minutes;
    double paused_main_timer_minutes;
    double last_evaluated_world_time_minutes;
    std::int32_t target_cash_points;
    float highway_max_average_sand_cover;
    float last_target_average_sand_level;
};

struct Gs1SiteCountersView
{
    std::uint32_t fully_grown_tile_count;
    std::uint32_t site_completion_tile_threshold;
    std::uint32_t tracked_living_plant_count;
    float objective_progress_normalized;
    float highway_average_sand_cover;
    std::uint8_t all_tracked_living_plants_stable;
    std::uint8_t reserved0[3];
};

struct Gs1SiteStateView
{
    std::uint32_t site_run_id;
    std::uint32_t site_id;
    std::uint32_t site_archetype_id;
    std::uint32_t attempt_index;
    std::uint64_t site_attempt_seed;
    std::uint32_t run_status;
    double world_time_minutes;
    std::uint32_t day_index;
    std::uint32_t day_phase;
    double ecology_pulse_accumulator;
    double task_refresh_accumulator;
    double delivery_accumulator;
    double accumulator_real_seconds;
    Gs1CampStateView camp;
    Gs1SiteCountersView counters;
    Gs1WeatherStateView weather;
    Gs1SiteEventView event;
    Gs1SiteActionView action;
    Gs1SiteObjectiveView objective;
    Gs1WorkerStateView worker;
    std::int32_t current_cash_points;
    std::int32_t phone_delivery_fee_cash_points;
    std::uint16_t phone_delivery_minutes;
    std::uint16_t reserved0;
    std::uint32_t worker_pack_storage_id;
    std::uint32_t opened_device_storage_id;
    std::uint32_t next_storage_id;
    std::uint32_t storage_count;
    const Gs1InventoryStorageView* storages;
    const Gs1InventorySlotView* storage_slots;
    std::uint32_t storage_slot_count;
    const Gs1TaskView* tasks;
    std::uint32_t task_count;
    const Gs1TaskRewardDraftOptionView* task_reward_draft_options;
    std::uint32_t task_reward_draft_option_count;
    const Gs1SiteModifierView* active_modifiers;
    std::uint32_t active_modifier_count;
    const Gs1PhoneListingView* phone_listings;
    std::uint32_t phone_listing_count;
    std::uint32_t result_newly_revealed_site_count;
    std::uint32_t tile_count;
    std::uint32_t tile_width;
    std::uint32_t tile_height;
};

struct Gs1GameStateView
{
    std::uint32_t struct_size;
    std::uint32_t revision;
    Gs1AppState app_state;
    std::uint8_t has_campaign;
    std::uint8_t has_active_site;
    std::uint8_t reserved0;
    const Gs1CampaignStateView* campaign;
    const Gs1SiteStateView* active_site;
};

struct Gs1SiteTileView
{
    std::uint32_t tile_index;
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint64_t tile_entity_id;
    std::uint64_t device_entity_id;
    std::uint32_t terrain_type_id;
    std::uint32_t ground_cover_type_id;
    std::uint32_t plant_id;
    std::uint32_t structure_id;
    float soil_fertility;
    float moisture;
    float soil_salinity;
    float sand_burial;
    float plant_density;
    float growth_pressure;
    std::uint8_t traversable;
    std::uint8_t plantable;
    std::uint8_t reserved_by_structure;
    std::uint8_t excavation_depth;
    float local_heat;
    float local_wind;
    float local_dust;
    float plant_heat_protection;
    float plant_wind_protection;
    float plant_dust_protection;
    float device_heat_protection;
    float device_wind_protection;
    float device_dust_protection;
    float device_integrity;
    float device_efficiency;
    float device_stored_water;
    std::uint8_t device_fixed_integrity;
    std::uint8_t reserved0[3];
};

static_assert(std::is_standard_layout_v<Gs1GameStateView>);
static_assert(std::is_trivially_copyable_v<Gs1GameStateView>);
static_assert(std::is_standard_layout_v<Gs1SiteTileView>);
static_assert(std::is_trivially_copyable_v<Gs1SiteTileView>);
