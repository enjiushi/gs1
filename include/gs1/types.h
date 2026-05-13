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

enum Gs1HostEventType : std::uint8_t
{
    GS1_HOST_EVENT_NONE = 0,
    GS1_HOST_EVENT_UI_ACTION = 1,
    GS1_HOST_EVENT_SITE_MOVE_DIRECTION = 2,
    GS1_HOST_EVENT_SITE_ACTION_REQUEST = 3,
    GS1_HOST_EVENT_SITE_ACTION_CANCEL = 4,
    GS1_HOST_EVENT_SITE_STORAGE_VIEW = 5,
    GS1_HOST_EVENT_SITE_CONTEXT_REQUEST = 6,
    GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP = 7,
    GS1_HOST_EVENT_SITE_SCENE_READY = 8
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

enum Gs1ProgressionViewId : std::uint8_t
{
    GS1_PROGRESSION_VIEW_NONE = 0,
    GS1_PROGRESSION_VIEW_REGIONAL_MAP_TECH_TREE = 1
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

enum Gs1UiSurfaceId : std::uint16_t
{
    GS1_UI_SURFACE_NONE = 0,
    GS1_UI_SURFACE_REGIONAL_SELECTION_PANEL = 1,
    GS1_UI_SURFACE_REGIONAL_TECH_TREE_OVERLAY = 2,
    GS1_UI_SURFACE_SITE_INVENTORY_PANEL = 3,
    GS1_UI_SURFACE_SITE_PHONE_PANEL = 4,
    GS1_UI_SURFACE_SITE_OVERLAY_PANEL = 5,
    GS1_UI_SURFACE_SITE_TASK_PANEL = 6,
    GS1_UI_SURFACE_SITE_CRAFT_PANEL = 7
};

enum Gs1UiPanelId : std::uint8_t
{
    GS1_UI_PANEL_NONE = 0,
    GS1_UI_PANEL_MAIN_MENU = 1,
    GS1_UI_PANEL_REGIONAL_MAP = 2,
    GS1_UI_PANEL_REGIONAL_MAP_SELECTION = 3
};

enum Gs1UiPanelSlotId : std::uint8_t
{
    GS1_UI_PANEL_SLOT_NONE = 0,
    GS1_UI_PANEL_SLOT_PRIMARY = 1,
    GS1_UI_PANEL_SLOT_SECONDARY = 2,
    GS1_UI_PANEL_SLOT_TERTIARY = 3
};

enum Gs1UiPanelListId : std::uint8_t
{
    GS1_UI_PANEL_LIST_NONE = 0,
    GS1_UI_PANEL_LIST_REGIONAL_SITES = 1,
    GS1_UI_PANEL_LIST_REGIONAL_LOADOUT = 2
};

enum Gs1UiPanelSlotFlags : std::uint8_t
{
    GS1_UI_PANEL_SLOT_FLAG_NONE = 0,
    GS1_UI_PANEL_SLOT_FLAG_PRIMARY = 1u << 0,
    GS1_UI_PANEL_SLOT_FLAG_DISABLED = 1u << 1
};

enum Gs1UiPanelListItemFlags : std::uint8_t
{
    GS1_UI_PANEL_LIST_ITEM_FLAG_NONE = 0,
    GS1_UI_PANEL_LIST_ITEM_FLAG_SELECTED = 1u << 0,
    GS1_UI_PANEL_LIST_ITEM_FLAG_DISABLED = 1u << 1
};

enum Gs1UiPanelListActionRole : std::uint8_t
{
    GS1_UI_PANEL_LIST_ACTION_ROLE_NONE = 0,
    GS1_UI_PANEL_LIST_ACTION_ROLE_PRIMARY = 1,
    GS1_UI_PANEL_LIST_ACTION_ROLE_SECONDARY = 2
};

enum Gs1UiActionType : std::uint8_t
{
    GS1_UI_ACTION_NONE = 0,
    GS1_UI_ACTION_START_NEW_CAMPAIGN = 1,
    GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE = 2,
    GS1_UI_ACTION_START_SITE_ATTEMPT = 3,
    GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP = 4,
    GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION = 5,
    GS1_UI_ACTION_ACCEPT_TASK = 6,
    GS1_UI_ACTION_CLAIM_TASK_REWARD = 7,
    GS1_UI_ACTION_BUY_PHONE_LISTING = 8,
    GS1_UI_ACTION_SELL_PHONE_LISTING = 9,
    GS1_UI_ACTION_HIRE_CONTRACTOR = 12,
    GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE = 13,
    GS1_UI_ACTION_ADD_PHONE_LISTING_TO_CART = 14,
    GS1_UI_ACTION_REMOVE_PHONE_LISTING_FROM_CART = 15,
    GS1_UI_ACTION_CHECKOUT_PHONE_CART = 16,
    GS1_UI_ACTION_SET_PHONE_PANEL_SECTION = 17,
    GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE = 18,
    GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE = 19,
    GS1_UI_ACTION_CLAIM_TECHNOLOGY_NODE = 20,
    GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB = 21,
    GS1_UI_ACTION_CLOSE_PHONE_PANEL = 22,
    GS1_UI_ACTION_REFUND_TECHNOLOGY_NODE = 23,
    GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR = 24,
    GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI = 25,
    GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE = 26,
    GS1_UI_ACTION_END_SITE_MODIFIER = 27
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

enum Gs1SiteProtectionOverlayMode : std::uint8_t
{
    GS1_SITE_PROTECTION_OVERLAY_NONE = 0,
    GS1_SITE_PROTECTION_OVERLAY_WIND = 1,
    GS1_SITE_PROTECTION_OVERLAY_HEAT = 2,
    GS1_SITE_PROTECTION_OVERLAY_DUST = 3,
    GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION = 4
};

enum Gs1EngineMessageType : std::uint8_t
{
    GS1_ENGINE_MESSAGE_NONE = 0,
    GS1_ENGINE_MESSAGE_LOG_TEXT = 1,
    GS1_ENGINE_MESSAGE_SET_APP_STATE = 2,
    GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY = 3,

    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT = 10,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT = 11,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE = 12,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT = 13,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE = 14,
    GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT = 15,

    GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP = 16,
    GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT = 17,
    GS1_ENGINE_MESSAGE_END_UI_SETUP = 18,
    GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP = 19,

    GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT = 20,
    GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT = 21,
    GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE = 22,
    GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE = 23,
    GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE = 24,
    GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT = 25,
    GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT = 26,
    GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE = 27,
    GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT = 28,
    GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE = 29,
    GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT = 30,
    GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE = 31,
    GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT = 32,
    GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE = 33,
    GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN = 34,
    GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT = 35,
    GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END = 36,
    GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW = 37,
    GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT = 38,
    GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE = 39,
    GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE = 40,
    GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE = 41,
    GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN = 42,
    GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT = 43,
    GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT = 49,
    GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_REMOVE = 50,
    GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT = 51,
    GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_REMOVE = 52,

    GS1_ENGINE_MESSAGE_BEGIN_UI_PANEL = 53,
    GS1_ENGINE_MESSAGE_UI_PANEL_TEXT_UPSERT = 54,
    GS1_ENGINE_MESSAGE_UI_PANEL_SLOT_ACTION_UPSERT = 55,
    GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ITEM_UPSERT = 56,
    GS1_ENGINE_MESSAGE_UI_PANEL_LIST_ACTION_UPSERT = 57,
    GS1_ENGINE_MESSAGE_END_UI_PANEL = 58,
    GS1_ENGINE_MESSAGE_CLOSE_UI_PANEL = 59,

    GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW = 60,
    GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT = 61,
    GS1_ENGINE_MESSAGE_END_PROGRESSION_VIEW = 62,
    GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW = 63,
    GS1_ENGINE_MESSAGE_SET_UI_SURFACE_VISIBILITY = 64,

    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_HUD_SNAPSHOT = 65,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_UPSERT = 66,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_SITE_REMOVE = 67,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_UPSERT = 68,
    GS1_ENGINE_MESSAGE_REGIONAL_MAP_HUD_LINK_REMOVE = 69,
    GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_HUD_SNAPSHOT = 70,

    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SUMMARY_SNAPSHOT = 71,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_UPSERT = 72,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_SITE_REMOVE = 73,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_UPSERT = 74,
    GS1_ENGINE_MESSAGE_REGIONAL_SUMMARY_LINK_REMOVE = 75,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SUMMARY_SNAPSHOT = 76,

    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_SNAPSHOT = 77,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_UPSERT = 78,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_SITE_REMOVE = 79,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_UPSERT = 80,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_LINK_REMOVE = 81,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_SNAPSHOT = 82,

    GS1_ENGINE_MESSAGE_BEGIN_SITE_SCENE_SNAPSHOT = 83,
    GS1_ENGINE_MESSAGE_SITE_SCENE_TILE_UPSERT = 84,
    GS1_ENGINE_MESSAGE_SITE_SCENE_WORKER_UPDATE = 85,
    GS1_ENGINE_MESSAGE_SITE_SCENE_PLANT_VISUAL_UPSERT = 86,
    GS1_ENGINE_MESSAGE_SITE_SCENE_PLANT_VISUAL_REMOVE = 87,
    GS1_ENGINE_MESSAGE_SITE_SCENE_DEVICE_VISUAL_UPSERT = 88,
    GS1_ENGINE_MESSAGE_SITE_SCENE_DEVICE_VISUAL_REMOVE = 89,
    GS1_ENGINE_MESSAGE_END_SITE_SCENE_SNAPSHOT = 90,

    GS1_ENGINE_MESSAGE_BEGIN_SITE_INVENTORY_PANEL_SNAPSHOT = 91,
    GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_STORAGE_UPSERT = 92,
    GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_SLOT_UPSERT = 93,
    GS1_ENGINE_MESSAGE_SITE_INVENTORY_PANEL_VIEW_STATE = 94,
    GS1_ENGINE_MESSAGE_END_SITE_INVENTORY_PANEL_SNAPSHOT = 95,

    GS1_ENGINE_MESSAGE_BEGIN_SITE_PHONE_PANEL_SNAPSHOT = 96,
    GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_LISTING_UPSERT = 97,
    GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_LISTING_REMOVE = 98,
    GS1_ENGINE_MESSAGE_END_SITE_PHONE_PANEL_SNAPSHOT = 99,

    GS1_ENGINE_MESSAGE_BEGIN_SITE_TASK_PANEL_SNAPSHOT = 100,
    GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_UPSERT = 101,
    GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_REMOVE = 102,
    GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_LIST_BEGIN = 103,
    GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_UPSERT = 104,
    GS1_ENGINE_MESSAGE_END_SITE_TASK_PANEL_SNAPSHOT = 105,

    GS1_ENGINE_MESSAGE_BEGIN_SITE_SUMMARY_SNAPSHOT = 106,
    GS1_ENGINE_MESSAGE_END_SITE_SUMMARY_SNAPSHOT = 107,

    GS1_ENGINE_MESSAGE_BEGIN_SITE_HUD_SNAPSHOT = 108,
    GS1_ENGINE_MESSAGE_SITE_HUD_STORAGE_UPSERT = 109,
    GS1_ENGINE_MESSAGE_END_SITE_HUD_SNAPSHOT = 110,

    GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_SELECTION_UI_PANEL = 111,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_TEXT_UPSERT = 112,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_SLOT_ACTION_UPSERT = 113,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ITEM_UPSERT = 114,
    GS1_ENGINE_MESSAGE_REGIONAL_SELECTION_UI_PANEL_LIST_ACTION_UPSERT = 115,
    GS1_ENGINE_MESSAGE_END_REGIONAL_SELECTION_UI_PANEL = 116,
    GS1_ENGINE_MESSAGE_CLOSE_REGIONAL_SELECTION_UI_PANEL = 117,

    GS1_ENGINE_MESSAGE_HUD_STATE = 44,
    GS1_ENGINE_MESSAGE_NOTIFICATION_PUSH = 45,
    GS1_ENGINE_MESSAGE_SITE_RESULT_READY = 46,
    GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE = 47,
    GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES = 48
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

struct Gs1HostEventSiteMoveDirectionData
{
    float world_move_x;
    float world_move_y;
    float world_move_z;
};

struct Gs1HostEventSiteActionRequestData
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

struct Gs1HostEventSiteActionCancelData
{
    std::uint32_t action_id;
    std::uint32_t flags;
    std::uint64_t reserved0;
    std::uint64_t reserved1;
};

struct Gs1HostEventSiteStorageViewData
{
    std::uint32_t storage_id;
    Gs1InventoryViewEventKind event_kind;
    std::uint8_t reserved0[3];
    std::uint64_t reserved1;
    std::uint64_t reserved2;
};

struct Gs1HostEventSiteContextRequestData
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint32_t flags;
    std::uint32_t reserved0;
    std::uint64_t reserved1;
};

struct Gs1HostEventSiteInventorySlotTapData
{
    std::uint32_t storage_id;
    std::uint32_t item_instance_id;
    std::uint16_t slot_index;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t reserved0;
    std::uint32_t companion_storage_id;
    std::uint32_t reserved1;
};

struct Gs1HostEventSiteSceneReadyData
{
    std::uint64_t reserved0;
    std::uint64_t reserved1;
    std::uint64_t reserved2;
};

struct Gs1HostEventEmptyData
{
    std::uint64_t reserved0;
    std::uint64_t reserved1;
};

union Gs1HostEventPayload
{
    Gs1HostEventUiActionData ui_action;
    Gs1HostEventSiteMoveDirectionData site_move_direction;
    Gs1HostEventSiteActionRequestData site_action_request;
    Gs1HostEventSiteActionCancelData site_action_cancel;
    Gs1HostEventSiteStorageViewData site_storage_view;
    Gs1HostEventSiteContextRequestData site_context_request;
    Gs1HostEventSiteInventorySlotTapData site_inventory_slot_tap;
    Gs1HostEventSiteSceneReadyData site_scene_ready;
    Gs1HostEventEmptyData empty;
};

using Gs1HostMessageType = Gs1HostEventType;
using Gs1HostMessageUiActionData = Gs1HostEventUiActionData;
using Gs1HostMessageSiteMoveDirectionData = Gs1HostEventSiteMoveDirectionData;
using Gs1HostMessageSiteActionRequestData = Gs1HostEventSiteActionRequestData;
using Gs1HostMessageSiteActionCancelData = Gs1HostEventSiteActionCancelData;
using Gs1HostMessageSiteStorageViewData = Gs1HostEventSiteStorageViewData;
using Gs1HostMessageSiteContextRequestData = Gs1HostEventSiteContextRequestData;
using Gs1HostMessageSiteInventorySlotTapData = Gs1HostEventSiteInventorySlotTapData;
using Gs1HostMessageSiteSceneReadyData = Gs1HostEventSiteSceneReadyData;
using Gs1HostMessageEmptyData = Gs1HostEventEmptyData;
using Gs1HostMessagePayload = Gs1HostEventPayload;

struct Gs1HostMessage
{
    Gs1HostMessageType type;
    Gs1HostMessagePayload payload;
};

using Gs1HostEvent = Gs1HostMessage;

struct Gs1Phase1Request
{
    std::uint32_t struct_size;
    double real_delta_seconds;
};

struct Gs1Phase1Result
{
    std::uint32_t struct_size;
    std::uint32_t fixed_steps_executed;
    std::uint32_t runtime_messages_queued;
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
    std::uint32_t runtime_messages_queued;
    std::uint32_t reserved1;
};

struct Gs1EngineMessageLogTextData
{
    Gs1LogLevel level;
    char text[62];
};

struct Gs1EngineMessageSetAppStateData
{
    Gs1AppState app_state;
};

struct Gs1EngineMessagePresentationDirtyData
{
    std::uint32_t dirty_flags;
    std::uint32_t reserved0;
    std::uint64_t revision;
};

struct Gs1EngineMessageRegionalMapSnapshotData
{
    Gs1ProjectionMode mode;
    std::uint32_t site_count;
    std::uint32_t link_count;
    std::uint32_t selected_site_id;
};

struct Gs1EngineMessageRegionalMapSiteData
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

struct Gs1EngineMessageRegionalMapLinkData
{
    std::uint32_t from_site_id;
    std::uint32_t to_site_id;
    std::uint8_t flags;
};

struct Gs1EngineMessageUiSetupData
{
    std::uint32_t context_id;
    std::uint16_t element_count;
    Gs1UiSetupId setup_id;
    Gs1ProjectionMode mode;
    Gs1UiSetupPresentationType presentation_type;
};

struct Gs1EngineMessageCloseUiSetupData
{
    Gs1UiSetupId setup_id;
    Gs1UiSetupPresentationType presentation_type;
};

struct Gs1EngineMessageUiPanelData
{
    std::uint32_t context_id;
    Gs1UiPanelId panel_id;
    Gs1ProjectionMode mode;
    std::uint8_t text_line_count;
    std::uint8_t slot_action_count;
    std::uint8_t list_item_count;
    std::uint8_t list_action_count;
};

struct Gs1EngineMessageCloseUiPanelData
{
    Gs1UiPanelId panel_id;
    std::uint8_t reserved0[3];
};

struct Gs1EngineMessageUiPanelTextData
{
    std::uint16_t line_id;
    std::uint8_t flags;
    std::uint8_t text_kind;
    std::uint32_t primary_id;
    std::uint32_t secondary_id;
    std::uint32_t quantity;
    std::uint32_t aux_value;
    std::uint8_t reserved0[40];
};

struct Gs1EngineMessageUiPanelSlotActionData
{
    Gs1UiAction action;
    Gs1UiPanelSlotId slot_id;
    std::uint8_t flags;
    std::uint8_t label_kind;
    std::uint32_t primary_id;
    std::uint32_t secondary_id;
    std::uint32_t quantity;
    std::uint8_t reserved0[16];
};

struct Gs1EngineMessageUiPanelListItemData
{
    std::uint32_t item_id;
    Gs1UiPanelListId list_id;
    std::uint8_t flags;
    std::uint8_t primary_kind;
    std::uint8_t secondary_kind;
    std::uint32_t primary_id;
    std::uint32_t secondary_id;
    std::uint32_t quantity;
    std::int32_t map_x;
    std::int32_t map_y;
    std::uint8_t reserved0[32];
};

struct Gs1EngineMessageUiPanelListActionData
{
    Gs1UiAction action;
    std::uint32_t item_id;
    Gs1UiPanelListId list_id;
    Gs1UiPanelListActionRole role;
    std::uint8_t flags;
    std::uint8_t reserved0;
};

struct Gs1EngineMessageUiElementData
{
    Gs1UiAction action;
    std::uint32_t element_id;
    Gs1UiElementType element_type;
    std::uint8_t flags;
    std::uint8_t content_kind;
    std::uint32_t primary_id;
    std::uint32_t secondary_id;
    std::uint32_t quantity;
    std::uint8_t reserved0[12];
};

struct Gs1EngineMessageProgressionViewData
{
    std::uint32_t context_id;
    std::uint16_t entry_count;
    Gs1ProgressionViewId view_id;
    Gs1ProjectionMode mode;
};

struct Gs1EngineMessageCloseProgressionViewData
{
    Gs1ProgressionViewId view_id;
    std::uint8_t reserved0;
};

struct Gs1EngineMessageProgressionEntryData
{
    Gs1UiAction action;
    std::uint16_t entry_id;
    std::uint16_t reputation_requirement;
    std::uint16_t content_id;
    std::uint16_t tech_node_id;
    std::uint8_t faction_id;
    Gs1ProgressionEntryKind entry_kind;
    std::uint8_t flags;
    std::uint8_t content_kind;
    std::uint8_t tier_index;
    std::uint8_t reserved0[2];
};

struct Gs1EngineMessageSiteSnapshotData
{
    std::uint32_t site_id;
    std::uint32_t site_archetype_id;
    std::uint16_t width;
    std::uint16_t height;
    Gs1ProjectionMode mode;
};

struct Gs1EngineMessageSiteTileData
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

struct Gs1EngineMessageWorkerData
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

struct Gs1EngineMessageCampData
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    float durability_normalized;
    std::uint8_t flags;
};

struct Gs1EngineMessageWeatherData
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

struct Gs1EngineMessageInventoryStorageData
{
    std::uint32_t storage_id;
    std::uint32_t owner_entity_id;
    std::uint16_t slot_count;
    std::int16_t tile_x;
    std::int16_t tile_y;
    Gs1InventoryContainerKind container_kind;
    std::uint8_t flags;
};

inline constexpr std::uint8_t GS1_INVENTORY_STORAGE_FLAG_DELIVERY_BOX = 1U << 0U;
inline constexpr std::uint8_t GS1_INVENTORY_STORAGE_FLAG_RETRIEVAL_ONLY = 1U << 1U;

struct Gs1EngineMessageInventorySlotData
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

struct Gs1EngineMessageInventoryViewData
{
    std::uint32_t storage_id;
    std::uint16_t slot_count;
    Gs1InventoryViewEventKind event_kind;
    std::uint8_t flags;
};

struct Gs1EngineMessageCraftContextData
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint16_t option_count;
    std::uint16_t flags;
};

struct Gs1EngineMessageCraftContextOptionData
{
    std::uint32_t recipe_id;
    std::uint32_t output_item_id;
    std::uint16_t flags;
    std::uint16_t reserved0;
};

struct Gs1EngineMessagePlacementPreviewData
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

struct Gs1EngineMessagePlacementPreviewTileData
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

inline constexpr std::uint8_t GS1_PLACEMENT_PREVIEW_TILE_FLAG_OCCUPIED = 1U << 0U;
inline constexpr std::uint8_t GS1_PLACEMENT_PREVIEW_TILE_FLAG_PLANT = 1U << 1U;
inline constexpr std::uint8_t GS1_PLACEMENT_PREVIEW_TILE_FLAG_STRUCTURE = 1U << 2U;

struct Gs1EngineMessagePlacementFailureData
{
    std::int32_t tile_x;
    std::int32_t tile_y;
    std::uint64_t blocked_mask;
    std::uint32_t sequence_id;
    Gs1SiteActionKind action_kind;
    std::uint8_t flags;
    std::uint16_t reserved0;
};

struct Gs1EngineMessageTaskData
{
    std::uint32_t task_instance_id;
    std::uint32_t task_template_id;
    std::uint32_t publisher_faction_id;
    std::uint16_t current_progress;
    std::uint16_t target_progress;
    Gs1TaskPresentationListKind list_kind;
    std::uint8_t flags;
};

struct Gs1EngineMessagePhoneListingData
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

struct Gs1EngineMessagePhonePanelData
{
    Gs1PhonePanelSection active_section;
    std::uint8_t reserved0[3];
    std::uint32_t visible_task_count;
    std::uint32_t accepted_task_count;
    std::uint32_t completed_task_count;
    std::uint32_t claimed_task_count;
    std::uint32_t buy_listing_count;
    std::uint32_t sell_listing_count;
    std::uint32_t service_listing_count;
    std::uint32_t cart_item_count;
    std::uint32_t flags;
};

inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_OPEN = 1U << 0U;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_LAUNCHER_BADGE = 1U << 1U;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_TASKS_BADGE = 1U << 2U;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_BUY_BADGE = 1U << 3U;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_SELL_BADGE = 1U << 4U;
inline constexpr std::uint32_t GS1_PHONE_PANEL_FLAG_HIRE_BADGE = 1U << 5U;

struct Gs1EngineMessageSiteProtectionOverlayData
{
    Gs1SiteProtectionOverlayMode mode;
    std::uint8_t reserved0[3];
};

struct Gs1EngineMessageUiSurfaceVisibilityData
{
    Gs1UiSurfaceId surface_id;
    std::uint8_t visible;
    std::uint8_t reserved0;
};

struct Gs1EngineMessageSiteModifierListData
{
    Gs1ProjectionMode mode;
    std::uint8_t reserved0;
    std::uint16_t modifier_count;
};

struct Gs1EngineMessageSiteModifierData
{
    std::uint32_t modifier_id;
    std::uint16_t remaining_game_hours;
    std::uint8_t flags;
    std::uint8_t reserved0;
};

inline constexpr std::uint8_t GS1_SITE_MODIFIER_FLAG_TIMED = 1U << 0U;

struct Gs1EngineMessageSiteActionData
{
    std::uint32_t action_id;
    std::int32_t target_tile_x;
    std::int32_t target_tile_y;
    Gs1SiteActionKind action_kind;
    std::uint8_t flags;
    std::uint16_t reserved0;
    float progress_normalized;
    float duration_minutes;
};

struct Gs1EngineMessageSitePlantVisualData
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

inline constexpr std::uint8_t GS1_SITE_PLANT_VISUAL_FLAG_HAS_AURA = 1U << 0U;
inline constexpr std::uint8_t GS1_SITE_PLANT_VISUAL_FLAG_HAS_WIND_PROJECTION = 1U << 1U;
inline constexpr std::uint8_t GS1_SITE_PLANT_VISUAL_FLAG_GROWABLE = 1U << 2U;

struct Gs1EngineMessageSiteVisualRemoveData
{
    std::uint64_t visual_id;
};

struct Gs1EngineMessageSiteDeviceVisualData
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

inline constexpr std::uint8_t GS1_SITE_DEVICE_VISUAL_FLAG_HAS_STORAGE = 1U << 0U;
inline constexpr std::uint8_t GS1_SITE_DEVICE_VISUAL_FLAG_IS_CRAFTING_STATION = 1U << 1U;
inline constexpr std::uint8_t GS1_SITE_DEVICE_VISUAL_FLAG_FIXED_INTEGRITY = 1U << 2U;

struct Gs1EngineMessageHudStateData
{
    float player_health;
    float player_hydration;
    float player_nourishment;
    float player_energy;
    float player_morale;
    float current_money;
    float site_completion_normalized;
    std::uint16_t active_task_count;
    std::uint16_t warning_code;
    Gs1SiteActionKind current_action_kind;
};

struct Gs1EngineMessageCampaignResourcesData
{
    float current_money;
    std::int32_t total_reputation;
    std::int32_t village_reputation;
    std::int32_t forestry_reputation;
    std::int32_t university_reputation;
};

struct Gs1EngineMessageNotificationData
{
    std::uint32_t notification_code;
    std::uint32_t subject_id;
    std::uint32_t arg0;
    std::uint32_t arg1;
    Gs1NotificationKind kind;
    char text[39];
};

struct Gs1EngineMessageSiteResultData
{
    std::uint32_t site_id;
    Gs1SiteAttemptResult result;
    std::uint16_t newly_revealed_site_count;
};

struct Gs1EngineMessageOneShotCueData
{
    std::uint32_t subject_id;
    float world_x;
    float world_y;
    std::uint32_t arg0;
    std::uint32_t arg1;
    Gs1OneShotCueKind cue_kind;
};

using Gs1RuntimeMessageType = Gs1EngineMessageType;

struct alignas(GS1_MESSAGE_CACHE_LINE_SIZE) Gs1RuntimeMessage
{
    unsigned char payload[GS1_MESSAGE_PAYLOAD_BYTE_COUNT];
    Gs1RuntimeMessageType type;

    template <typename PayloadData>
    [[nodiscard]] PayloadData& emplace_payload() noexcept
    {
        validate_payload_type<PayloadData>();
        auto* ptr = std::construct_at(reinterpret_cast<PayloadData*>(payload), PayloadData {});
        return *std::launder(ptr);
    }

    template <typename PayloadData>
    [[nodiscard]] PayloadData& emplace_payload(const PayloadData& value) noexcept
    {
        validate_payload_type<PayloadData>();
        auto* ptr = std::construct_at(reinterpret_cast<PayloadData*>(payload), value);
        return *std::launder(ptr);
    }

    template <typename PayloadData>
    [[nodiscard]] PayloadData& payload_as() noexcept
    {
        validate_payload_type<PayloadData>();
        return *std::launder(reinterpret_cast<PayloadData*>(payload));
    }

    template <typename PayloadData>
    [[nodiscard]] const PayloadData& payload_as() const noexcept
    {
        validate_payload_type<PayloadData>();
        return *std::launder(reinterpret_cast<const PayloadData*>(payload));
    }

private:
    template <typename PayloadData>
    static constexpr void validate_payload_type() noexcept
    {
        GS1_ASSERT_TRIVIAL_SCHEMA(PayloadData);
        static_assert(sizeof(PayloadData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT, "Runtime message payload data exceeds message payload storage.");
        static_assert(alignof(PayloadData) <= alignof(Gs1RuntimeMessage), "Runtime message payload data requires stronger alignment than Gs1RuntimeMessage.");
    }
};

using Gs1EngineMessage = Gs1RuntimeMessage;

GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1RuntimeCreateDesc, 32U);
GS1_ASSERT_TRIVIAL_SCHEMA(Gs1RuntimeTimingStats);
GS1_ASSERT_TRIVIAL_SCHEMA(Gs1RuntimeProfileSystemStats);
GS1_ASSERT_TRIVIAL_SCHEMA(Gs1RuntimeProfilingSnapshot);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1UiAction, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventUiActionData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventSiteMoveDirectionData, 12U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventSiteActionRequestData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventSiteActionCancelData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventSiteStorageViewData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventSiteContextRequestData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventSiteInventorySlotTapData, 20U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventSiteSceneReadyData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostEventEmptyData, 16U);
static_assert(std::is_standard_layout_v<Gs1HostEventPayload>, "Gs1HostEventPayload must remain standard layout.");
static_assert(std::is_trivial_v<Gs1HostEventPayload>, "Gs1HostEventPayload must remain trivial.");
static_assert(std::is_trivially_copyable_v<Gs1HostEventPayload>, "Gs1HostEventPayload must remain trivially copyable.");
static_assert(sizeof(Gs1HostEventPayload) == 24U, "Gs1HostEventPayload size changed; revisit event packing.");
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1HostMessage, 32U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1Phase1Request, 16U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1Phase1Result, 16U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1Phase2Request, 4U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1Phase2Result, 20U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageLogTextData, 63U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSetAppStateData, 1U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessagePresentationDirtyData, 16U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageRegionalMapSnapshotData, 16U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageRegionalMapSiteData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageRegionalMapLinkData, 12U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageUiSetupData, 12U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageCloseUiSetupData, 2U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageUiPanelData, 12U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageCloseUiPanelData, 4U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageUiPanelTextData, 60U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageUiPanelSlotActionData, 56U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageUiPanelListItemData, 60U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageUiPanelListActionData, 32U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageUiElementData, 56U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageProgressionViewData, 8U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageCloseProgressionViewData, 2U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageProgressionEntryData, 40U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteSnapshotData, 16U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteTileData, 60U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageWorkerData, 40U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageCampData, 16U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageWeatherData, 36U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageInventoryStorageData, 16U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageInventorySlotData, 36U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageInventoryViewData, 8U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageCraftContextData, 12U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageCraftContextOptionData, 12U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessagePlacementPreviewData, 32U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessagePlacementFailureData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageTaskData, 20U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessagePhoneListingData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessagePhonePanelData, 40U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteProtectionOverlayData, 4U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageUiSurfaceVisibilityData, 4U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteModifierListData, 4U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteModifierData, 8U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteActionData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSitePlantVisualData, 32U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteVisualRemoveData, 8U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteDeviceVisualData, 32U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageHudStateData, 36U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageCampaignResourcesData, 20U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageNotificationData, 56U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageSiteResultData, 8U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1EngineMessageOneShotCueData, 24U);
GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT(Gs1RuntimeMessage, 64U);

static_assert(sizeof(Gs1EngineMessageLogTextData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSetAppStateData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessagePresentationDirtyData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageRegionalMapSnapshotData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageRegionalMapSiteData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageRegionalMapLinkData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageUiSetupData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageCloseUiSetupData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageUiPanelData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageCloseUiPanelData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageUiPanelTextData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageUiPanelSlotActionData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageUiPanelListItemData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageUiPanelListActionData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageUiElementData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageProgressionViewData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageCloseProgressionViewData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageProgressionEntryData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteSnapshotData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteTileData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageWorkerData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageCampData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageWeatherData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageInventoryStorageData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageInventorySlotData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageInventoryViewData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageCraftContextData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageCraftContextOptionData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessagePlacementPreviewData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessagePlacementPreviewTileData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessagePlacementFailureData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageTaskData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessagePhoneListingData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessagePhonePanelData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteProtectionOverlayData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageUiSurfaceVisibilityData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteModifierListData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteModifierData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteActionData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSitePlantVisualData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteVisualRemoveData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteDeviceVisualData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageHudStateData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageCampaignResourcesData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageNotificationData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageSiteResultData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1EngineMessageOneShotCueData) <= GS1_MESSAGE_PAYLOAD_BYTE_COUNT);
static_assert(sizeof(Gs1RuntimeMessage) == GS1_MESSAGE_CACHE_LINE_SIZE, "Gs1RuntimeMessage must fit exactly one cache line.");
static_assert(alignof(Gs1RuntimeMessage) == GS1_MESSAGE_CACHE_LINE_SIZE, "Gs1RuntimeMessage must be cache-line aligned.");
static_assert(offsetof(Gs1RuntimeMessage, payload) == 0U, "Gs1RuntimeMessage payload must start at byte zero.");
static_assert(offsetof(Gs1RuntimeMessage, type) == GS1_MESSAGE_PAYLOAD_BYTE_COUNT, "Gs1RuntimeMessage type must sit at the tail byte.");

#undef GS1_ASSERT_TRIVIAL_SCHEMA_LAYOUT
#undef GS1_ASSERT_TRIVIAL_SCHEMA
