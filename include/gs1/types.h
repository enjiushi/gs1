#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

struct Gs1RuntimeHandle;

enum Gs1LogLevel : std::uint32_t
{
    GS1_LOG_LEVEL_DEBUG = 0,
    GS1_LOG_LEVEL_INFO = 1,
    GS1_LOG_LEVEL_WARNING = 2,
    GS1_LOG_LEVEL_ERROR = 3
};

enum Gs1AppState : std::uint32_t
{
    GS1_APP_STATE_BOOT = 0,
    GS1_APP_STATE_MAIN_MENU = 1,
    GS1_APP_STATE_CAMPAIGN_SETUP = 2,
    GS1_APP_STATE_REGIONAL_MAP = 3,
    GS1_APP_STATE_SITE_LOADING = 4,
    GS1_APP_STATE_SITE_ACTIVE = 5,
    GS1_APP_STATE_SITE_PAUSED = 6,
    GS1_APP_STATE_SITE_RESULT = 7,
    GS1_APP_STATE_CAMPAIGN_END = 8
};

enum Gs1SiteState : std::uint32_t
{
    GS1_SITE_STATE_LOCKED = 0,
    GS1_SITE_STATE_AVAILABLE = 1,
    GS1_SITE_STATE_COMPLETED = 2
};

enum Gs1HostEventType : std::uint32_t
{
    GS1_HOST_EVENT_NONE = 0,
    GS1_HOST_EVENT_UI_ACTION = 1
};

enum Gs1UiSetupId : std::uint32_t
{
    GS1_UI_SETUP_NONE = 0,
    GS1_UI_SETUP_MAIN_MENU = 1,
    GS1_UI_SETUP_REGIONAL_MAP_SELECTION = 2,
    GS1_UI_SETUP_SITE_RESULT = 3
};

enum Gs1UiElementType : std::uint32_t
{
    GS1_UI_ELEMENT_NONE = 0,
    GS1_UI_ELEMENT_BUTTON = 1,
    GS1_UI_ELEMENT_LABEL = 2
};

enum Gs1UiElementFlags : std::uint32_t
{
    GS1_UI_ELEMENT_FLAG_NONE = 0,
    GS1_UI_ELEMENT_FLAG_PRIMARY = 1u << 0,
    GS1_UI_ELEMENT_FLAG_DISABLED = 1u << 1,
    GS1_UI_ELEMENT_FLAG_BACKGROUND_CLICK = 1u << 2
};

enum Gs1UiSetupPresentationType : std::uint32_t
{
    GS1_UI_SETUP_PRESENTATION_NONE = 0,
    GS1_UI_SETUP_PRESENTATION_NORMAL = 1,
    GS1_UI_SETUP_PRESENTATION_OVERLAY = 2
};

enum Gs1UiActionType : std::uint32_t
{
    GS1_UI_ACTION_NONE = 0,
    GS1_UI_ACTION_START_NEW_CAMPAIGN = 1,
    GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE = 2,
    GS1_UI_ACTION_START_SITE_ATTEMPT = 3,
    GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP = 4,
    GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION = 5
};

enum Gs1FeedbackEventType : std::uint32_t
{
    GS1_FEEDBACK_EVENT_NONE = 0,
    GS1_FEEDBACK_EVENT_PHYSICS_CONTACT = 1,
    GS1_FEEDBACK_EVENT_TRACE_HIT = 2,
    GS1_FEEDBACK_EVENT_ANIMATION_NOTIFY = 3
};

enum Gs1ProjectionMode : std::uint32_t
{
    GS1_PROJECTION_MODE_SNAPSHOT = 0,
    GS1_PROJECTION_MODE_DELTA = 1
};

enum Gs1PresentationDirtyFlags : std::uint32_t
{
    GS1_PRESENTATION_DIRTY_NONE = 0,
    GS1_PRESENTATION_DIRTY_APP_STATE = 1u << 0,
    GS1_PRESENTATION_DIRTY_REGIONAL_MAP = 1u << 1,
    GS1_PRESENTATION_DIRTY_SITE = 1u << 2,
    GS1_PRESENTATION_DIRTY_HUD = 1u << 3,
    GS1_PRESENTATION_DIRTY_PHONE = 1u << 4,
    GS1_PRESENTATION_DIRTY_NOTIFICATIONS = 1u << 5,
    GS1_PRESENTATION_DIRTY_SITE_RESULT = 1u << 6,
    GS1_PRESENTATION_DIRTY_ALL = 0xffffffffu
};

enum Gs1WeatherEventPhase : std::uint32_t
{
    GS1_WEATHER_EVENT_PHASE_NONE = 0,
    GS1_WEATHER_EVENT_PHASE_WARNING = 1,
    GS1_WEATHER_EVENT_PHASE_BUILD = 2,
    GS1_WEATHER_EVENT_PHASE_PEAK = 3,
    GS1_WEATHER_EVENT_PHASE_DECAY = 4,
    GS1_WEATHER_EVENT_PHASE_AFTERMATH = 5
};

enum Gs1TaskPresentationListKind : std::uint32_t
{
    GS1_TASK_PRESENTATION_LIST_VISIBLE = 0,
    GS1_TASK_PRESENTATION_LIST_ACCEPTED = 1,
    GS1_TASK_PRESENTATION_LIST_COMPLETED = 2
};

enum Gs1InventoryContainerKind : std::uint32_t
{
    GS1_INVENTORY_CONTAINER_WORKER_PACK = 0,
    GS1_INVENTORY_CONTAINER_CAMP_STORAGE = 1
};

enum Gs1PhoneListingPresentationKind : std::uint32_t
{
    GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM = 0,
    GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM = 1,
    GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR = 2,
    GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE = 3
};

enum Gs1NotificationKind : std::uint32_t
{
    GS1_NOTIFICATION_KIND_INFO = 0,
    GS1_NOTIFICATION_KIND_WARNING = 1,
    GS1_NOTIFICATION_KIND_ERROR = 2,
    GS1_NOTIFICATION_KIND_TASK = 3,
    GS1_NOTIFICATION_KIND_SITE_RESULT = 4
};

enum Gs1OneShotCueKind : std::uint32_t
{
    GS1_ONE_SHOT_CUE_NONE = 0,
    GS1_ONE_SHOT_CUE_ACTION_COMPLETED = 1,
    GS1_ONE_SHOT_CUE_ACTION_FAILED = 2,
    GS1_ONE_SHOT_CUE_HAZARD_PEAK = 3,
    GS1_ONE_SHOT_CUE_SITE_COMPLETED = 4,
    GS1_ONE_SHOT_CUE_SITE_FAILED = 5
};

enum Gs1SiteAttemptResult : std::uint32_t
{
    GS1_SITE_ATTEMPT_RESULT_NONE = 0,
    GS1_SITE_ATTEMPT_RESULT_COMPLETED = 1,
    GS1_SITE_ATTEMPT_RESULT_FAILED = 2
};

enum Gs1EngineCommandType : std::uint32_t
{
    GS1_ENGINE_COMMAND_NONE = 0,
    GS1_ENGINE_COMMAND_LOG_TEXT = 1,
    GS1_ENGINE_COMMAND_SET_APP_STATE = 2,
    GS1_ENGINE_COMMAND_PRESENTATION_DIRTY = 3,

    GS1_ENGINE_COMMAND_BEGIN_REGIONAL_MAP_SNAPSHOT = 10,
    GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_UPSERT = 11,
    GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_REMOVE = 12,
    GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_UPSERT = 13,
    GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_REMOVE = 14,
    GS1_ENGINE_COMMAND_END_REGIONAL_MAP_SNAPSHOT = 15,

    GS1_ENGINE_COMMAND_BEGIN_UI_SETUP = 16,
    GS1_ENGINE_COMMAND_UI_ELEMENT_UPSERT = 17,
    GS1_ENGINE_COMMAND_END_UI_SETUP = 18,
    GS1_ENGINE_COMMAND_CLOSE_UI_SETUP = 19,

    GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT = 20,
    GS1_ENGINE_COMMAND_SITE_TILE_UPSERT = 21,
    GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE = 22,
    GS1_ENGINE_COMMAND_SITE_CAMP_UPDATE = 23,
    GS1_ENGINE_COMMAND_SITE_WEATHER_UPDATE = 24,
    GS1_ENGINE_COMMAND_SITE_INVENTORY_SLOT_UPSERT = 25,
    GS1_ENGINE_COMMAND_SITE_TASK_UPSERT = 26,
    GS1_ENGINE_COMMAND_SITE_TASK_REMOVE = 27,
    GS1_ENGINE_COMMAND_SITE_PHONE_LISTING_UPSERT = 28,
    GS1_ENGINE_COMMAND_SITE_PHONE_LISTING_REMOVE = 29,
    GS1_ENGINE_COMMAND_END_SITE_SNAPSHOT = 30,

    GS1_ENGINE_COMMAND_HUD_STATE = 40,
    GS1_ENGINE_COMMAND_NOTIFICATION_PUSH = 41,
    GS1_ENGINE_COMMAND_SITE_RESULT_READY = 42,
    GS1_ENGINE_COMMAND_PLAY_ONE_SHOT_CUE = 43
};

struct Gs1RuntimeCreateDesc
{
    std::uint32_t struct_size;
    std::uint32_t api_version;
    double fixed_step_seconds;
};

struct Gs1InputSnapshot
{
    std::uint32_t struct_size;
    std::uint64_t frame_number;
    float move_x;
    float move_y;
    float cursor_world_x;
    float cursor_world_y;
    std::uint32_t buttons_down_mask;
    std::uint32_t buttons_pressed_mask;
    std::uint32_t buttons_released_mask;
};

struct Gs1UiAction
{
    Gs1UiActionType type;
    std::uint32_t target_id;
    std::uint64_t arg0;
    std::uint64_t arg1;
};

struct Gs1HostEventUiActionData
{
    Gs1UiAction action;
};

struct Gs1HostEventEmptyData
{
    std::uint64_t reserved0;
    std::uint64_t reserved1;
};

union Gs1HostEventPayload
{
    Gs1HostEventUiActionData ui_action;
    Gs1HostEventEmptyData empty;
};

struct Gs1HostEvent
{
    std::uint32_t struct_size;
    Gs1HostEventType type;
    Gs1HostEventPayload payload;
};

struct Gs1FeedbackEventData
{
    std::uint32_t site_id;
    std::uint32_t subject_id;
    std::uint32_t other_id;
    std::uint32_t code;
};

struct Gs1FeedbackEvent
{
    std::uint32_t struct_size;
    Gs1FeedbackEventType type;
    Gs1FeedbackEventData data;
};

struct Gs1Phase1Request
{
    std::uint32_t struct_size;
    double real_delta_seconds;
    const Gs1InputSnapshot* input;
};

struct Gs1Phase1Result
{
    std::uint32_t struct_size;
    std::uint32_t fixed_steps_executed;
    std::uint32_t engine_commands_queued;
    std::uint32_t processed_host_event_count;
};

struct Gs1Phase2Request
{
    std::uint32_t struct_size;
};

struct Gs1Phase2Result
{
    std::uint32_t struct_size;
    std::uint32_t processed_host_event_count;
    std::uint32_t processed_feedback_event_count;
    std::uint32_t engine_commands_queued;
    std::uint32_t reserved;
};

struct Gs1EngineCommandLogTextData
{
    Gs1LogLevel level;
    std::uint32_t reserved;
    char text[120];
};

struct Gs1EngineCommandSetAppStateData
{
    Gs1AppState app_state;
    std::uint32_t reserved0;
    std::uint64_t frame_ordinal;
};

struct Gs1EngineCommandPresentationDirtyData
{
    std::uint32_t dirty_flags;
    std::uint32_t reserved0;
    std::uint64_t revision;
};

struct Gs1EngineCommandRegionalMapSnapshotData
{
    Gs1ProjectionMode mode;
    std::uint32_t site_count;
    std::uint32_t link_count;
    std::uint32_t selected_site_id;
};

struct Gs1EngineCommandRegionalMapSiteData
{
    std::uint32_t site_id;
    Gs1SiteState site_state;
    std::uint32_t site_archetype_id;
    std::uint32_t flags;
    std::int32_t map_x;
    std::int32_t map_y;
    std::uint32_t support_package_id;
    std::uint32_t support_preview_mask;
};

struct Gs1EngineCommandRegionalMapLinkData
{
    std::uint32_t from_site_id;
    std::uint32_t to_site_id;
    std::uint32_t flags;
    std::uint32_t reserved;
};

struct Gs1EngineCommandUiSetupData
{
    Gs1UiSetupId setup_id;
    Gs1ProjectionMode mode;
    Gs1UiSetupPresentationType presentation_type;
    std::uint32_t element_count;
    std::uint32_t context_id;
};

struct Gs1EngineCommandCloseUiSetupData
{
    Gs1UiSetupId setup_id;
    Gs1UiSetupPresentationType presentation_type;
    std::uint32_t reserved0;
    std::uint32_t reserved1;
};

struct Gs1EngineCommandUiElementData
{
    std::uint32_t element_id;
    Gs1UiElementType element_type;
    std::uint32_t flags;
    std::uint32_t reserved0;
    Gs1UiAction action;
    char text[64];
};

struct Gs1EngineCommandSiteSnapshotData
{
    Gs1ProjectionMode mode;
    std::uint32_t site_id;
    std::uint32_t site_archetype_id;
    std::uint32_t width;
    std::uint32_t height;
};

struct Gs1EngineCommandSiteTileData
{
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t terrain_type_id;
    std::uint32_t plant_type_id;
    std::uint32_t structure_type_id;
    std::uint32_t ground_cover_type_id;
    float plant_density;
    float sand_burial;
};

struct Gs1EngineCommandWorkerData
{
    float tile_x;
    float tile_y;
    float facing_degrees;
    float health_normalized;
    float hydration_normalized;
    float energy_normalized;
    std::uint32_t flags;
    std::uint32_t current_action_kind;
};

struct Gs1EngineCommandCampData
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    float durability_normalized;
    std::uint32_t flags;
};

struct Gs1EngineCommandWeatherData
{
    float heat;
    float wind;
    float dust;
    std::uint32_t event_template_id;
    Gs1WeatherEventPhase event_phase;
    float phase_minutes_remaining;
    std::uint32_t reserved;
};

struct Gs1EngineCommandInventorySlotData
{
    Gs1InventoryContainerKind container_kind;
    std::uint32_t slot_index;
    std::uint32_t item_id;
    std::uint32_t quantity;
    float condition;
    float freshness;
    std::uint32_t flags;
    std::uint32_t reserved;
};

struct Gs1EngineCommandTaskData
{
    std::uint32_t task_instance_id;
    std::uint32_t task_template_id;
    Gs1TaskPresentationListKind list_kind;
    std::uint32_t current_progress;
    std::uint32_t target_progress;
    std::uint32_t publisher_faction_id;
    std::uint32_t flags;
};

struct Gs1EngineCommandPhoneListingData
{
    std::uint32_t listing_id;
    Gs1PhoneListingPresentationKind listing_kind;
    std::uint32_t item_or_unlockable_id;
    std::int32_t price;
    std::uint32_t quantity;
    std::uint32_t flags;
    std::uint32_t related_site_id;
};

struct Gs1EngineCommandHudStateData
{
    float player_health;
    float player_hydration;
    float player_energy;
    std::int32_t current_money;
    std::uint32_t active_task_count;
    std::uint32_t warning_code;
    std::uint32_t current_action_kind;
    float site_completion_normalized;
};

struct Gs1EngineCommandNotificationData
{
    Gs1NotificationKind kind;
    std::uint32_t notification_code;
    std::uint32_t subject_id;
    std::uint32_t arg0;
    std::uint32_t arg1;
    char text[96];
};

struct Gs1EngineCommandSiteResultData
{
    std::uint32_t site_id;
    Gs1SiteAttemptResult result;
    std::uint32_t newly_revealed_site_count;
    std::uint32_t reserved;
};

struct Gs1EngineCommandOneShotCueData
{
    Gs1OneShotCueKind cue_kind;
    std::uint32_t subject_id;
    float world_x;
    float world_y;
    std::uint32_t arg0;
    std::uint32_t arg1;
};

struct Gs1EngineCommandPayload
{
    static constexpr std::size_t raw_u32_count = 32U;
    static constexpr std::size_t byte_count = sizeof(std::uint32_t) * raw_u32_count;
    static constexpr std::size_t payload_alignment = alignof(std::uint64_t);

    alignas(std::uint64_t) unsigned char raw_bytes[byte_count];

    [[nodiscard]] void* data() noexcept { return raw_bytes; }
    [[nodiscard]] const void* data() const noexcept { return raw_bytes; }

    template <typename PayloadData>
    [[nodiscard]] PayloadData& as() noexcept
    {
        validate_payload_type<PayloadData>();
        return *static_cast<PayloadData*>(data());
    }

    template <typename PayloadData>
    [[nodiscard]] const PayloadData& as() const noexcept
    {
        validate_payload_type<PayloadData>();
        return *static_cast<const PayloadData*>(data());
    }

private:
    template <typename PayloadData>
    static constexpr void validate_payload_type() noexcept
    {
        static_assert(
            std::is_trivial_v<PayloadData> && std::is_standard_layout_v<PayloadData>,
            "Engine command payload data must stay POD-like.");
        static_assert(sizeof(PayloadData) <= byte_count, "Engine command payload data exceeds the raw payload size.");
        static_assert(
            alignof(PayloadData) <= payload_alignment,
            "Engine command payload data requires stronger alignment than the raw payload storage.");
    }
};

static_assert(
    sizeof(Gs1EngineCommandPayload) == Gs1EngineCommandPayload::byte_count,
    "Engine command payload storage must stay exactly 32 uint32 words.");
static_assert(
    alignof(Gs1EngineCommandPayload) == Gs1EngineCommandPayload::payload_alignment,
    "Engine command payload alignment changed.");

struct Gs1EngineCommand
{
    std::uint32_t struct_size;
    Gs1EngineCommandType type;
    Gs1EngineCommandPayload payload;

    template <typename PayloadData>
    [[nodiscard]] PayloadData& payload_as() noexcept
    {
        return payload.as<PayloadData>();
    }

    template <typename PayloadData>
    [[nodiscard]] const PayloadData& payload_as() const noexcept
    {
        return payload.as<PayloadData>();
    }
};
