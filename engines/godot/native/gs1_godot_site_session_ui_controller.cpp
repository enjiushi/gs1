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
#include <string_view>

using namespace godot;

namespace
{
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
    set_process(true);
    set_process_input(true);
}

void Gs1GodotSiteSessionUiController::_process(double delta)
{
    (void)delta;
    if (adapter_service_ == nullptr)
    {
        return;
    }

    refresh_visibility();
    refresh_selected_tile_if_needed();
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
            cached_storage_lookup_.clear();
        }
    }

    site_state_reducer_.apply_engine_message(message);
    mark_selected_tile_dirty();
    refresh_visibility();
}

void Gs1GodotSiteSessionUiController::handle_runtime_message_reset()
{
    current_app_state_ = APP_STATE_BOOT;
    site_state_reducer_.reset();
    cached_storage_lookup_.clear();
    selected_tile_ = Vector2i(0, 0);
    last_tile_label_x_ = -1;
    last_tile_label_y_ = -1;
    selected_tile_dirty_ = true;
    refresh_visibility();
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
    const bool site_visible =
        current_app_state_ >= APP_STATE_SITE_LOADING &&
        current_app_state_ <= APP_STATE_SITE_RESULT;
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
    if (!site_state_reducer_.state().has_value())
    {
        return nullptr;
    }
    return &site_state_reducer_.state().value();
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
