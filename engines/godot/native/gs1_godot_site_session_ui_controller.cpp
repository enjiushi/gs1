#include "gs1_godot_site_session_ui_controller.h"

#include "gs1_godot_controller_context.h"

#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"

#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

using namespace godot;

namespace
{
constexpr std::uint16_t k_invalid_tile_coordinate = std::numeric_limits<std::uint16_t>::max();

template <typename Projection, typename Key, typename KeyFn>
void upsert_projection(std::vector<Projection>& projections, Projection projection, Key key, KeyFn&& key_fn)
{
    const auto existing = std::find_if(projections.begin(), projections.end(), [&](const Projection& item) {
        return key_fn(item) == key;
    });
    if (existing != projections.end())
    {
        *existing = std::move(projection);
    }
    else
    {
        projections.push_back(std::move(projection));
    }
}

void sort_inventory_slots(std::vector<Gs1RuntimeInventorySlotProjection>& slots)
{
    std::sort(slots.begin(), slots.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.storage_id != rhs.storage_id)
        {
            return lhs.storage_id < rhs.storage_id;
        }
        return lhs.slot_index < rhs.slot_index;
    });
}

void sort_inventory_storages(std::vector<Gs1RuntimeInventoryStorageProjection>& storages)
{
    std::sort(storages.begin(), storages.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.container_kind != rhs.container_kind)
        {
            return lhs.container_kind < rhs.container_kind;
        }
        return lhs.storage_id < rhs.storage_id;
    });
}

void sort_tasks(std::vector<Gs1RuntimeTaskProjection>& tasks)
{
    std::sort(tasks.begin(), tasks.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.task_instance_id < rhs.task_instance_id;
    });
}

void sort_phone_listings(std::vector<Gs1RuntimePhoneListingProjection>& listings)
{
    std::sort(listings.begin(), listings.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.listing_id < rhs.listing_id;
    });
}

void sort_modifiers(std::vector<Gs1RuntimeModifierProjection>& modifiers)
{
    std::sort(modifiers.begin(), modifiers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.modifier_id < rhs.modifier_id;
    });
}

void sort_craft_options(std::vector<Gs1RuntimeCraftContextOptionProjection>& options)
{
    std::sort(options.begin(), options.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.output_item_id != rhs.output_item_id)
        {
            return lhs.output_item_id < rhs.output_item_id;
        }
        return lhs.recipe_id < rhs.recipe_id;
    });
}

void sort_placement_preview_tiles(std::vector<Gs1RuntimePlacementPreviewTileProjection>& tiles)
{
    std::sort(tiles.begin(), tiles.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.y != rhs.y)
        {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    });
}

String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
}
}

void Gs1GodotSiteSessionUiController::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_ui_root_path", "path"), &Gs1GodotSiteSessionUiController::set_ui_root_path);
    ClassDB::bind_method(D_METHOD("get_ui_root_path"), &Gs1GodotSiteSessionUiController::get_ui_root_path);
    ClassDB::bind_method(D_METHOD("on_return_to_map_pressed"), &Gs1GodotSiteSessionUiController::on_return_to_map_pressed);
    ClassDB::bind_method(D_METHOD("on_submit_move_pressed", "x", "y", "z"), &Gs1GodotSiteSessionUiController::on_submit_move_pressed);
    ClassDB::bind_method(D_METHOD("on_submit_selected_tile_context_pressed", "flags"), &Gs1GodotSiteSessionUiController::on_submit_selected_tile_context_pressed);
    ClassDB::bind_method(D_METHOD("on_submit_site_action_cancel_pressed", "action_id", "flags"), &Gs1GodotSiteSessionUiController::on_submit_site_action_cancel_pressed);
    ClassDB::bind_method(D_METHOD("on_open_nearest_storage_pressed"), &Gs1GodotSiteSessionUiController::on_open_nearest_storage_pressed);
    ClassDB::bind_method(D_METHOD("on_close_storage_pressed"), &Gs1GodotSiteSessionUiController::on_close_storage_pressed);
    ClassDB::bind_method(D_METHOD("on_plant_selected_seed_pressed"), &Gs1GodotSiteSessionUiController::on_plant_selected_seed_pressed);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "ui_root_path"), "set_ui_root_path", "get_ui_root_path");
}

void Gs1GodotSiteSessionUiController::_ready()
{
    cache_adapter_service();
    cache_ui_references();
    wire_static_buttons();
    set_process_input(true);
}

void Gs1GodotSiteSessionUiController::_input(const Ref<InputEvent>& event)
{
    handle_input_event(event);
}

void Gs1GodotSiteSessionUiController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotSiteSessionUiController::set_ui_root_path(const NodePath& path)
{
    ui_root_path_ = path;
    ui_root_ = nullptr;
}

NodePath Gs1GodotSiteSessionUiController::get_ui_root_path() const
{
    return ui_root_path_;
}

void Gs1GodotSiteSessionUiController::cache_adapter_service()
{
    if (adapter_service_ != nullptr)
    {
        return;
    }

    adapter_service_ = gs1_resolve_adapter_service(this);
    if (adapter_service_ == nullptr)
    {
        return;
    }

    adapter_service_->subscribe_matching_messages(*this);
}

Control* Gs1GodotSiteSessionUiController::resolve_ui_root()
{
    if (ui_root_ != nullptr)
    {
        return ui_root_;
    }

    if (ui_root_path_.is_empty() || ui_root_path_ == NodePath("."))
    {
        ui_root_ = this;
        return ui_root_;
    }

    ui_root_ = Object::cast_to<Control>(get_node_or_null(ui_root_path_));
    if (ui_root_ == nullptr)
    {
        ui_root_ = this;
    }
    return ui_root_;
}

void Gs1GodotSiteSessionUiController::cache_ui_references()
{
    Control* ui_root = resolve_ui_root();
    if (ui_root == nullptr)
    {
        return;
    }

    if (site_panel_ == nullptr)
    {
        site_panel_ = Object::cast_to<Control>(ui_root->find_child("SitePanel", true, false));
    }
    if (tile_label_ == nullptr)
    {
        tile_label_ = Object::cast_to<Label>(ui_root->find_child("TileLabel", true, false));
    }
}

void Gs1GodotSiteSessionUiController::wire_static_buttons()
{
    if (buttons_wired_)
    {
        return;
    }

    Control* ui_root = resolve_ui_root();
    if (ui_root == nullptr)
    {
        return;
    }

    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("ReturnToMapButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_return_to_map_pressed));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("MoveNorthButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_submit_move_pressed).bind(0.0, 0.0, -1.0));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("MoveSouthButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_submit_move_pressed).bind(0.0, 0.0, 1.0));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("MoveWestButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_submit_move_pressed).bind(-1.0, 0.0, 0.0));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("MoveEastButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_submit_move_pressed).bind(1.0, 0.0, 0.0));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("HoverTileButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_submit_selected_tile_context_pressed).bind(0));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("CraftAtSelectedTileButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_submit_selected_tile_context_pressed).bind(0));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("CancelActionButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_submit_site_action_cancel_pressed).bind(0, SITE_ACTION_CANCEL_FLAG_CURRENT_ACTION | SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("OpenNearestStorageButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_open_nearest_storage_pressed));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("CloseStorageButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_close_storage_pressed));
    bind_button(Object::cast_to<BaseButton>(ui_root->find_child("PlantSelectedSeedButton", true, false)), callable_mp(this, &Gs1GodotSiteSessionUiController::on_plant_selected_seed_pressed));

    buttons_wired_ = true;
}

void Gs1GodotSiteSessionUiController::bind_button(BaseButton* button, const Callable& callback)
{
    if (button == nullptr || button->is_connected("pressed", callback))
    {
        return;
    }
    button->connect("pressed", callback);
}

std::size_t Gs1GodotSiteSessionUiController::site_tile_capacity(const Gs1RuntimeSiteProjection& site) const noexcept
{
    return static_cast<std::size_t>(site.width) * static_cast<std::size_t>(site.height);
}

std::optional<std::uint32_t> Gs1GodotSiteSessionUiController::site_tile_index(
    const Gs1RuntimeSiteProjection& site,
    std::uint16_t x,
    std::uint16_t y) const noexcept
{
    if (x >= site.width || y >= site.height)
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(
        static_cast<std::size_t>(y) * static_cast<std::size_t>(site.width) + static_cast<std::size_t>(x));
}

void Gs1GodotSiteSessionUiController::reset_site_state() noexcept
{
    site_state_.reset();
    pending_site_state_.reset();
    pending_inventory_storage_indices_.clear();
    pending_worker_pack_slot_indices_.clear();
    pending_opened_storage_slot_indices_.clear();
    pending_task_indices_.clear();
    pending_phone_listing_indices_.clear();
    pending_modifier_indices_.clear();
}

void Gs1GodotSiteSessionUiController::apply_site_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
        if (payload.mode == GS1_PROJECTION_MODE_DELTA && site_state_.has_value())
        {
            pending_site_state_ = site_state_;
        }
        else
        {
            pending_site_state_ = Gs1RuntimeSiteProjection {};
        }
        pending_site_state_->site_id = payload.site_id;
        pending_site_state_->site_archetype_id = payload.site_archetype_id;
        pending_site_state_->width = payload.width;
        pending_site_state_->height = payload.height;
        pending_inventory_storage_indices_.clear();
        pending_worker_pack_slot_indices_.clear();
        pending_opened_storage_slot_indices_.clear();
        pending_task_indices_.clear();
        pending_phone_listing_indices_.clear();
        pending_modifier_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_site_state_->tiles.assign(site_tile_capacity(*pending_site_state_), Gs1RuntimeTileProjection {});
            for (Gs1RuntimeTileProjection& tile : pending_site_state_->tiles)
            {
                tile.x = k_invalid_tile_coordinate;
                tile.y = k_invalid_tile_coordinate;
            }
            pending_site_state_->inventory_storages.clear();
            pending_site_state_->worker_pack_slots.clear();
            pending_site_state_->tasks.clear();
            pending_site_state_->active_modifiers.clear();
            pending_site_state_->phone_listings.clear();
            pending_site_state_->worker_pack_open = false;
            pending_site_state_->phone_panel = Gs1RuntimePhonePanelProjection {};
            pending_site_state_->protection_overlay = Gs1RuntimeProtectionOverlayProjection {};
            pending_site_state_->opened_storage.reset();
            pending_site_state_->craft_context.reset();
            pending_site_state_->placement_preview.reset();
            pending_site_state_->placement_preview_tiles.clear();
            pending_site_state_->placement_failure.reset();
            pending_site_state_->worker.reset();
            pending_site_state_->camp.reset();
            pending_site_state_->weather.reset();
        }
        else if (pending_site_state_->tiles.size() != site_tile_capacity(*pending_site_state_))
        {
            pending_site_state_->tiles.assign(site_tile_capacity(*pending_site_state_), Gs1RuntimeTileProjection {});
            for (Gs1RuntimeTileProjection& tile : pending_site_state_->tiles)
            {
                tile.x = k_invalid_tile_coordinate;
                tile.y = k_invalid_tile_coordinate;
            }
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        Gs1RuntimeTileProjection projection = message.payload_as<Gs1EngineMessageSiteTileData>();
        const auto tile_index = site_tile_index(*pending_site_state_, projection.x, projection.y);
        if (tile_index.has_value())
        {
            pending_site_state_->tiles[*tile_index] = projection;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
        if (pending_site_state_.has_value()) pending_site_state_->worker = message.payload_as<Gs1EngineMessageWorkerData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
        if (pending_site_state_.has_value()) pending_site_state_->camp = message.payload_as<Gs1EngineMessageCampData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
        if (pending_site_state_.has_value()) pending_site_state_->weather = message.payload_as<Gs1EngineMessageWeatherData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        Gs1RuntimeInventoryStorageProjection projection = message.payload_as<Gs1EngineMessageInventoryStorageData>();
        const auto found = pending_inventory_storage_indices_.find(projection.storage_id);
        if (found != pending_inventory_storage_indices_.end() &&
            found->second < pending_site_state_->inventory_storages.size())
        {
            pending_site_state_->inventory_storages[found->second] = projection;
        }
        else
        {
            pending_inventory_storage_indices_[projection.storage_id] = pending_site_state_->inventory_storages.size();
            pending_site_state_->inventory_storages.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        Gs1RuntimeInventorySlotProjection projection = message.payload_as<Gs1EngineMessageInventorySlotData>();
        std::vector<Gs1RuntimeInventorySlotProjection>* slots = nullptr;
        if (projection.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
        {
            slots = &pending_site_state_->worker_pack_slots;
        }
        else if (pending_site_state_->opened_storage.has_value() &&
                 pending_site_state_->opened_storage->storage_id == projection.storage_id)
        {
            slots = &pending_site_state_->opened_storage->slots;
        }
        if (slots == nullptr)
        {
            break;
        }
        const std::uint64_t slot_key =
            (static_cast<std::uint64_t>(projection.storage_id) << 32U) | projection.slot_index;
        auto& slot_indices =
            slots == &pending_site_state_->worker_pack_slots
                ? pending_worker_pack_slot_indices_
                : pending_opened_storage_slot_indices_;
        const auto found = slot_indices.find(slot_key);
        if (found != slot_indices.end() && found->second < slots->size())
        {
            (*slots)[found->second] = projection;
        }
        else
        {
            slot_indices[slot_key] = slots->size();
            slots->push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageInventoryViewData>();
        const auto storage = std::find_if(
            pending_site_state_->inventory_storages.begin(),
            pending_site_state_->inventory_storages.end(),
            [&](const auto& projection) { return projection.storage_id == payload.storage_id; });
        const bool is_worker_pack_storage =
            storage != pending_site_state_->inventory_storages.end() &&
            storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;
        if (is_worker_pack_storage)
        {
            pending_site_state_->worker_pack_open = payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT;
            break;
        }
        if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE)
        {
            if (pending_site_state_->opened_storage.has_value() &&
                pending_site_state_->opened_storage->storage_id == payload.storage_id)
            {
                pending_site_state_->opened_storage.reset();
            }
            break;
        }
        if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            pending_site_state_->opened_storage = Gs1RuntimeInventoryViewProjection {};
            pending_site_state_->opened_storage->storage_id = payload.storage_id;
            pending_site_state_->opened_storage->slot_count = payload.slot_count;
            pending_site_state_->opened_storage->slots.clear();
            pending_opened_storage_slot_indices_.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageCraftContextData>();
        pending_site_state_->craft_context = Gs1RuntimeCraftContextProjection {};
        pending_site_state_->craft_context->tile_x = payload.tile_x;
        pending_site_state_->craft_context->tile_y = payload.tile_y;
        pending_site_state_->craft_context->flags = payload.flags;
        pending_site_state_->craft_context->options.clear();
        pending_site_state_->craft_context->options.reserve(payload.option_count);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
        if (pending_site_state_.has_value() && pending_site_state_->craft_context.has_value())
        {
            pending_site_state_->craft_context->options.push_back(
                message.payload_as<Gs1EngineMessageCraftContextOptionData>());
        }
        break;
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
        if (pending_site_state_.has_value() && pending_site_state_->craft_context.has_value())
        {
            sort_craft_options(pending_site_state_->craft_context->options);
        }
        break;
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessagePlacementPreviewData>();
        if ((payload.flags & 1U) == 0U)
        {
            pending_site_state_->placement_preview.reset();
            pending_site_state_->placement_preview_tiles.clear();
        }
        else
        {
            pending_site_state_->placement_preview = payload;
            pending_site_state_->placement_preview_tiles.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        Gs1RuntimePlacementPreviewTileProjection projection =
            message.payload_as<Gs1EngineMessagePlacementPreviewTileData>();
        upsert_projection(
            pending_site_state_->placement_preview_tiles,
            projection,
            (static_cast<std::uint64_t>(static_cast<std::uint16_t>(projection.x)) << 16U) |
                static_cast<std::uint16_t>(projection.y),
            [](const auto& tile) {
                return (static_cast<std::uint64_t>(static_cast<std::uint16_t>(tile.x)) << 16U) |
                       static_cast<std::uint16_t>(tile.y);
            });
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
    {
        auto* site =
            pending_site_state_.has_value() ? &pending_site_state_.value()
                                            : (site_state_.has_value() ? &site_state_.value() : nullptr);
        if (site != nullptr)
        {
            site->placement_failure = message.payload_as<Gs1EngineMessagePlacementFailureData>();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        Gs1RuntimeTaskProjection projection = message.payload_as<Gs1EngineMessageTaskData>();
        const auto found = pending_task_indices_.find(projection.task_instance_id);
        if (found != pending_task_indices_.end() && found->second < pending_site_state_->tasks.size())
        {
            pending_site_state_->tasks[found->second] = projection;
        }
        else
        {
            pending_task_indices_[projection.task_instance_id] = pending_site_state_->tasks.size();
            pending_site_state_->tasks.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageTaskData>();
        const auto found = pending_task_indices_.find(payload.task_instance_id);
        if (found == pending_task_indices_.end() || found->second >= pending_site_state_->tasks.size())
        {
            break;
        }
        const std::size_t index = found->second;
        const std::size_t last_index = pending_site_state_->tasks.size() - 1U;
        if (index != last_index)
        {
            pending_site_state_->tasks[index] = std::move(pending_site_state_->tasks[last_index]);
            pending_task_indices_[pending_site_state_->tasks[index].task_instance_id] = index;
        }
        pending_site_state_->tasks.pop_back();
        pending_task_indices_.erase(found);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        Gs1RuntimePhoneListingProjection projection = message.payload_as<Gs1EngineMessagePhoneListingData>();
        const auto found = pending_phone_listing_indices_.find(projection.listing_id);
        if (found != pending_phone_listing_indices_.end() &&
            found->second < pending_site_state_->phone_listings.size())
        {
            pending_site_state_->phone_listings[found->second] = projection;
        }
        else
        {
            pending_phone_listing_indices_[projection.listing_id] = pending_site_state_->phone_listings.size();
            pending_site_state_->phone_listings.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessagePhoneListingData>();
        const auto found = pending_phone_listing_indices_.find(payload.listing_id);
        if (found == pending_phone_listing_indices_.end() ||
            found->second >= pending_site_state_->phone_listings.size())
        {
            break;
        }
        const std::size_t index = found->second;
        const std::size_t last_index = pending_site_state_->phone_listings.size() - 1U;
        if (index != last_index)
        {
            pending_site_state_->phone_listings[index] = std::move(pending_site_state_->phone_listings[last_index]);
            pending_phone_listing_indices_[pending_site_state_->phone_listings[index].listing_id] = index;
        }
        pending_site_state_->phone_listings.pop_back();
        pending_phone_listing_indices_.erase(found);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
        if (pending_site_state_.has_value()) pending_site_state_->phone_panel = message.payload_as<Gs1EngineMessagePhonePanelData>();
        break;
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        const auto& payload = message.payload_as<Gs1EngineMessageSiteModifierListData>();
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            pending_site_state_->active_modifiers.clear();
        }
        pending_site_state_->active_modifiers.reserve(payload.modifier_count);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        Gs1RuntimeModifierProjection projection = message.payload_as<Gs1EngineMessageSiteModifierData>();
        const auto found = pending_modifier_indices_.find(projection.modifier_id);
        if (found != pending_modifier_indices_.end() &&
            found->second < pending_site_state_->active_modifiers.size())
        {
            pending_site_state_->active_modifiers[found->second] = projection;
        }
        else
        {
            pending_modifier_indices_[projection.modifier_id] = pending_site_state_->active_modifiers.size();
            pending_site_state_->active_modifiers.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
    {
        auto* site =
            pending_site_state_.has_value() ? &pending_site_state_.value()
                                            : (site_state_.has_value() ? &site_state_.value() : nullptr);
        if (site != nullptr)
        {
            site->protection_overlay = message.payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
        break;
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
    {
        if (!pending_site_state_.has_value())
        {
            break;
        }
        sort_inventory_storages(pending_site_state_->inventory_storages);
        sort_inventory_slots(pending_site_state_->worker_pack_slots);
        if (pending_site_state_->opened_storage.has_value())
        {
            sort_inventory_slots(pending_site_state_->opened_storage->slots);
        }
        sort_tasks(pending_site_state_->tasks);
        sort_modifiers(pending_site_state_->active_modifiers);
        sort_phone_listings(pending_site_state_->phone_listings);
        sort_placement_preview_tiles(pending_site_state_->placement_preview_tiles);
        site_state_ = std::move(pending_site_state_);
        pending_site_state_.reset();
        pending_inventory_storage_indices_.clear();
        pending_worker_pack_slot_indices_.clear();
        pending_opened_storage_slot_indices_.clear();
        pending_task_indices_.clear();
        pending_phone_listing_indices_.clear();
        pending_modifier_indices_.clear();
        break;
    }
    default:
        break;
    }
}

bool Gs1GodotSiteSessionUiController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
    case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1GodotSiteSessionUiController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        current_app_state_ = static_cast<int>(message.payload_as<Gs1EngineMessageSetAppStateData>().app_state);
        if (current_app_state_ < APP_STATE_SITE_LOADING || current_app_state_ > APP_STATE_SITE_RESULT)
        {
            reset_site_state();
            cached_storage_lookup_.clear();
        }
    }

    apply_site_message(message);
    refresh_visibility();
    mark_selected_tile_dirty();
    apply_selected_tile_if_needed();
}

void Gs1GodotSiteSessionUiController::handle_runtime_message_reset()
{
    current_app_state_ = APP_STATE_BOOT;
    reset_site_state();
    cached_storage_lookup_.clear();
    selected_tile_ = Vector2i(0, 0);
    last_tile_label_x_ = -1;
    last_tile_label_y_ = -1;
    selected_tile_dirty_ = true;
    refresh_visibility();
    apply_selected_tile_if_needed();
}

void Gs1GodotSiteSessionUiController::handle_input_event(const Ref<InputEvent>& event)
{
    const auto* key_event = event.is_null() ? nullptr : Object::cast_to<InputEventKey>(*event);
    if (key_event == nullptr || !key_event->is_pressed())
    {
        return;
    }
    if (site_panel_ == nullptr || !site_panel_->is_visible())
    {
        return;
    }

    switch (key_event->get_keycode())
    {
    case KEY_UP:
        selected_tile_.y -= 1;
        break;
    case KEY_DOWN:
        selected_tile_.y += 1;
        break;
    case KEY_LEFT:
        selected_tile_.x -= 1;
        break;
    case KEY_RIGHT:
        selected_tile_.x += 1;
        break;
    default:
        return;
    }

    mark_selected_tile_dirty();
    clamp_selected_tile();
}

void Gs1GodotSiteSessionUiController::refresh_visibility()
{
    const bool has_site_projection = site_state_.has_value() || pending_site_state_.has_value();
    const bool site_visible =
        has_site_projection ||
        (current_app_state_ >= APP_STATE_SITE_LOADING &&
            current_app_state_ <= APP_STATE_SITE_RESULT);
    if (site_panel_ != nullptr)
    {
        site_panel_->set_visible(site_visible);
    }
}

void Gs1GodotSiteSessionUiController::refresh_selected_tile_if_needed()
{
    if (site_panel_ == nullptr || !site_panel_->is_visible())
    {
        return;
    }
    if (!selected_tile_dirty_ &&
        selected_tile_.x == last_tile_label_x_ &&
        selected_tile_.y == last_tile_label_y_)
    {
        return;
    }

    clamp_selected_tile();
    const Gs1RuntimeTileProjection* selected_tile = tile_at(selected_tile_);
    if (tile_label_ != nullptr)
    {
        String tile_text = vformat(
            "Selected Tile: (%d, %d)  Plant: %s  Structure: %s",
            selected_tile_.x,
            selected_tile_.y,
            selected_tile != nullptr && selected_tile->plant_type_id != 0U ? plant_name_for(static_cast<int>(selected_tile->plant_type_id)) : String("None"),
            selected_tile != nullptr && selected_tile->structure_type_id != 0U ? structure_name_for(static_cast<int>(selected_tile->structure_type_id)) : String("None"));
        if (selected_tile != nullptr)
        {
            tile_text += vformat(
                "  Wind %.2f  Moisture %.2f  Fertility %.2f",
                selected_tile->local_wind,
                selected_tile->moisture,
                selected_tile->soil_fertility);
        }
        tile_label_->set_text(tile_text);
    }

    last_tile_label_x_ = selected_tile_.x;
    last_tile_label_y_ = selected_tile_.y;
    selected_tile_dirty_ = false;
}

void Gs1GodotSiteSessionUiController::apply_selected_tile_if_needed()
{
    refresh_selected_tile_if_needed();
}

void Gs1GodotSiteSessionUiController::mark_selected_tile_dirty()
{
    selected_tile_dirty_ = true;
}

void Gs1GodotSiteSessionUiController::clamp_selected_tile()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    const int width = std::max(1, site_state != nullptr ? static_cast<int>(site_state->width) : 1);
    const int height = std::max(1, site_state != nullptr ? static_cast<int>(site_state->height) : 1);
    selected_tile_.x = std::clamp(selected_tile_.x, 0, width - 1);
    selected_tile_.y = std::clamp(selected_tile_.y, 0, height - 1);
}

const Gs1RuntimeSiteProjection* Gs1GodotSiteSessionUiController::active_site() const
{
    if (!site_state_.has_value())
    {
        return nullptr;
    }
    return &site_state_.value();
}

const Gs1RuntimeTileProjection* Gs1GodotSiteSessionUiController::tile_at(const Vector2i& tile_coord) const
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr || site_state->width == 0 || site_state->height == 0)
    {
        return nullptr;
    }
    if (tile_coord.x < 0 || tile_coord.y < 0 ||
        tile_coord.x >= site_state->width || tile_coord.y >= site_state->height)
    {
        return nullptr;
    }

    const std::size_t index =
        static_cast<std::size_t>(tile_coord.y) * static_cast<std::size_t>(site_state->width) +
        static_cast<std::size_t>(tile_coord.x);
    if (index >= site_state->tiles.size())
    {
        return nullptr;
    }

    return &site_state->tiles[index];
}

int Gs1GodotSiteSessionUiController::find_worker_pack_storage_id() const
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr)
    {
        return 0;
    }

    for (const auto& storage : site_state->inventory_storages)
    {
        if (static_cast<int>(storage.container_kind) == CONTAINER_WORKER_PACK)
        {
            return static_cast<int>(storage.storage_id);
        }
    }
    return 0;
}

int Gs1GodotSiteSessionUiController::find_selected_tile_storage_id()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr)
    {
        return 0;
    }

    for (const auto& storage : site_state->inventory_storages)
    {
        const int tile_x = storage.tile_x;
        const int tile_y = storage.tile_y;
        const int storage_id = static_cast<int>(storage.storage_id);
        cached_storage_lookup_[tile_y * 100000 + tile_x] = storage_id;
        if (tile_x == selected_tile_.x && tile_y == selected_tile_.y)
        {
            return storage_id;
        }
    }
    return 0;
}

void Gs1GodotSiteSessionUiController::plant_first_seed_on_selected_tile()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state == nullptr)
    {
        return;
    }

    for (const auto& slot : site_state->worker_pack_slots)
    {
        const auto* item_def = gs1::find_item_def(gs1::ItemId {slot.item_id});
        if (item_def == nullptr || item_def->linked_plant_id.value == 0U)
        {
            continue;
        }

        submit_site_action_request(
            SITE_ACTION_PLANT,
            SITE_ACTION_REQUEST_FLAG_HAS_ITEM,
            1,
            selected_tile_.x,
            selected_tile_.y,
            0,
            0,
            static_cast<int>(slot.item_id));
        return;
    }
}

String Gs1GodotSiteSessionUiController::plant_name_for(int plant_id) const
{
    auto found = plant_name_cache_.find(plant_id);
    if (found != plant_name_cache_.end())
    {
        return found->second;
    }

    String display_name = String("Plant #") + String::num_int64(plant_id);
    if (const auto* plant_def = gs1::find_plant_def(gs1::PlantId {static_cast<std::uint32_t>(plant_id)}))
    {
        display_name = string_from_view(plant_def->display_name);
    }

    return plant_name_cache_.emplace(plant_id, display_name).first->second;
}

String Gs1GodotSiteSessionUiController::structure_name_for(int structure_id) const
{
    auto found = structure_name_cache_.find(structure_id);
    if (found != structure_name_cache_.end())
    {
        return found->second;
    }

    String display_name = String("Structure #") + String::num_int64(structure_id);
    if (const auto* structure_def = gs1::find_structure_def(gs1::StructureId {static_cast<std::uint32_t>(structure_id)}))
    {
        display_name = string_from_view(structure_def->display_name);
    }

    return structure_name_cache_.emplace(structure_id, display_name).first->second;
}

void Gs1GodotSiteSessionUiController::submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)
{
    if (adapter_service_ == nullptr)
    {
        return;
    }
    const bool ok = adapter_service_->submit_ui_action(action_type, target_id, arg0, arg1);
    (void)ok;
}

void Gs1GodotSiteSessionUiController::submit_move(double x, double y, double z)
{
    if (adapter_service_ == nullptr)
    {
        return;
    }
    const bool ok = adapter_service_->submit_move_direction(x, y, z);
    (void)ok;
}

void Gs1GodotSiteSessionUiController::submit_site_context_request(int tile_x, int tile_y, int flags)
{
    if (adapter_service_ == nullptr)
    {
        return;
    }
    const bool ok = adapter_service_->submit_site_context_request(tile_x, tile_y, flags);
    (void)ok;
}

void Gs1GodotSiteSessionUiController::submit_site_action_request(
    int action_kind,
    int flags,
    int quantity,
    int tile_x,
    int tile_y,
    int primary_subject_id,
    int secondary_subject_id,
    int item_id)
{
    if (adapter_service_ == nullptr)
    {
        return;
    }
    const bool ok = adapter_service_->submit_site_action_request(
        action_kind,
        flags,
        quantity,
        tile_x,
        tile_y,
        primary_subject_id,
        secondary_subject_id,
        item_id);
    (void)ok;
}

void Gs1GodotSiteSessionUiController::submit_site_action_cancel(int action_id, int flags)
{
    if (adapter_service_ == nullptr)
    {
        return;
    }
    const bool ok = adapter_service_->submit_site_action_cancel(action_id, flags);
    (void)ok;
}

void Gs1GodotSiteSessionUiController::submit_storage_view(int storage_id, int event_kind)
{
    if (adapter_service_ == nullptr)
    {
        return;
    }
    const bool ok = adapter_service_->submit_site_storage_view(storage_id, event_kind);
    (void)ok;
}

void Gs1GodotSiteSessionUiController::on_return_to_map_pressed()
{
    submit_ui_action(UI_ACTION_RETURN_TO_REGIONAL_MAP, 0, 0, 0);
}

void Gs1GodotSiteSessionUiController::on_submit_move_pressed(double x, double y, double z)
{
    submit_move(x, y, z);
    mark_selected_tile_dirty();
}

void Gs1GodotSiteSessionUiController::on_submit_selected_tile_context_pressed(int flags)
{
    submit_site_context_request(selected_tile_.x, selected_tile_.y, flags);
}

void Gs1GodotSiteSessionUiController::on_submit_site_action_cancel_pressed(int action_id, int flags)
{
    submit_site_action_cancel(action_id, flags);
}

void Gs1GodotSiteSessionUiController::on_open_nearest_storage_pressed()
{
    const int id = find_selected_tile_storage_id();
    if (id != 0)
    {
        submit_storage_view(id, INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT);
    }
}

void Gs1GodotSiteSessionUiController::on_close_storage_pressed()
{
    const Gs1RuntimeSiteProjection* site_state = active_site();
    if (site_state != nullptr && site_state->opened_storage.has_value())
    {
        submit_storage_view(static_cast<int>(site_state->opened_storage->storage_id), INVENTORY_VIEW_EVENT_CLOSE);
    }
}

void Gs1GodotSiteSessionUiController::on_plant_selected_seed_pressed()
{
    plant_first_seed_on_selected_tile();
}
