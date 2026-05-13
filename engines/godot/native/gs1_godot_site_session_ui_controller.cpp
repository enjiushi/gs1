#include "gs1_godot_site_session_ui_controller.h"

#include "gs1_godot_controller_context.h"

#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"

#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/label.hpp>
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
    refresh_from_game_state_view();
    refresh_visibility();
    apply_selected_tile_if_needed();
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

void Gs1GodotSiteSessionUiController::reset_site_state() noexcept
{
    site_state_.reset();
    inventory_storages_.clear();
    inventory_slots_.clear();
}

void Gs1GodotSiteSessionUiController::refresh_from_game_state_view()
{
    reset_site_state();
    if (adapter_service_ == nullptr)
    {
        return;
    }

    Gs1GameStateView view {};
    if (!adapter_service_->get_game_state_view(view))
    {
        return;
    }

    current_app_state_ = static_cast<int>(view.app_state);
    if (view.has_active_site == 0U || view.active_site == nullptr)
    {
        return;
    }

    site_state_ = *view.active_site;
    if (site_state_->storage_count > 0U && site_state_->storages != nullptr)
    {
        inventory_storages_.assign(
            site_state_->storages,
            site_state_->storages + site_state_->storage_count);
    }
    if (site_state_->storage_slot_count > 0U && site_state_->storage_slots != nullptr)
    {
        inventory_slots_.assign(
            site_state_->storage_slots,
            site_state_->storage_slots + site_state_->storage_slot_count);
    }

    site_state_->storages = inventory_storages_.empty() ? nullptr : inventory_storages_.data();
    site_state_->storage_slots = inventory_slots_.empty() ? nullptr : inventory_slots_.data();
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
    return type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        type == GS1_ENGINE_MESSAGE_SET_APP_STATE;
}

void Gs1GodotSiteSessionUiController::handle_engine_message(const Gs1EngineMessage& message)
{
    if (message.type == GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY ||
        message.type == GS1_ENGINE_MESSAGE_SET_APP_STATE)
    {
        refresh_from_game_state_view();
        if (current_app_state_ < APP_STATE_SITE_LOADING || current_app_state_ > APP_STATE_SITE_RESULT)
        {
            cached_storage_lookup_.clear();
        }
    }

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
    const bool has_site_projection = site_state_.has_value();
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
    Gs1SiteTileView selected_tile_view {};
    const bool has_tile = query_tile_at(selected_tile_, selected_tile_view);
    if (tile_label_ != nullptr)
    {
        String tile_text = vformat(
            "Selected Tile: (%d, %d)  Plant: %s  Structure: %s",
            selected_tile_.x,
            selected_tile_.y,
            has_tile && selected_tile_view.plant_id != 0U ? plant_name_for(static_cast<int>(selected_tile_view.plant_id)) : String("None"),
            has_tile && selected_tile_view.structure_id != 0U ? structure_name_for(static_cast<int>(selected_tile_view.structure_id)) : String("None"));
        if (has_tile)
        {
            tile_text += vformat(
                "  Wind %.2f  Moisture %.2f  Fertility %.2f",
                selected_tile_view.local_wind,
                selected_tile_view.moisture,
                selected_tile_view.soil_fertility);
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
    const Gs1SiteStateView* site_state = active_site();
    const int width = std::max(1, site_state != nullptr ? static_cast<int>(site_state->tile_width) : 1);
    const int height = std::max(1, site_state != nullptr ? static_cast<int>(site_state->tile_height) : 1);
    selected_tile_.x = std::clamp(selected_tile_.x, 0, width - 1);
    selected_tile_.y = std::clamp(selected_tile_.y, 0, height - 1);
}

const Gs1SiteStateView* Gs1GodotSiteSessionUiController::active_site() const
{
    if (!site_state_.has_value())
    {
        return nullptr;
    }
    return &site_state_.value();
}

bool Gs1GodotSiteSessionUiController::query_tile_at(const Vector2i& tile_coord, Gs1SiteTileView& out_tile) const
{
    const Gs1SiteStateView* site_state = active_site();
    if (site_state == nullptr || site_state->tile_width == 0U || site_state->tile_height == 0U)
    {
        return false;
    }
    if (tile_coord.x < 0 || tile_coord.y < 0 ||
        tile_coord.x >= static_cast<int>(site_state->tile_width) ||
        tile_coord.y >= static_cast<int>(site_state->tile_height))
    {
        return false;
    }
    if (adapter_service_ == nullptr)
    {
        return false;
    }

    const std::uint32_t tile_index =
        static_cast<std::uint32_t>(tile_coord.y) * site_state->tile_width +
        static_cast<std::uint32_t>(tile_coord.x);
    return adapter_service_->query_site_tile_view(tile_index, out_tile);
}

int Gs1GodotSiteSessionUiController::find_worker_pack_storage_id() const
{
    const Gs1SiteStateView* site_state = active_site();
    return site_state != nullptr ? static_cast<int>(site_state->worker_pack_storage_id) : 0;
}

int Gs1GodotSiteSessionUiController::find_selected_tile_storage_id()
{
    const Gs1SiteStateView* site_state = active_site();
    if (site_state == nullptr)
    {
        return 0;
    }

    for (const auto& storage : inventory_storages_)
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
    const int worker_pack_storage_id = find_worker_pack_storage_id();
    if (worker_pack_storage_id == 0)
    {
        return;
    }

    for (const auto& slot : inventory_slots_)
    {
        if (slot.storage_id != static_cast<std::uint32_t>(worker_pack_storage_id) ||
            slot.occupied == 0U ||
            slot.item_id == 0U)
        {
            continue;
        }

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
    const Gs1SiteStateView* site_state = active_site();
    if (site_state != nullptr && site_state->opened_device_storage_id != 0U)
    {
        submit_storage_view(static_cast<int>(site_state->opened_device_storage_id), INVENTORY_VIEW_EVENT_CLOSE);
    }
}

void Gs1GodotSiteSessionUiController::on_plant_selected_seed_pressed()
{
    plant_first_seed_on_selected_tile();
}
