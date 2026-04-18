#include "runtime/game_runtime.h"
#include "content/defs/item_defs.h"
#include "site/inventory_storage.h"
#include "site/site_world_access.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{
using gs1::GameMessage;
using gs1::GameMessageType;
using gs1::GameRuntime;
using gs1::InventoryItemUseRequestedMessage;
using gs1::PhoneListingPurchaseRequestedMessage;
using gs1::SelectDeploymentSiteMessage;
using gs1::SiteId;
using gs1::StartNewCampaignMessage;
using gs1::StartSiteAttemptMessage;
using gs1::TaskAcceptRequestedMessage;
using gs1::TileCoord;
}

namespace gs1
{
struct GameRuntimeProjectionTestAccess
{
    static std::optional<CampaignState>& campaign(GameRuntime& runtime)
    {
        return runtime.campaign_;
    }

    static std::optional<SiteRunState>& active_site_run(GameRuntime& runtime)
    {
        return runtime.active_site_run_;
    }

    static void mark_tile_dirty(GameRuntime& runtime, TileCoord coord)
    {
        runtime.mark_site_tile_projection_dirty(coord);
    }

    static void mark_all_tiles_dirty(GameRuntime& runtime)
    {
        runtime.mark_site_projection_update_dirty(1ULL << 0);
    }

    static void flush_projection(GameRuntime& runtime)
    {
        runtime.flush_site_presentation_if_dirty();
    }
};
}  // namespace gs1

namespace
{

GameMessage make_start_campaign_message()
{
    GameMessage message {};
    message.type = GameMessageType::StartNewCampaign;
    message.set_payload(StartNewCampaignMessage {42ULL, 30U});
    return message;
}

GameMessage make_start_site_attempt_message(std::uint32_t site_id)
{
    GameMessage message {};
    message.type = GameMessageType::StartSiteAttempt;
    message.set_payload(StartSiteAttemptMessage {site_id});
    return message;
}

GameMessage make_select_site_message(std::uint32_t site_id)
{
    GameMessage message {};
    message.type = GameMessageType::SelectDeploymentSite;
    message.set_payload(SelectDeploymentSiteMessage {site_id});
    return message;
}

std::vector<Gs1EngineMessage> drain_engine_messages(GameRuntime& runtime)
{
    std::vector<Gs1EngineMessage> messages {};
    Gs1EngineMessage message {};
    while (runtime.pop_engine_message(message) == GS1_STATUS_OK)
    {
        messages.push_back(message);
    }
    return messages;
}

Gs1HostEvent make_site_action_request_event(
    Gs1SiteActionKind action_kind,
    TileCoord target_tile,
    std::uint32_t primary_subject_id,
    std::uint32_t item_id = 0U,
    std::uint8_t flags = 0U)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_REQUEST;
    event.payload.site_action_request.action_kind = action_kind;
    event.payload.site_action_request.flags = flags != 0U
        ? flags
        : (item_id != 0U
            ? GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM
            : GS1_SITE_ACTION_REQUEST_FLAG_HAS_PRIMARY_SUBJECT);
    event.payload.site_action_request.quantity = 1U;
    event.payload.site_action_request.target_tile_x = target_tile.x;
    event.payload.site_action_request.target_tile_y = target_tile.y;
    event.payload.site_action_request.primary_subject_id = primary_subject_id;
    event.payload.site_action_request.item_id = item_id;
    return event;
}

Gs1HostEvent make_site_context_request_event(TileCoord target_tile)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_CONTEXT_REQUEST;
    event.payload.site_context_request.tile_x = target_tile.x;
    event.payload.site_context_request.tile_y = target_tile.y;
    event.payload.site_context_request.flags = 0U;
    return event;
}

Gs1HostEvent make_site_action_cancel_event(std::uint32_t flags)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_CANCEL;
    event.payload.site_action_cancel.action_id = 0U;
    event.payload.site_action_cancel.flags = flags;
    return event;
}

std::uint64_t pack_inventory_use_arg(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t slot_index,
    std::uint32_t quantity)
{
    return static_cast<std::uint64_t>(container_kind) |
        (static_cast<std::uint64_t>(slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 16U);
}

std::uint64_t pack_inventory_transfer_arg(
    Gs1InventoryContainerKind source_container_kind,
    std::uint32_t source_slot_index,
    Gs1InventoryContainerKind destination_container_kind,
    std::uint32_t quantity)
{
    return static_cast<std::uint64_t>(source_container_kind) |
        (static_cast<std::uint64_t>(source_slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(destination_container_kind) << 16U) |
        (static_cast<std::uint64_t>(quantity & 0xffffU) << 24U);
}

std::uint64_t pack_inventory_transfer_owner_arg(
    std::uint32_t source_owner_id,
    std::uint32_t destination_owner_id)
{
    return static_cast<std::uint64_t>(source_owner_id) |
        (static_cast<std::uint64_t>(destination_owner_id) << 32U);
}

Gs1HostEvent make_ui_action_event(const Gs1UiAction& action)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    return event;
}

Gs1HostEvent make_storage_view_event(
    std::uint32_t storage_id,
    Gs1InventoryViewEventKind event_kind)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_STORAGE_VIEW;
    event.payload.site_storage_view.storage_id = storage_id;
    event.payload.site_storage_view.event_kind = event_kind;
    return event;
}

Gs1HostEvent make_move_direction_event(float world_move_x, float world_move_y, float world_move_z)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_MOVE_DIRECTION;
    event.payload.site_move_direction.world_move_x = world_move_x;
    event.payload.site_move_direction.world_move_y = world_move_y;
    event.payload.site_move_direction.world_move_z = world_move_z;
    return event;
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds, Gs1Phase1Result& out_result)
{
    Gs1Phase1Request request {};
    request.struct_size = sizeof(Gs1Phase1Request);
    request.real_delta_seconds = real_delta_seconds;

    out_result = {};
    assert(runtime.run_phase1(request, out_result) == GS1_STATUS_OK);
}

void run_phase1(GameRuntime& runtime, double real_delta_seconds)
{
    Gs1Phase1Result result {};
    run_phase1(runtime, real_delta_seconds, result);
}

void set_tile_sand_burial(gs1::SiteRunState& site_run, TileCoord coord, float sand_burial)
{
    auto ecology = gs1::site_world_access::tile_ecology(site_run, coord);
    ecology.sand_burial = sand_burial;
    gs1::site_world_access::set_tile_ecology(site_run, coord, ecology);
}

std::vector<Gs1EngineMessage> flush_tile_delta_for(
    GameRuntime& runtime,
    TileCoord coord,
    float sand_burial)
{
    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    set_tile_sand_burial(site_run, coord, sand_burial);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, coord);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    return drain_engine_messages(runtime);
}

std::vector<const Gs1EngineMessage*> collect_messages_of_type(
    const std::vector<Gs1EngineMessage>& messages,
    Gs1EngineMessageType type)
{
    std::vector<const Gs1EngineMessage*> matches {};
    for (const auto& message : messages)
    {
        if (message.type == type)
        {
            matches.push_back(&message);
        }
    }
    return matches;
}

std::vector<const Gs1EngineMessage*> collect_inventory_slot_messages(
    const std::vector<Gs1EngineMessage>& messages)
{
    return collect_messages_of_type(messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT);
}

const Gs1EngineMessage* find_inventory_storage_message(
    const std::vector<Gs1EngineMessage>& messages,
    Gs1InventoryContainerKind container_kind,
    TileCoord tile)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageInventoryStorageData>();
        if (payload.container_kind == container_kind &&
            payload.tile_x == tile.x &&
            payload.tile_y == tile.y)
        {
            return &message;
        }
    }

    return nullptr;
}

const Gs1EngineMessage* find_inventory_slot_message(
    const std::vector<Gs1EngineMessage>& messages,
    Gs1InventoryContainerKind container_kind,
    std::uint32_t storage_id,
    std::uint16_t slot_index)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageInventorySlotData>();
        if (payload.container_kind == container_kind &&
            payload.storage_id == storage_id &&
            payload.slot_index == slot_index)
        {
            return &message;
        }
    }

    return nullptr;
}

bool contains_phone_listing_message(
    const std::vector<Gs1EngineMessage>& messages,
    Gs1EngineMessageType message_type,
    std::uint32_t listing_id)
{
    return std::any_of(messages.begin(), messages.end(), [&](const Gs1EngineMessage& message) {
        return message.type == message_type &&
            message.payload_as<Gs1EngineMessagePhoneListingData>().listing_id == listing_id;
    });
}

std::uint32_t starter_storage_owner_id(const gs1::SiteRunState& site_run)
{
    return static_cast<std::uint32_t>(site_run.site_world->device_entity_id(site_run.camp.starter_storage_tile));
}

std::uint32_t starter_storage_id(gs1::SiteRunState& site_run)
{
    return gs1::inventory_storage::storage_id_for_container(
        site_run,
        gs1::inventory_storage::starter_storage_container(site_run));
}

std::uint32_t delivery_box_owner_id(const gs1::SiteRunState& site_run)
{
    return static_cast<std::uint32_t>(site_run.site_world->device_entity_id(site_run.camp.delivery_box_tile));
}

std::uint32_t delivery_box_id(gs1::SiteRunState& site_run)
{
    return gs1::inventory_storage::storage_id_for_container(
        site_run,
        gs1::inventory_storage::delivery_box_container(site_run));
}

TileCoord starter_workbench_tile(const gs1::SiteRunState& site_run)
{
    return TileCoord {site_run.camp.camp_anchor_tile.x + 1, site_run.camp.camp_anchor_tile.y};
}

std::uint32_t starter_workbench_id(gs1::SiteRunState& site_run)
{
    const auto tile = starter_workbench_tile(site_run);
    const auto device_entity_id = site_run.site_world->device_entity_id(tile);
    return gs1::inventory_storage::storage_id_for_container(
        site_run,
        gs1::inventory_storage::find_device_storage_container(site_run, device_entity_id));
}

std::uint16_t find_starter_storage_slot_index(
    gs1::SiteRunState& site_run,
    gs1::ItemId item_id,
    std::uint32_t quantity)
{
    const auto container = gs1::inventory_storage::starter_storage_container(site_run);
    const auto slot_count = gs1::inventory_storage::slot_count_in_container(site_run, container);
    for (std::uint32_t slot_index = 0U; slot_index < slot_count; ++slot_index)
    {
        const auto item = gs1::inventory_storage::item_entity_for_slot(site_run, container, slot_index);
        const auto* stack = gs1::inventory_storage::stack_data(site_run, item);
        if (stack != nullptr && stack->item_id == item_id && stack->quantity == quantity)
        {
            return static_cast<std::uint16_t>(slot_index);
        }
    }

    return std::numeric_limits<std::uint16_t>::max();
}

std::uint16_t find_delivery_box_slot_index(
    gs1::SiteRunState& site_run,
    gs1::ItemId item_id,
    std::uint32_t quantity)
{
    const auto container = gs1::inventory_storage::delivery_box_container(site_run);
    const auto slot_count = gs1::inventory_storage::slot_count_in_container(site_run, container);
    for (std::uint32_t slot_index = 0U; slot_index < slot_count; ++slot_index)
    {
        const auto item = gs1::inventory_storage::item_entity_for_slot(site_run, container, slot_index);
        const auto* stack = gs1::inventory_storage::stack_data(site_run, item);
        if (stack != nullptr && stack->item_id == item_id && stack->quantity == quantity)
        {
            return static_cast<std::uint16_t>(slot_index);
        }
    }

    return std::numeric_limits<std::uint16_t>::max();
}

bool contains_ui_element_text(
    const std::vector<Gs1EngineMessage>& messages,
    const char* expected_text)
{
    for (const auto& message : messages)
    {
        if (message.type != GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT)
        {
            continue;
        }

        const auto& payload = message.payload_as<Gs1EngineMessageUiElementData>();
        if (std::strcmp(payload.text, expected_text) == 0)
        {
            return true;
        }
    }

    return false;
}

}  // namespace

int main()
{
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime runtime {create_desc};

    assert(runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::campaign(runtime).has_value());

    const auto campaign_site_id = gs1::GameRuntimeProjectionTestAccess::campaign(runtime)->sites.front().site_id.value;
    drain_engine_messages(runtime);
    assert(runtime.handle_message(make_select_site_message(campaign_site_id)) == GS1_STATUS_OK);
    const auto loadout_ui_messages = drain_engine_messages(runtime);
    assert(contains_ui_element_text(loadout_ui_messages, "Water x2"));
    assert(contains_ui_element_text(loadout_ui_messages, "Wind Reed Seeds x8"));
    assert(contains_ui_element_text(loadout_ui_messages, "Wood x6"));

    const auto first_site_id = campaign_site_id;
    assert(runtime.handle_message(make_start_site_attempt_message(first_site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).has_value());

    auto& bootstrap_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    assert(gs1::site_world_access::width(bootstrap_site_run) == 32U);
    assert(gs1::site_world_access::height(bootstrap_site_run) == 32U);
    assert(bootstrap_site_run.inventory.worker_pack_slots.size() == 8U);
    assert(bootstrap_site_run.inventory.worker_pack_slots[0].occupied);
    assert(bootstrap_site_run.inventory.worker_pack_slots[0].item_id.value == 1U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               bootstrap_site_run,
               gs1::inventory_storage::starter_storage_container(bootstrap_site_run),
               gs1::ItemId {gs1::k_item_wind_reed_seed_bundle}) == 8U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               bootstrap_site_run,
               gs1::inventory_storage::starter_storage_container(bootstrap_site_run),
               gs1::ItemId {gs1::k_item_wood_bundle}) == 6U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               bootstrap_site_run,
               gs1::inventory_storage::starter_storage_container(bootstrap_site_run),
               gs1::ItemId {gs1::k_item_iron_bundle}) == 4U);
    assert(bootstrap_site_run.task_board.visible_tasks.size() == 1U);
    assert(bootstrap_site_run.economy.money == 45);
    assert(bootstrap_site_run.economy.available_phone_listings.size() >= 11U);

    const auto bootstrap_messages = drain_engine_messages(runtime);
    const auto storage_messages =
        collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT);
    const auto weather_messages =
        collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE);
    assert(!collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT).empty());
    assert(storage_messages.size() == 4U);
    assert(weather_messages.size() == 1U);
    assert(!collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT).empty());
    const auto bootstrap_phone_panel_messages =
        collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE);
    assert(!bootstrap_phone_panel_messages.empty());
    assert(!collect_messages_of_type(bootstrap_messages, GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT).empty());
    {
        const auto& phone_panel_payload =
            bootstrap_phone_panel_messages.front()->payload_as<Gs1EngineMessagePhonePanelData>();
        assert(phone_panel_payload.active_section == GS1_PHONE_PANEL_SECTION_MARKETPLACE);
        assert(phone_panel_payload.buy_listing_count >= 9U);
    }
    {
        const auto& weather_payload =
            weather_messages.front()->payload_as<Gs1EngineMessageWeatherData>();
        assert(weather_payload.heat == 0.0f);
        assert(weather_payload.wind == 0.0f);
        assert(weather_payload.dust == 0.0f);
        assert(weather_payload.wind_direction_degrees == 0.0f);
        assert(weather_payload.event_phase == GS1_WEATHER_EVENT_PHASE_NONE);
    }
    {
        const auto* starter_storage_message = find_inventory_storage_message(
            bootstrap_messages,
            GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
            bootstrap_site_run.camp.starter_storage_tile);
        assert(starter_storage_message != nullptr);
        const auto& starter_payload =
            starter_storage_message->payload_as<Gs1EngineMessageInventoryStorageData>();
        assert(starter_payload.storage_id == starter_storage_id(bootstrap_site_run));
        assert(starter_payload.owner_entity_id == starter_storage_owner_id(bootstrap_site_run));
        assert(starter_payload.slot_count == 10U);
    }
    {
        const auto* delivery_box_message = find_inventory_storage_message(
            bootstrap_messages,
            GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
            bootstrap_site_run.camp.delivery_box_tile);
        assert(delivery_box_message != nullptr);
        const auto& delivery_payload =
            delivery_box_message->payload_as<Gs1EngineMessageInventoryStorageData>();
        assert(delivery_payload.storage_id == delivery_box_id(bootstrap_site_run));
        assert(delivery_payload.owner_entity_id == delivery_box_owner_id(bootstrap_site_run));
        assert((delivery_payload.flags & GS1_INVENTORY_STORAGE_FLAG_DELIVERY_BOX) != 0U);
        assert((delivery_payload.flags & GS1_INVENTORY_STORAGE_FLAG_RETRIEVAL_ONLY) != 0U);
        assert(delivery_payload.slot_count == 10U);
    }
    {
        const auto workbench_tile = starter_workbench_tile(bootstrap_site_run);
        const auto* workbench_storage_message = find_inventory_storage_message(
            bootstrap_messages,
            GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
            workbench_tile);
        assert(workbench_storage_message != nullptr);
        const auto& workbench_payload =
            workbench_storage_message->payload_as<Gs1EngineMessageInventoryStorageData>();
        assert(workbench_payload.storage_id == starter_workbench_id(bootstrap_site_run));
        assert(workbench_payload.owner_entity_id ==
               static_cast<std::uint32_t>(bootstrap_site_run.site_world->device_entity_id(workbench_tile)));
        assert(workbench_payload.slot_count == 8U);
    }

    GameMessage accept_task {};
    accept_task.type = GameMessageType::TaskAcceptRequested;
    accept_task.set_payload(TaskAcceptRequestedMessage {1U});
    assert(runtime.handle_message(accept_task) == GS1_STATUS_OK);
    assert(bootstrap_site_run.task_board.accepted_task_ids.size() == 1U);

    bootstrap_site_run.inventory.worker_pack_slots[4].occupied = true;
    bootstrap_site_run.inventory.worker_pack_slots[4].item_id = gs1::ItemId {gs1::k_item_medicine_pack};
    bootstrap_site_run.inventory.worker_pack_slots[4].item_quantity = 1U;
    bootstrap_site_run.inventory.worker_pack_slots[4].item_condition = 1.0f;
    bootstrap_site_run.inventory.worker_pack_slots[4].item_freshness = 1.0f;

    auto worker = gs1::site_world_access::worker_conditions(bootstrap_site_run);
    worker.health = 70.0f;
    gs1::site_world_access::set_worker_conditions(bootstrap_site_run, worker);
    GameMessage use_item {};
    use_item.type = GameMessageType::InventoryItemUseRequested;
    use_item.set_payload(InventoryItemUseRequestedMessage {
        gs1::k_item_medicine_pack,
        bootstrap_site_run.inventory.worker_pack_storage_id,
        1U,
        4U});
    assert(runtime.handle_message(use_item) == GS1_STATUS_OK);
    assert(!bootstrap_site_run.inventory.worker_pack_slots[4].occupied);
    assert(gs1::site_world_access::worker_conditions(bootstrap_site_run).health > 70.0f);

    GameMessage buy_listing {};
    buy_listing.type = GameMessageType::PhoneListingPurchaseRequested;
    buy_listing.set_payload(PhoneListingPurchaseRequestedMessage {1U, 1U, 0U});
    assert(runtime.handle_message(buy_listing) == GS1_STATUS_OK);
    assert(bootstrap_site_run.economy.money == 35);
    assert(bootstrap_site_run.economy.available_phone_listings[0].quantity == 5U);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    drain_engine_messages(runtime);

    GameMessage sell_listing {};
    sell_listing.type = GameMessageType::PhoneListingSaleRequested;
    sell_listing.set_payload(gs1::PhoneListingSaleRequestedMessage {1001U, 1U, 0U});
    assert(runtime.handle_message(sell_listing) == GS1_STATUS_OK);
    assert(runtime.handle_message(sell_listing) == GS1_STATUS_OK);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto sell_projection_messages = drain_engine_messages(runtime);
    assert(contains_phone_listing_message(
        sell_projection_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE,
        1001U));
    assert(!contains_phone_listing_message(
        sell_projection_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT,
        1001U));

    Gs1Phase1Result delivery_result {};
    run_phase1(runtime, 3.0, delivery_result);
    const auto delivered_slot_index = find_delivery_box_slot_index(
        bootstrap_site_run,
        gs1::ItemId {gs1::k_item_water_container},
        1U);
    assert(delivered_slot_index != std::numeric_limits<std::uint16_t>::max());
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto delivery_messages = drain_engine_messages(runtime);
    const auto delivery_inventory_messages = collect_inventory_slot_messages(delivery_messages);
    assert(delivery_inventory_messages.empty());

    const auto first_messages = flush_tile_delta_for(runtime, TileCoord {3, 2}, 0.25f);
    const auto first_tiles = collect_messages_of_type(first_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(first_messages.size() == 3U);
    assert(first_messages.front().type == GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT);
    assert(first_messages.back().type == GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT);
    assert(first_tiles.size() == 1U);
    {
        const auto& payload = first_tiles.front()->payload_as<Gs1EngineMessageSiteTileData>();
        assert(payload.x == 3U);
        assert(payload.y == 2U);
        assert(payload.sand_burial == 0.25f);
    }

    auto& site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(runtime).value();
    const auto repeated_coord = TileCoord {5, 4};
    set_tile_sand_burial(site_run, repeated_coord, 0.5f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, repeated_coord);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, repeated_coord);
    set_tile_sand_burial(site_run, TileCoord {1, 1}, 0.75f);
    gs1::GameRuntimeProjectionTestAccess::mark_tile_dirty(runtime, TileCoord {1, 1});
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);

    const auto second_messages = drain_engine_messages(runtime);
    const auto second_tiles = collect_messages_of_type(second_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(second_messages.size() == 4U);
    assert(second_tiles.size() == 2U);
    {
        const auto& first_tile = second_tiles[0]->payload_as<Gs1EngineMessageSiteTileData>();
        const auto& second_tile = second_tiles[1]->payload_as<Gs1EngineMessageSiteTileData>();
        assert(first_tile.x == 1U && first_tile.y == 1U);
        assert(first_tile.sand_burial == 0.75f);
        assert(second_tile.x == 5U && second_tile.y == 4U);
        assert(second_tile.sand_burial == 0.5f);
    }

    set_tile_sand_burial(site_run, TileCoord {0, 0}, 0.9f);
    gs1::GameRuntimeProjectionTestAccess::mark_all_tiles_dirty(runtime);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(runtime);
    const auto fallback_messages = drain_engine_messages(runtime);
    const auto fallback_tiles = collect_messages_of_type(fallback_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    assert(!fallback_tiles.empty());
    assert(fallback_tiles.size() == gs1::site_world_access::tile_count(site_run));

    Gs1RuntimeCreateDesc phone_panel_desc {};
    phone_panel_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    phone_panel_desc.api_version = gs1::k_api_version;
    phone_panel_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime phone_panel_runtime {phone_panel_desc};
    assert(phone_panel_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto phone_panel_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(phone_panel_runtime)->sites.front().site_id.value;
    assert(phone_panel_runtime.handle_message(make_start_site_attempt_message(phone_panel_site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(phone_panel_runtime).has_value());
    auto& phone_panel_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(phone_panel_runtime).value();
    drain_engine_messages(phone_panel_runtime);

    GameMessage buy_sellable_listing {};
    buy_sellable_listing.type = GameMessageType::PhoneListingPurchaseRequested;
    buy_sellable_listing.set_payload(PhoneListingPurchaseRequestedMessage {5U, 1U, 0U});
    assert(phone_panel_runtime.handle_message(buy_sellable_listing) == GS1_STATUS_OK);
    gs1::GameRuntimeProjectionTestAccess::flush_projection(phone_panel_runtime);
    drain_engine_messages(phone_panel_runtime);

    Gs1Phase1Result phone_panel_delivery_result {};
    run_phase1(phone_panel_runtime, 3.0, phone_panel_delivery_result);
    const auto delivered_sellable_slot_index = find_delivery_box_slot_index(
        phone_panel_site_run,
        gs1::ItemId {gs1::k_item_saltbush_seed_bundle},
        1U);
    assert(delivered_sellable_slot_index != std::numeric_limits<std::uint16_t>::max());
    gs1::GameRuntimeProjectionTestAccess::flush_projection(phone_panel_runtime);
    const auto phone_panel_delivery_messages = drain_engine_messages(phone_panel_runtime);
    assert(contains_phone_listing_message(
        phone_panel_delivery_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT,
        1000U + gs1::k_item_saltbush_seed_bundle));
    assert(!collect_messages_of_type(
        phone_panel_delivery_messages,
        GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE).empty());

    Gs1RuntimeCreateDesc ui_desc {};
    ui_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    ui_desc.api_version = gs1::k_api_version;
    ui_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime ui_runtime {ui_desc};
    assert(ui_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto ui_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(ui_runtime)->sites.front().site_id.value;
    assert(ui_runtime.handle_message(make_start_site_attempt_message(ui_site_id)) == GS1_STATUS_OK);
    auto& ui_site_run = gs1::GameRuntimeProjectionTestAccess::active_site_run(ui_runtime).value();
    drain_engine_messages(ui_runtime);

    auto open_storage_event = make_storage_view_event(
        starter_storage_id(ui_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result open_storage_result {};
    assert(ui_runtime.submit_host_events(&open_storage_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, open_storage_result);
    assert(open_storage_result.processed_host_event_count == 1U);
    const auto open_storage_messages = drain_engine_messages(ui_runtime);
    assert(!collect_messages_of_type(open_storage_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE).empty());
    assert(find_inventory_slot_message(
               open_storage_messages,
               GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
               starter_storage_id(ui_site_run),
               0U) != nullptr);

    auto ui_worker = gs1::site_world_access::worker_conditions(ui_site_run);
    ui_worker.hydration = 70.0f;
    gs1::site_world_access::set_worker_conditions(ui_site_run, ui_worker);

    Gs1UiAction transfer_action {};
    transfer_action.type = GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM;
    transfer_action.arg0 = pack_inventory_transfer_arg(
        GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
        0U,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        0U);
    transfer_action.arg1 = pack_inventory_transfer_owner_arg(
        starter_storage_id(ui_site_run),
        ui_site_run.inventory.worker_pack_storage_id);
    auto transfer_event = make_ui_action_event(transfer_action);

    Gs1Phase1Result transfer_result {};
    assert(ui_runtime.submit_host_events(&transfer_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, transfer_result);
    assert(transfer_result.processed_host_event_count == 1U);
    assert(gs1::inventory_storage::available_item_quantity_in_container(
               ui_site_run,
               gs1::inventory_storage::starter_storage_container(ui_site_run),
               gs1::ItemId {gs1::k_item_wind_reed_seed_bundle}) == 0U);
    assert(ui_site_run.inventory.worker_pack_slots[3].occupied);
    assert(ui_site_run.inventory.worker_pack_slots[3].item_id.value == gs1::k_item_wind_reed_seed_bundle);
    assert(ui_site_run.inventory.worker_pack_slots[3].item_quantity == 8U);
    const auto transfer_messages = drain_engine_messages(ui_runtime);
    const auto transfer_inventory_messages = collect_inventory_slot_messages(transfer_messages);
    assert(!transfer_inventory_messages.empty());
    assert(find_inventory_slot_message(
               transfer_messages,
               GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
               starter_storage_id(ui_site_run),
               0U) != nullptr);
    assert(find_inventory_slot_message(
               transfer_messages,
               GS1_INVENTORY_CONTAINER_WORKER_PACK,
               ui_site_run.inventory.worker_pack_storage_id,
               3U) != nullptr);

    Gs1UiAction use_action {};
    use_action.type = GS1_UI_ACTION_USE_INVENTORY_ITEM;
    use_action.target_id = ui_site_run.inventory.worker_pack_slots[0].item_instance_id;
    use_action.arg0 = pack_inventory_use_arg(
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        0U,
        1U);
    use_action.arg1 = ui_site_run.inventory.worker_pack_storage_id;
    auto use_event = make_ui_action_event(use_action);

    Gs1Phase1Result use_result {};
    assert(ui_runtime.submit_host_events(&use_event, 1U) == GS1_STATUS_OK);
    run_phase1(ui_runtime, 0.0, use_result);
    assert(use_result.processed_host_event_count == 1U);
    assert(ui_site_run.inventory.worker_pack_slots[0].occupied);
    assert(ui_site_run.inventory.worker_pack_slots[0].item_quantity == 2U);
    assert(gs1::site_world_access::worker_conditions(ui_site_run).hydration == 70.0f);
    const auto use_messages = drain_engine_messages(ui_runtime);
    const auto use_inventory_messages = collect_inventory_slot_messages(use_messages);
    assert(use_inventory_messages.empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE).empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE).empty());
    assert(!collect_messages_of_type(use_messages, GS1_ENGINE_MESSAGE_HUD_STATE).empty());

    Gs1RuntimeCreateDesc water_action_desc {};
    water_action_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    water_action_desc.api_version = gs1::k_api_version;
    water_action_desc.fixed_step_seconds = 60.0;

    GameRuntime water_action_runtime {water_action_desc};
    assert(water_action_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto water_action_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(water_action_runtime)->sites.front().site_id.value;
    assert(water_action_runtime.handle_message(make_start_site_attempt_message(water_action_site_id)) == GS1_STATUS_OK);
    drain_engine_messages(water_action_runtime);

    const TileCoord water_target {2, 2};
    auto& water_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(water_action_runtime).value();
    water_site_run.site_action.current_action_id = gs1::RuntimeActionId {77U};
    water_site_run.site_action.action_kind = gs1::ActionKind::Water;
    water_site_run.site_action.target_tile = water_target;
    water_site_run.site_action.primary_subject_id = 17U;
    water_site_run.site_action.total_action_minutes = 0.75;
    water_site_run.site_action.remaining_action_minutes = 0.75;
    water_site_run.site_action.started_at_world_minute = water_site_run.clock.world_time_minutes;

    GameMessage water_started {};
    water_started.type = GameMessageType::SiteActionStarted;
    water_started.set_payload(gs1::SiteActionStartedMessage {
        77U,
        GS1_SITE_ACTION_WATER,
        0U,
        0U,
        water_target.x,
        water_target.y,
        17U,
        0.75f});
    assert(water_action_runtime.handle_message(water_started) == GS1_STATUS_OK);
    const auto water_start_messages = drain_engine_messages(water_action_runtime);
    const auto water_action_updates =
        collect_messages_of_type(water_start_messages, GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE);
    assert(water_action_updates.size() == 1U);
    const auto& water_action_payload =
        water_action_updates.front()->payload_as<Gs1EngineMessageSiteActionData>();
    assert((water_action_payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_ACTIVE) != 0U);
    assert(water_action_payload.action_kind == GS1_SITE_ACTION_WATER);
    assert(water_action_payload.target_tile_x == water_target.x);
    assert(water_action_payload.target_tile_y == water_target.y);
    assert(water_action_payload.duration_minutes == 0.75f);
    assert(water_action_payload.progress_normalized == 0.0f);

    water_site_run.site_action = {};
    GameMessage water_completed {};
    water_completed.type = GameMessageType::SiteActionCompleted;
    water_completed.set_payload(gs1::SiteActionCompletedMessage {
        77U,
        GS1_SITE_ACTION_WATER,
        0U,
        0U,
        water_target.x,
        water_target.y,
        17U,
        0U});
    assert(water_action_runtime.handle_message(water_completed) == GS1_STATUS_OK);
    const auto water_complete_messages = drain_engine_messages(water_action_runtime);
    const auto water_clear_updates =
        collect_messages_of_type(water_complete_messages, GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE);
    assert(water_clear_updates.size() == 1U);
    {
        const auto& payload = water_clear_updates.front()->payload_as<Gs1EngineMessageSiteActionData>();
        assert((payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR) != 0U);
        assert(payload.action_kind == GS1_SITE_ACTION_NONE);
        assert(payload.action_id == 0U);
        assert(payload.duration_minutes == 0.0f);
    }

    Gs1RuntimeCreateDesc action_desc {};
    action_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    action_desc.api_version = gs1::k_api_version;
    action_desc.fixed_step_seconds = 60.0;

    GameRuntime action_runtime {action_desc};
    assert(action_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto action_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(action_runtime)->sites.front().site_id.value;
    assert(action_runtime.handle_message(make_start_site_attempt_message(action_site_id)) == GS1_STATUS_OK);
    assert(gs1::GameRuntimeProjectionTestAccess::active_site_run(action_runtime).has_value());
    drain_engine_messages(action_runtime);
    auto& action_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(action_runtime).value();
    action_site_run.inventory.worker_pack_slots[3].occupied = true;
    action_site_run.inventory.worker_pack_slots[3].item_id = gs1::ItemId {gs1::k_item_wind_reed_seed_bundle};
    action_site_run.inventory.worker_pack_slots[3].item_quantity = 2U;
    action_site_run.inventory.worker_pack_slots[3].item_condition = 1.0f;
    action_site_run.inventory.worker_pack_slots[3].item_freshness = 1.0f;

    const TileCoord action_target {5, 4};
    auto action_event = make_site_action_request_event(
        GS1_SITE_ACTION_PLANT,
        action_target,
        0U,
        gs1::k_item_wind_reed_seed_bundle);
    assert(action_runtime.submit_host_events(&action_event, 1U) == GS1_STATUS_OK);
    gs1::SiteWorld::TileEcologyData action_ecology {};
    for (int step = 0; step < 10; ++step)
    {
        run_phase1(action_runtime, 60.0);
        action_ecology = gs1::site_world_access::tile_ecology(action_site_run, action_target);
        if (action_ecology.plant_id.value == gs1::k_plant_wind_reed)
        {
            break;
        }
    }

    assert(action_ecology.plant_id.value == gs1::k_plant_wind_reed);
    assert(action_ecology.plant_density >= 0.2f);
    assert(action_site_run.inventory.worker_pack_slots[3].item_quantity == 1U);

    run_phase1(action_runtime, 0.0);
    const auto action_messages = drain_engine_messages(action_runtime);
    const auto action_tile_messages =
        collect_messages_of_type(action_messages, GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT);
    const auto projected_action_tile = std::find_if(
        action_tile_messages.begin(),
        action_tile_messages.end(),
        [&](const Gs1EngineMessage* message) {
            const auto& payload = message->payload_as<Gs1EngineMessageSiteTileData>();
            return payload.x == static_cast<std::uint32_t>(action_target.x) &&
                payload.y == static_cast<std::uint32_t>(action_target.y) &&
                payload.plant_type_id == gs1::k_plant_wind_reed &&
                payload.plant_density >= 0.2f;
        });
    assert(projected_action_tile != action_tile_messages.end());

    Gs1RuntimeCreateDesc placement_preview_desc {};
    placement_preview_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    placement_preview_desc.api_version = gs1::k_api_version;
    placement_preview_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime placement_preview_runtime {placement_preview_desc};
    assert(placement_preview_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto placement_preview_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(placement_preview_runtime)->sites.front().site_id.value;
    assert(placement_preview_runtime.handle_message(make_start_site_attempt_message(placement_preview_site_id)) == GS1_STATUS_OK);
    auto& placement_preview_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(placement_preview_runtime).value();
    drain_engine_messages(placement_preview_runtime);

    placement_preview_site_run.inventory.worker_pack_slots[2].occupied = true;
    placement_preview_site_run.inventory.worker_pack_slots[2].item_id = gs1::ItemId {gs1::k_item_wind_reed_seed_bundle};
    placement_preview_site_run.inventory.worker_pack_slots[2].item_quantity = 2U;
    placement_preview_site_run.inventory.worker_pack_slots[2].item_condition = 1.0f;
    placement_preview_site_run.inventory.worker_pack_slots[2].item_freshness = 1.0f;

    auto placement_mode_event = make_site_action_request_event(
        GS1_SITE_ACTION_PLANT,
        TileCoord {4, 4},
        0U,
        gs1::k_item_wind_reed_seed_bundle,
        GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
            GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION);
    assert(placement_preview_runtime.submit_host_events(&placement_mode_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto placement_mode_messages = drain_engine_messages(placement_preview_runtime);
    const auto placement_preview_messages =
        collect_messages_of_type(placement_mode_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW);
    assert(placement_preview_messages.size() == 1U);
    {
        const auto& payload =
            placement_preview_messages.front()->payload_as<Gs1EngineMessagePlacementPreviewData>();
        assert(payload.tile_x == 4);
        assert(payload.tile_y == 4);
        assert(payload.item_id == gs1::k_item_wind_reed_seed_bundle);
        assert(payload.action_kind == GS1_SITE_ACTION_PLANT);
        assert(payload.footprint_width == 2U);
        assert(payload.footprint_height == 2U);
        assert((payload.flags & 1U) != 0U);
        assert((payload.flags & 2U) != 0U);
        assert(payload.blocked_mask == 0ULL);
    }

    auto blocked_tile = placement_preview_site_run.site_world->tile_at(TileCoord {5, 4});
    blocked_tile.ecology.ground_cover_type_id = 9U;
    placement_preview_site_run.site_world->set_tile(TileCoord {5, 4}, blocked_tile);

    auto cursor_update_event = make_site_context_request_event(TileCoord {5, 5});
    assert(placement_preview_runtime.submit_host_events(&cursor_update_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto cursor_update_messages = drain_engine_messages(placement_preview_runtime);
    const auto blocked_preview_messages =
        collect_messages_of_type(cursor_update_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW);
    assert(blocked_preview_messages.size() == 1U);
    {
        const auto& payload =
            blocked_preview_messages.front()->payload_as<Gs1EngineMessagePlacementPreviewData>();
        assert(payload.tile_x == 4);
        assert(payload.tile_y == 4);
        assert((payload.flags & 1U) != 0U);
        assert((payload.flags & 2U) == 0U);
        assert(payload.blocked_mask == (1ULL << 1U));
    }

    auto blocked_confirm_event = make_site_action_request_event(
        GS1_SITE_ACTION_PLANT,
        TileCoord {5, 5},
        0U,
        gs1::k_item_wind_reed_seed_bundle,
        GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM);
    assert(placement_preview_runtime.submit_host_events(&blocked_confirm_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto blocked_confirm_messages = drain_engine_messages(placement_preview_runtime);
    const auto placement_failure_messages =
        collect_messages_of_type(blocked_confirm_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE);
    assert(placement_failure_messages.size() == 1U);
    {
        const auto& payload =
            placement_failure_messages.front()->payload_as<Gs1EngineMessagePlacementFailureData>();
        assert(payload.tile_x == 4);
        assert(payload.tile_y == 4);
        assert(payload.action_kind == GS1_SITE_ACTION_PLANT);
        assert(payload.blocked_mask == (1ULL << 1U));
        assert(payload.sequence_id != 0U);
        assert((payload.flags & 1U) != 0U);
    }

    auto cancel_placement_event = make_site_action_cancel_event(GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE);
    assert(placement_preview_runtime.submit_host_events(&cancel_placement_event, 1U) == GS1_STATUS_OK);
    run_phase1(placement_preview_runtime, 0.0);
    const auto cancel_placement_messages = drain_engine_messages(placement_preview_runtime);
    const auto clear_preview_messages =
        collect_messages_of_type(cancel_placement_messages, GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW);
    assert(clear_preview_messages.size() == 1U);
    {
        const auto& payload =
            clear_preview_messages.front()->payload_as<Gs1EngineMessagePlacementPreviewData>();
        assert((payload.flags & 4U) != 0U);
        assert(payload.action_kind == GS1_SITE_ACTION_NONE);
        assert(payload.item_id == 0U);
    }

    Gs1RuntimeCreateDesc storage_walk_desc {};
    storage_walk_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    storage_walk_desc.api_version = gs1::k_api_version;
    storage_walk_desc.fixed_step_seconds = 0.1;

    GameRuntime storage_walk_runtime {storage_walk_desc};
    assert(storage_walk_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto storage_walk_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(storage_walk_runtime)->sites.front().site_id.value;
    assert(storage_walk_runtime.handle_message(make_start_site_attempt_message(storage_walk_site_id)) == GS1_STATUS_OK);
    auto& storage_walk_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(storage_walk_runtime).value();
    drain_engine_messages(storage_walk_runtime);

    auto worker_position = gs1::site_world_access::worker_position(storage_walk_site_run);
    worker_position.tile_coord = TileCoord {7, 7};
    worker_position.tile_x = 7.0f;
    worker_position.tile_y = 7.0f;
    gs1::site_world_access::set_worker_position(storage_walk_site_run, worker_position);

    auto distant_open_event = make_storage_view_event(
        starter_storage_id(storage_walk_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result distant_open_result {};
    assert(storage_walk_runtime.submit_host_events(&distant_open_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_walk_runtime, 0.1, distant_open_result);
    assert(distant_open_result.fixed_steps_executed == 1U);
    assert(distant_open_result.processed_host_event_count == 1U);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);
    assert(storage_walk_site_run.inventory.pending_device_storage_open.active);
    const auto distant_open_messages = drain_engine_messages(storage_walk_runtime);
    assert(collect_messages_of_type(
               distant_open_messages,
               GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE)
               .empty());
    assert(collect_messages_of_type(
               distant_open_messages,
               GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT)
               .empty());

    auto move_cancel_event = make_move_direction_event(1.0f, 0.0f, 0.0f);
    Gs1Phase1Result move_cancel_result {};
    assert(storage_walk_runtime.submit_host_events(&move_cancel_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_walk_runtime, 0.1, move_cancel_result);
    assert(move_cancel_result.fixed_steps_executed == 1U);
    assert(move_cancel_result.processed_host_event_count == 1U);
    assert(!storage_walk_site_run.inventory.pending_device_storage_open.active);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);
    drain_engine_messages(storage_walk_runtime);

    for (int step = 0; step < 12; ++step)
    {
        run_phase1(storage_walk_runtime, 0.1);
    }

    assert(!storage_walk_site_run.inventory.pending_device_storage_open.active);
    assert(storage_walk_site_run.inventory.opened_device_storage_id == 0U);

    Gs1RuntimeCreateDesc storage_close_desc {};
    storage_close_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    storage_close_desc.api_version = gs1::k_api_version;
    storage_close_desc.fixed_step_seconds = 1.0;

    GameRuntime storage_close_runtime {storage_close_desc};
    assert(storage_close_runtime.handle_message(make_start_campaign_message()) == GS1_STATUS_OK);
    const auto storage_close_site_id =
        gs1::GameRuntimeProjectionTestAccess::campaign(storage_close_runtime)->sites.front().site_id.value;
    assert(storage_close_runtime.handle_message(make_start_site_attempt_message(storage_close_site_id)) == GS1_STATUS_OK);
    auto& storage_close_site_run =
        gs1::GameRuntimeProjectionTestAccess::active_site_run(storage_close_runtime).value();
    drain_engine_messages(storage_close_runtime);

    auto close_worker_position = gs1::site_world_access::worker_position(storage_close_site_run);
    close_worker_position.tile_coord = TileCoord {
        storage_close_site_run.camp.starter_storage_tile.x + 1,
        storage_close_site_run.camp.starter_storage_tile.y};
    close_worker_position.tile_x = static_cast<float>(close_worker_position.tile_coord.x);
    close_worker_position.tile_y = static_cast<float>(close_worker_position.tile_coord.y);
    gs1::site_world_access::set_worker_position(storage_close_site_run, close_worker_position);

    auto close_open_event = make_storage_view_event(
        starter_storage_id(storage_close_site_run),
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    Gs1Phase1Result close_open_result {};
    assert(storage_close_runtime.submit_host_events(&close_open_event, 1U) == GS1_STATUS_OK);
    run_phase1(storage_close_runtime, 0.0, close_open_result);
    assert(close_open_result.processed_host_event_count == 1U);
    assert(storage_close_site_run.inventory.opened_device_storage_id == starter_storage_id(storage_close_site_run));
    const auto close_open_messages = drain_engine_messages(storage_close_runtime);
    const auto close_open_view_messages =
        collect_messages_of_type(close_open_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE);
    assert(!close_open_view_messages.empty());

    const auto storage_tile = storage_close_site_run.camp.starter_storage_tile;
    const auto world_width = static_cast<std::int32_t>(storage_close_site_run.site_world->width());
    const auto world_height = static_cast<std::int32_t>(storage_close_site_run.site_world->height());
    TileCoord out_of_range_tile {
        std::min(close_worker_position.tile_coord.x + 2, world_width - 1),
        close_worker_position.tile_coord.y};
    if (out_of_range_tile.x >= storage_tile.x - 1 &&
        out_of_range_tile.x <= storage_tile.x + 1 &&
        out_of_range_tile.y >= storage_tile.y - 1 &&
        out_of_range_tile.y <= storage_tile.y + 1)
    {
        out_of_range_tile = TileCoord {
            close_worker_position.tile_coord.x,
            std::min(close_worker_position.tile_coord.y + 2, world_height - 1)};
    }
    assert(
        out_of_range_tile.x < storage_tile.x - 1 ||
        out_of_range_tile.x > storage_tile.x + 1 ||
        out_of_range_tile.y < storage_tile.y - 1 ||
        out_of_range_tile.y > storage_tile.y + 1);

    auto out_of_range_position = gs1::site_world_access::worker_position(storage_close_site_run);
    out_of_range_position.tile_coord = out_of_range_tile;
    out_of_range_position.tile_x = static_cast<float>(out_of_range_tile.x);
    out_of_range_position.tile_y = static_cast<float>(out_of_range_tile.y);
    gs1::site_world_access::set_worker_position(storage_close_site_run, out_of_range_position);

    Gs1Phase1Result move_away_result {};
    run_phase1(storage_close_runtime, 1.0, move_away_result);
    assert(move_away_result.fixed_steps_executed == 1U);
    assert(move_away_result.processed_host_event_count == 0U);
    assert(storage_close_site_run.inventory.opened_device_storage_id == 0U);

    const auto close_messages = drain_engine_messages(storage_close_runtime);
    const auto close_view_messages =
        collect_messages_of_type(close_messages, GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE);
    const auto close_view_message = std::find_if(
        close_view_messages.begin(),
        close_view_messages.end(),
        [&](const Gs1EngineMessage* message) {
            const auto& payload = message->payload_as<Gs1EngineMessageInventoryViewData>();
            return payload.storage_id == starter_storage_id(storage_close_site_run) &&
                payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE;
        });
    assert(close_view_message != close_view_messages.end());

    return 0;
}
