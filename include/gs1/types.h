#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>

struct Gs1RuntimeHandle;

inline constexpr std::size_t GS1_MESSAGE_CACHE_LINE_SIZE = 64U;
inline constexpr std::size_t GS1_MESSAGE_PAYLOAD_BYTE_COUNT = GS1_MESSAGE_CACHE_LINE_SIZE - sizeof(std::uint8_t);

#define GS1_ASSERT_TRIVIAL_SCHEMA(Type) \
    static_assert(std::is_standard_layout_v<Type>, #Type " must remain standard layout."); \
    static_assert(std::is_trivial_v<Type>, #Type " must remain trivial."); \
    static_assert(std::is_trivially_copyable_v<Type>, #Type " must remain trivially copyable.")

#define GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Type, ExpectedSize) \
    GS1_ASSERT_TRIVIAL_SCHEMA(Type); \
    static_assert(sizeof(Type) == (ExpectedSize), #Type " size changed; revisit transport packing.")

enum Gs1LogLevel : std::uint8_t
{
    GS1_LOG_LEVEL_DEBUG = 0,
    GS1_LOG_LEVEL_INFO = 1,
    GS1_LOG_LEVEL_WARNING = 2,
    GS1_LOG_LEVEL_ERROR = 3
};

enum Gs1AppState : std::uint8_t
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

enum Gs1SiteState : std::uint8_t
{
    GS1_SITE_STATE_LOCKED = 0,
    GS1_SITE_STATE_AVAILABLE = 1,
    GS1_SITE_STATE_COMPLETED = 2
};

enum Gs1UiSetupId : std::uint8_t
{
    GS1_UI_SETUP_NONE = 0,
    GS1_UI_SETUP_MAIN_MENU = 1,
    GS1_UI_SETUP_REGIONAL_MAP_SELECTION = 2,
    GS1_UI_SETUP_SITE_RESULT = 3,
    GS1_UI_SETUP_REGIONAL_MAP_MENU = 4,
    GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE = 5,
    GS1_UI_SETUP_SITE_PROTECTION_SELECTOR = 6
};

enum Gs1UiElementType : std::uint8_t
{
    GS1_UI_ELEMENT_NONE = 0,
    GS1_UI_ELEMENT_BUTTON = 1,
    GS1_UI_ELEMENT_LABEL = 2
};

enum Gs1UiElementFlags : std::uint8_t
{
    GS1_UI_ELEMENT_FLAG_NONE = 0,
    GS1_UI_ELEMENT_FLAG_PRIMARY = 1u << 0,
    GS1_UI_ELEMENT_FLAG_DISABLED = 1u << 1,
    GS1_UI_ELEMENT_FLAG_BACKGROUND_CLICK = 1u << 2
};

enum Gs1UiSetupPresentationType : std::uint8_t
{
    GS1_UI_SETUP_PRESENTATION_NONE = 0,
    GS1_UI_SETUP_PRESENTATION_NORMAL = 1,
    GS1_UI_SETUP_PRESENTATION_OVERLAY = 2
};

enum Gs1ProgressionEntryKind : std::uint8_t
{
    GS1_PROGRESSION_ENTRY_NONE = 0,
    GS1_PROGRESSION_ENTRY_REPUTATION_UNLOCK = 1,
    GS1_PROGRESSION_ENTRY_TECHNOLOGY_NODE = 2
};

enum Gs1ProgressionEntryFlags : std::uint8_t
{
    GS1_PROGRESSION_ENTRY_FLAG_NONE = 0,
    GS1_PROGRESSION_ENTRY_FLAG_UNLOCKED = 1u << 0,
    GS1_PROGRESSION_ENTRY_FLAG_LOCKED = 1u << 1,
    GS1_PROGRESSION_ENTRY_FLAG_ACTIONABLE = 1u << 2,
    GS1_PROGRESSION_ENTRY_FLAG_SELECTED = 1u << 3
};

enum Gs1GameplayActionType : std::uint8_t
{
    GS1_GAMEPLAY_ACTION_NONE = 0,
    GS1_GAMEPLAY_ACTION_START_NEW_CAMPAIGN = 1,
    GS1_GAMEPLAY_ACTION_SELECT_DEPLOYMENT_SITE = 2,
    GS1_GAMEPLAY_ACTION_START_SITE_ATTEMPT = 3,
    GS1_GAMEPLAY_ACTION_RETURN_TO_REGIONAL_MAP = 4,
    GS1_GAMEPLAY_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION = 5,
    GS1_GAMEPLAY_ACTION_ACCEPT_TASK = 6,
    GS1_GAMEPLAY_ACTION_CLAIM_TASK_REWARD = 7,
    GS1_GAMEPLAY_ACTION_BUY_PHONE_LISTING = 8,
    GS1_GAMEPLAY_ACTION_SELL_PHONE_LISTING = 9,
    GS1_GAMEPLAY_ACTION_HIRE_CONTRACTOR = 12,
    GS1_GAMEPLAY_ACTION_PURCHASE_SITE_UNLOCKABLE = 13,
    GS1_GAMEPLAY_ACTION_ADD_PHONE_LISTING_TO_CART = 14,
    GS1_GAMEPLAY_ACTION_REMOVE_PHONE_LISTING_FROM_CART = 15,
    GS1_GAMEPLAY_ACTION_CHECKOUT_PHONE_CART = 16,
    GS1_GAMEPLAY_ACTION_SET_PHONE_PANEL_SECTION = 17,
    GS1_GAMEPLAY_ACTION_OPEN_REGIONAL_MAP_TECH_TREE = 18,
    GS1_GAMEPLAY_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE = 19,
    GS1_GAMEPLAY_ACTION_CLAIM_TECHNOLOGY_NODE = 20,
    GS1_GAMEPLAY_ACTION_SELECT_TECH_TREE_FACTION_TAB = 21,
    GS1_GAMEPLAY_ACTION_CLOSE_PHONE_PANEL = 22,
    GS1_GAMEPLAY_ACTION_REFUND_TECHNOLOGY_NODE = 23,
    GS1_GAMEPLAY_ACTION_OPEN_SITE_PROTECTION_SELECTOR = 24,
    GS1_GAMEPLAY_ACTION_CLOSE_SITE_PROTECTION_UI = 25,
    GS1_GAMEPLAY_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE = 26,
    GS1_GAMEPLAY_ACTION_END_SITE_MODIFIER = 27
};

using Gs1UiActionType = Gs1GameplayActionType;

inline constexpr Gs1UiActionType GS1_UI_ACTION_NONE = GS1_GAMEPLAY_ACTION_NONE;
inline constexpr Gs1UiActionType GS1_UI_ACTION_START_NEW_CAMPAIGN = GS1_GAMEPLAY_ACTION_START_NEW_CAMPAIGN;
inline constexpr Gs1UiActionType GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE = GS1_GAMEPLAY_ACTION_SELECT_DEPLOYMENT_SITE;
inline constexpr Gs1UiActionType GS1_UI_ACTION_START_SITE_ATTEMPT = GS1_GAMEPLAY_ACTION_START_SITE_ATTEMPT;
inline constexpr Gs1UiActionType GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP = GS1_GAMEPLAY_ACTION_RETURN_TO_REGIONAL_MAP;
inline constexpr Gs1UiActionType GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION =
    GS1_GAMEPLAY_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
inline constexpr Gs1UiActionType GS1_UI_ACTION_ACCEPT_TASK = GS1_GAMEPLAY_ACTION_ACCEPT_TASK;
inline constexpr Gs1UiActionType GS1_UI_ACTION_CLAIM_TASK_REWARD = GS1_GAMEPLAY_ACTION_CLAIM_TASK_REWARD;
inline constexpr Gs1UiActionType GS1_UI_ACTION_BUY_PHONE_LISTING = GS1_GAMEPLAY_ACTION_BUY_PHONE_LISTING;
inline constexpr Gs1UiActionType GS1_UI_ACTION_SELL_PHONE_LISTING = GS1_GAMEPLAY_ACTION_SELL_PHONE_LISTING;
inline constexpr Gs1UiActionType GS1_UI_ACTION_HIRE_CONTRACTOR = GS1_GAMEPLAY_ACTION_HIRE_CONTRACTOR;
inline constexpr Gs1UiActionType GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE =
    GS1_GAMEPLAY_ACTION_PURCHASE_SITE_UNLOCKABLE;
inline constexpr Gs1UiActionType GS1_UI_ACTION_ADD_PHONE_LISTING_TO_CART =
    GS1_GAMEPLAY_ACTION_ADD_PHONE_LISTING_TO_CART;
inline constexpr Gs1UiActionType GS1_UI_ACTION_REMOVE_PHONE_LISTING_FROM_CART =
    GS1_GAMEPLAY_ACTION_REMOVE_PHONE_LISTING_FROM_CART;
inline constexpr Gs1UiActionType GS1_UI_ACTION_CHECKOUT_PHONE_CART = GS1_GAMEPLAY_ACTION_CHECKOUT_PHONE_CART;
inline constexpr Gs1UiActionType GS1_UI_ACTION_SET_PHONE_PANEL_SECTION = GS1_GAMEPLAY_ACTION_SET_PHONE_PANEL_SECTION;
inline constexpr Gs1UiActionType GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE =
    GS1_GAMEPLAY_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;
inline constexpr Gs1UiActionType GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE =
    GS1_GAMEPLAY_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE;
inline constexpr Gs1UiActionType GS1_UI_ACTION_CLAIM_TECHNOLOGY_NODE = GS1_GAMEPLAY_ACTION_CLAIM_TECHNOLOGY_NODE;
inline constexpr Gs1UiActionType GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB =
    GS1_GAMEPLAY_ACTION_SELECT_TECH_TREE_FACTION_TAB;
inline constexpr Gs1UiActionType GS1_UI_ACTION_CLOSE_PHONE_PANEL = GS1_GAMEPLAY_ACTION_CLOSE_PHONE_PANEL;
inline constexpr Gs1UiActionType GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR =
    GS1_GAMEPLAY_ACTION_OPEN_SITE_PROTECTION_SELECTOR;
inline constexpr Gs1UiActionType GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI =
    GS1_GAMEPLAY_ACTION_CLOSE_SITE_PROTECTION_UI;
inline constexpr Gs1UiActionType GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE =
    GS1_GAMEPLAY_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
inline constexpr Gs1UiActionType GS1_UI_ACTION_END_SITE_MODIFIER = GS1_GAMEPLAY_ACTION_END_SITE_MODIFIER;

enum Gs1ProgressionViewId : std::uint8_t
{
    GS1_PROGRESSION_VIEW_NONE = 0,
    GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE = 1
};

enum Gs1SiteActionKind : std::uint8_t
{
    GS1_SITE_ACTION_NONE = 0,
    GS1_SITE_ACTION_PLANT = 1,
    GS1_SITE_ACTION_BUILD = 2,
    GS1_SITE_ACTION_REPAIR = 3,
    GS1_SITE_ACTION_WATER = 4,
    GS1_SITE_ACTION_CLEAR_BURIAL = 5,
    GS1_SITE_ACTION_CRAFT = 6,
    GS1_SITE_ACTION_DRINK = 7,
    GS1_SITE_ACTION_EAT = 8,
    GS1_SITE_ACTION_HARVEST = 9,
    GS1_SITE_ACTION_EXCAVATE = 10
};

enum Gs1SiteActionRequestFlags : std::uint8_t
{
    GS1_SITE_ACTION_REQUEST_FLAG_NONE = 0,
    GS1_SITE_ACTION_REQUEST_FLAG_HAS_PRIMARY_SUBJECT = 1u << 0,
    GS1_SITE_ACTION_REQUEST_FLAG_HAS_SECONDARY_SUBJECT = 1u << 1,
    GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM = 1u << 2,
    GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION = 1u << 3
};

enum Gs1SiteActionCancelFlags : std::uint32_t
{
    GS1_SITE_ACTION_CANCEL_FLAG_NONE = 0,
    GS1_SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION = 1u << 0,
    GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE = 1u << 1
};

enum Gs1SiteActionPresentationFlags : std::uint8_t
{
    GS1_SITE_ACTION_PRESENTATION_FLAG_NONE = 0,
    GS1_SITE_ACTION_PRESENTATION_FLAG_ACTIVE = 1u << 0,
    GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR = 1u << 1
};

enum Gs1ProjectionMode : std::uint8_t
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

enum Gs1TaskPresentationListKind : std::uint8_t
{
    GS1_TASK_PRESENTATION_LIST_VISIBLE = 0,
    GS1_TASK_PRESENTATION_LIST_ACCEPTED = 1,
    GS1_TASK_PRESENTATION_LIST_PENDING_CLAIM = 2,
    GS1_TASK_PRESENTATION_LIST_CLAIMED = 3
};

enum Gs1InventoryContainerKind : std::uint8_t
{
    GS1_INVENTORY_CONTAINER_WORKER_PACK = 0,
    GS1_INVENTORY_CONTAINER_DEVICE_STORAGE = 1
};

enum Gs1InventoryViewEventKind : std::uint8_t
{
    GS1_INVENTORY_VIEW_EVENT_NONE = 0,
    GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT = 1,
    GS1_INVENTORY_VIEW_EVENT_CLOSE = 2
};

enum Gs1PhoneListingPresentationKind : std::uint8_t
{
    GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM = 0,
    GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM = 1,
    GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR = 2,
    GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE = 3
};

enum Gs1PhonePanelSection : std::uint8_t
{
    GS1_PHONE_PANEL_SECTION_HOME = 0,
    GS1_PHONE_PANEL_SECTION_TASKS = 1,
    GS1_PHONE_PANEL_SECTION_BUY = 2,
    GS1_PHONE_PANEL_SECTION_SELL = 3,
    GS1_PHONE_PANEL_SECTION_HIRE = 4,
    GS1_PHONE_PANEL_SECTION_CART = 5
};

inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_OPEN = 1u << 0;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE = 1u << 1;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_TASKS_BADGE = 1u << 2;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_BUY_BADGE = 1u << 3;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_SELL_BADGE = 1u << 4;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_HIRE_BADGE = 1u << 5;

enum Gs1SiteProtectionOverlayMode : std::uint8_t
{
    GS1_SITE_PROTECTION_OVERLAY_NONE = 0,
    GS1_SITE_PROTECTION_OVERLAY_WIND = 1,
    GS1_SITE_PROTECTION_OVERLAY_HEAT = 2,
    GS1_SITE_PROTECTION_OVERLAY_DUST = 3,
    GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION = 4
};

enum Gs1NotificationKind : std::uint8_t
{
    GS1_NOTIFICATION_KIND_INFO = 0,
    GS1_NOTIFICATION_KIND_WARNING = 1,
    GS1_NOTIFICATION_KIND_ERROR = 2,
    GS1_NOTIFICATION_KIND_TASK = 3,
    GS1_NOTIFICATION_KIND_SITE_RESULT = 4
};

enum Gs1OneShotCueKind : std::uint8_t
{
    GS1_ONE_SHOT_CUE_NONE = 0,
    GS1_ONE_SHOT_CUE_ACTION_COMPLETED = 1,
    GS1_ONE_SHOT_CUE_ACTION_FAILED = 2,
    GS1_ONE_SHOT_CUE_HAZARD_PEAK = 3,
    GS1_ONE_SHOT_CUE_SITE_COMPLETED = 4,
    GS1_ONE_SHOT_CUE_SITE_FAILED = 5,
    GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED = 6,
    GS1_ONE_SHOT_CUE_CAMPAIGN_UNLOCKED = 7,
    GS1_ONE_SHOT_CUE_CRAFT_OUTPUT_STORED = 8
};

enum Gs1SiteAttemptResult : std::uint8_t
{
    GS1_SITE_ATTEMPT_RESULT_NONE = 0,
    GS1_SITE_ATTEMPT_RESULT_COMPLETED = 1,
    GS1_SITE_ATTEMPT_RESULT_FAILED = 2
};

enum Gs1RuntimeProfileSystemId : std::uint8_t
{
    GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW = 0,
    GS1_RUNTIME_PROFILE_SYSTEM_LOADOUT_PLANNER = 1,
    GS1_RUNTIME_PROFILE_SYSTEM_FACTION_REPUTATION = 2,
    GS1_RUNTIME_PROFILE_SYSTEM_TECHNOLOGY = 3,
    GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW = 4,
    GS1_RUNTIME_PROFILE_SYSTEM_MODIFIER = 5,
    GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT = 6,
    GS1_RUNTIME_PROFILE_SYSTEM_ACTION_EXECUTION = 7,
    GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE = 8,
    GS1_RUNTIME_PROFILE_SYSTEM_WORKER_CONDITION = 9,
    GS1_RUNTIME_PROFILE_SYSTEM_CAMP_DURABILITY = 10,
    GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_MAINTENANCE = 11,
    GS1_RUNTIME_PROFILE_SYSTEM_DEVICE_SUPPORT = 12,
    GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY = 13,
    GS1_RUNTIME_PROFILE_SYSTEM_INVENTORY = 14,
    GS1_RUNTIME_PROFILE_SYSTEM_CRAFT = 15,
    GS1_RUNTIME_PROFILE_SYSTEM_TASK_BOARD = 16,
    GS1_RUNTIME_PROFILE_SYSTEM_ECONOMY_PHONE = 17,
    GS1_RUNTIME_PROFILE_SYSTEM_PLACEMENT_VALIDATION = 18,
    GS1_RUNTIME_PROFILE_SYSTEM_FAILURE_RECOVERY = 19,
    GS1_RUNTIME_PROFILE_SYSTEM_SITE_COMPLETION = 20,
    GS1_RUNTIME_PROFILE_SYSTEM_PHONE_PANEL = 21,
    GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_TIME = 22,
    GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME = 23,
    GS1_RUNTIME_PROFILE_SYSTEM_COUNT = 24
};

struct Gs1RuntimeCreateDesc
{
    std::uint32_t struct_size;
    std::uint32_t api_version;
    double fixed_step_seconds;
    const char* project_config_root_utf8;
    const char* adapter_config_json_utf8;
};

struct Gs1RuntimeTimingStats
{
    std::uint64_t sample_count;
    double last_elapsed_ms;
    double total_elapsed_ms;
    double max_elapsed_ms;
};

struct Gs1RuntimeProfileSystemStats
{
    Gs1RuntimeProfileSystemId system_id;
    std::uint8_t enabled;
    std::uint16_t reserved0;
    std::uint32_t reserved1;
    Gs1RuntimeTimingStats run_timing;
    Gs1RuntimeTimingStats message_timing;
};

struct Gs1RuntimeProfilingSnapshot
{
    std::uint32_t struct_size;
    std::uint32_t system_count;
    Gs1RuntimeTimingStats phase1_timing;
    Gs1RuntimeTimingStats phase2_timing;
    Gs1RuntimeTimingStats fixed_step_timing;
    Gs1RuntimeProfileSystemStats systems[GS1_RUNTIME_PROFILE_SYSTEM_COUNT];
};

struct Gs1GameplayAction
{
    Gs1GameplayActionType type;
    std::uint32_t target_id;
    std::uint64_t arg0;
    std::uint64_t arg1;
};

using Gs1UiAction = Gs1GameplayAction;

struct Gs1SiteMoveDirectionCommand
{
    float world_move_x;
    float world_move_y;
    float world_move_z;
};

struct Gs1SiteActionRequestCommand
{
    Gs1SiteActionKind action_kind;
    std::uint8_t flags;
    std::uint16_t quantity;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    std::uint32_t primary_subject_id;
    std::uint32_t secondary_subject_id;
    std::uint32_t item_id;
};

struct Gs1SiteActionCancelCommand
{
    std::uint32_t action_id;
    std::uint32_t flags;
    std::uint64_t reserved0;
    std::uint64_t reserved1;
};

struct Gs1SiteStorageViewRequest
{
    std::uint32_t storage_id;
    Gs1InventoryViewEventKind event_kind;
    std::uint8_t reserved0[3];
    std::uint64_t reserved1;
    std::uint64_t reserved2;
};

struct Gs1SiteContextRequestCommand
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint32_t flags;
    std::uint32_t reserved0;
    std::uint64_t reserved1;
};

struct Gs1SiteInventorySlotTapCommand
{
    std::uint32_t storage_id;
    std::uint32_t item_instance_id;
    std::uint16_t slot_index;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t reserved0;
    std::uint32_t companion_storage_id;
    std::uint32_t reserved1;
};

struct Gs1Phase1Request
{
    std::uint32_t struct_size;
    double real_delta_seconds;
};

struct Gs1Phase1Result
{
    std::uint32_t struct_size;
    std::uint32_t fixed_steps_executed;
    std::uint32_t processed_host_message_count;
};

struct Gs1Phase2Request
{
    std::uint32_t struct_size;
};

struct Gs1Phase2Result
{
    std::uint32_t struct_size;
    std::uint32_t processed_host_message_count;
    std::uint32_t reserved0;
    std::uint32_t reserved1;
    std::uint32_t reserved2;
};

inline constexpr std::uint8_t GS1_INVENTORY_STORAGE_FLAG_DELIVERY_BOX = 1U << 0U;
inline constexpr std::uint8_t GS1_INVENTORY_STORAGE_FLAG_RETRIEVAL_ONLY = 1U << 1U;

inline constexpr std::uint8_t GS1_PLACEMENT_PREVIEW_TILE_FLAG_OCCUPIED = 1U << 0U;
inline constexpr std::uint8_t GS1_PLACEMENT_PREVIEW_TILE_FLAG_PLANT = 1U << 1U;
inline constexpr std::uint8_t GS1_PLACEMENT_PREVIEW_TILE_FLAG_STRUCTURE = 1U << 2U;

inline constexpr std::uint8_t GS1_SITE_MODIFIER_FLAG_TIMED = 1U << 0U;

inline constexpr std::uint8_t GS1_SITE_PLANT_VISUAL_FLAG_HAS_AURA = 1U << 0U;
inline constexpr std::uint8_t GS1_SITE_PLANT_VISUAL_FLAG_HAS_WIND_PROJECTION = 1U << 1U;
inline constexpr std::uint8_t GS1_SITE_PLANT_VISUAL_FLAG_GROWABLE = 1U << 2U;

inline constexpr std::uint8_t GS1_SITE_DEVICE_VISUAL_FLAG_HAS_STORAGE = 1U << 0U;
inline constexpr std::uint8_t GS1_SITE_DEVICE_VISUAL_FLAG_IS_CRAFTING_STATION = 1U << 1U;
inline constexpr std::uint8_t GS1_SITE_DEVICE_VISUAL_FLAG_FIXED_INTEGRITY = 1U << 2U;

GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1RuntimeCreateDesc, 32U);
GS1_ASSERT_TRIVIAL_SCHEMA(Gs1RuntimeTimingStats);
GS1_ASSERT_TRIVIAL_SCHEMA(Gs1RuntimeProfileSystemStats);
GS1_ASSERT_TRIVIAL_SCHEMA(Gs1RuntimeProfilingSnapshot);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1GameplayAction, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1SiteMoveDirectionCommand, 12U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1SiteActionRequestCommand, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1SiteActionCancelCommand, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1SiteStorageViewRequest, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1SiteContextRequestCommand, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1SiteInventorySlotTapCommand, 20U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1Phase1Request, 16U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1Phase1Result, 12U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1Phase2Request, 4U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1Phase2Result, 20U);

#undef GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT
#undef GS1_ASSERT_TRIVIAL_SCHEMA
