#include "gs1_godot_runtime_node.h"

#include "godot_progression_resources.h"

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/task_defs.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <algorithm>
#include <string_view>

using namespace godot;

namespace
{
constexpr std::int64_t k_content_resource_kind_plant = 0;
constexpr std::int64_t k_content_resource_kind_item = 1;
constexpr std::int64_t k_content_resource_kind_structure = 2;
constexpr std::int64_t k_content_resource_kind_recipe = 3;

String to_godot_string(const std::string& value)
{
    return String::utf8(value.c_str(), static_cast<int64_t>(value.size()));
}

String to_godot_string(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int64_t>(value.size()));
}

String item_display_name(std::uint32_t item_id)
{
    const auto* item_def = gs1::find_item_def(gs1::ItemId {item_id});
    return item_def != nullptr
        ? to_godot_string(item_def->display_name)
        : String("Item #") + String::num_int64(item_id);
}

String plant_display_name(std::uint32_t plant_id)
{
    const auto* plant_def = gs1::find_plant_def(gs1::PlantId {plant_id});
    return plant_def != nullptr
        ? String::utf8(plant_def->display_name, static_cast<int64_t>(std::char_traits<char>::length(plant_def->display_name)))
        : String("Plant #") + String::num_int64(plant_id);
}

String structure_display_name(std::uint32_t structure_id)
{
    const auto* structure_def = gs1::find_structure_def(gs1::StructureId {structure_id});
    return structure_def != nullptr
        ? to_godot_string(structure_def->display_name)
        : String("Structure #") + String::num_int64(structure_id);
}

String recipe_output_name(std::uint32_t recipe_id, std::uint32_t output_item_id)
{
    const auto* recipe_def = gs1::find_craft_recipe_def(gs1::RecipeId {recipe_id});
    if (recipe_def != nullptr)
    {
        return item_display_name(recipe_def->output_item_id.value);
    }
    return item_display_name(output_item_id);
}

String one_shot_cue_name(Gs1OneShotCueKind cue_kind)
{
    switch (cue_kind)
    {
    case GS1_ONE_SHOT_CUE_CAMPAIGN_UNLOCKED:
        return "campaign_unlocked";
    case GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED:
        return "task_reward_claimed";
    case GS1_ONE_SHOT_CUE_CRAFT_OUTPUT_STORED:
        return "craft_output_stored";
    case GS1_ONE_SHOT_CUE_ACTION_COMPLETED:
        return "action_completed";
    case GS1_ONE_SHOT_CUE_ACTION_FAILED:
        return "action_failed";
    case GS1_ONE_SHOT_CUE_HAZARD_PEAK:
        return "hazard_peak";
    case GS1_ONE_SHOT_CUE_SITE_COMPLETED:
        return "site_completed";
    case GS1_ONE_SHOT_CUE_SITE_FAILED:
        return "site_failed";
    case GS1_ONE_SHOT_CUE_NONE:
    default:
        return "none";
    }
}

Dictionary ui_action_to_dictionary(const Gs1UiAction& action)
{
    Dictionary result;
    result["type"] = static_cast<int>(action.type);
    result["target_id"] = static_cast<int64_t>(action.target_id);
    result["arg0"] = static_cast<int64_t>(action.arg0);
    result["arg1"] = static_cast<int64_t>(action.arg1);
    return result;
}

const Gs1RuntimeTileProjection* find_tile_at(
    const Gs1RuntimeSiteProjection& site,
    std::int32_t x,
    std::int32_t y)
{
    if (x < 0 || y < 0)
    {
        return nullptr;
    }

    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(site.width)
        + static_cast<std::size_t>(x);
    if (index >= site.tiles.size())
    {
        return nullptr;
    }

    const auto& tile = site.tiles[index];
    if (tile.x == static_cast<std::uint16_t>(x) && tile.y == static_cast<std::uint16_t>(y))
    {
        return &tile;
    }
    return nullptr;
}
}

Gs1RuntimeNode::~Gs1RuntimeNode()
{
    runtime_session_.stop();
}

void Gs1RuntimeNode::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_runtime_summary"), &Gs1RuntimeNode::get_runtime_summary);
    ClassDB::bind_method(D_METHOD("get_projection"), &Gs1RuntimeNode::get_projection);
    ClassDB::bind_method(D_METHOD("has_active_site"), &Gs1RuntimeNode::has_active_site);
    ClassDB::bind_method(D_METHOD("get_site_width"), &Gs1RuntimeNode::get_site_width);
    ClassDB::bind_method(D_METHOD("get_site_height"), &Gs1RuntimeNode::get_site_height);
    ClassDB::bind_method(D_METHOD("get_worker_tile_x"), &Gs1RuntimeNode::get_worker_tile_x);
    ClassDB::bind_method(D_METHOD("get_worker_tile_y"), &Gs1RuntimeNode::get_worker_tile_y);
    ClassDB::bind_method(D_METHOD("get_tile_count"), &Gs1RuntimeNode::get_tile_count);
    ClassDB::bind_method(D_METHOD("get_tile_at_index", "index"), &Gs1RuntimeNode::get_tile_at_index);

    ClassDB::bind_method(
        D_METHOD("submit_ui_action", "action_type", "target_id", "arg0", "arg1"),
        static_cast<bool (Gs1RuntimeNode::*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t)>(&Gs1RuntimeNode::submit_ui_action),
        DEFVAL(0),
        DEFVAL(0),
        DEFVAL(0));
    ClassDB::bind_method(D_METHOD("submit_move_direction", "world_move_x", "world_move_y", "world_move_z"), &Gs1RuntimeNode::submit_move_direction);
    ClassDB::bind_method(D_METHOD("submit_site_context_request", "tile_x", "tile_y", "flags"), &Gs1RuntimeNode::submit_site_context_request);
    ClassDB::bind_method(D_METHOD("submit_site_action_request", "action_kind", "flags", "quantity", "tile_x", "tile_y", "primary_subject_id", "secondary_subject_id", "item_id"), &Gs1RuntimeNode::submit_site_action_request);
    ClassDB::bind_method(D_METHOD("submit_site_action_cancel", "action_id", "flags"), &Gs1RuntimeNode::submit_site_action_cancel);
    ClassDB::bind_method(D_METHOD("submit_site_storage_view", "storage_id", "event_kind"), &Gs1RuntimeNode::submit_site_storage_view);
}

void Gs1RuntimeNode::_ready()
{
    set_process(true);
    refresh_gameplay_dll_path();
    refresh_project_config_root();
    ensure_runtime_started();
}

void Gs1RuntimeNode::_process(double delta)
{
    if (!runtime_session_.is_running())
    {
        ensure_runtime_started();
    }
    process_frame(delta > 0.0 ? delta : (1.0 / 60.0));
}

void Gs1RuntimeNode::ensure_runtime_started()
{
    if (runtime_session_.is_running())
    {
        return;
    }

    if (gameplay_dll_path_.empty())
    {
        refresh_gameplay_dll_path();
    }
    if (project_config_root_.empty())
    {
        refresh_project_config_root();
    }

    adapter_config_ = load_adapter_config_blob(
        project_config_root_,
        std::filesystem::path {"project"} / "config");

    projection_cache_.reset();
    drained_messages_.clear();
    if (!runtime_session_.start(gameplay_dll_path_, project_config_root_, &adapter_config_))
    {
        last_error_ = runtime_session_.last_error();
        projection_revision_ += 1U;
        return;
    }

    projection_revision_ += 1U;
    if (!drain_projection_messages())
    {
        runtime_session_.stop();
        projection_cache_.reset();
        drained_messages_.clear();
        projection_revision_ += 1U;
        return;
    }

    last_error_.clear();
}

void Gs1RuntimeNode::process_frame(double delta_seconds)
{
    if (!runtime_session_.is_running())
    {
        return;
    }

    Gs1Phase1Result phase1 {};
    Gs1Phase2Result phase2 {};
    if (!runtime_session_.update(delta_seconds, phase1, phase2))
    {
        last_error_ = runtime_session_.last_error();
        runtime_session_.stop();
        projection_cache_.reset();
        drained_messages_.clear();
        projection_revision_ += 1U;
        return;
    }

    if (!drain_projection_messages())
    {
        runtime_session_.stop();
        projection_cache_.reset();
        drained_messages_.clear();
        projection_revision_ += 1U;
        return;
    }

    last_error_.clear();
}

bool Gs1RuntimeNode::drain_projection_messages()
{
    std::uint32_t message_count = 0;
    std::string pump_error {};
    if (!message_pump_.drain_runtime_messages(
            runtime_session_,
            projection_cache_,
            &drained_messages_,
            message_count,
            pump_error))
    {
        last_error_ = std::move(pump_error);
        return false;
    }

    if (message_count > 0U)
    {
        projection_revision_ += 1U;
    }

    return true;
}

std::vector<Gs1EngineMessage> Gs1RuntimeNode::take_drained_messages()
{
    std::vector<Gs1EngineMessage> messages;
    messages.swap(drained_messages_);
    return messages;
}

bool Gs1RuntimeNode::submit_ui_action(const Gs1UiAction& action)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        projection_revision_ += 1U;
        return false;
    }
    return true;
}

bool Gs1RuntimeNode::submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)
{
    Gs1UiAction action {};
    action.type = static_cast<Gs1UiActionType>(action_type);
    action.target_id = static_cast<std::uint32_t>(target_id);
    action.arg0 = static_cast<std::uint64_t>(arg0);
    action.arg1 = static_cast<std::uint64_t>(arg1);
    return submit_ui_action(action);
}

bool Gs1RuntimeNode::submit_move_direction(double world_move_x, double world_move_y, double world_move_z)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_MOVE_DIRECTION;
    event.payload.site_move_direction.world_move_x = static_cast<float>(world_move_x);
    event.payload.site_move_direction.world_move_y = static_cast<float>(world_move_y);
    event.payload.site_move_direction.world_move_z = static_cast<float>(world_move_z);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        projection_revision_ += 1U;
        return false;
    }
    return true;
}

bool Gs1RuntimeNode::submit_site_context_request(std::int64_t tile_x, std::int64_t tile_y, std::int64_t flags)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_CONTEXT_REQUEST;
    event.payload.site_context_request.tile_x = static_cast<std::int32_t>(tile_x);
    event.payload.site_context_request.tile_y = static_cast<std::int32_t>(tile_y);
    event.payload.site_context_request.flags = static_cast<std::uint32_t>(flags);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        projection_revision_ += 1U;
        return false;
    }
    return true;
}

bool Gs1RuntimeNode::submit_site_action_request(
    std::int64_t action_kind,
    std::int64_t flags,
    std::int64_t quantity,
    std::int64_t tile_x,
    std::int64_t tile_y,
    std::int64_t primary_subject_id,
    std::int64_t secondary_subject_id,
    std::int64_t item_id)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_REQUEST;
    event.payload.site_action_request.action_kind = static_cast<Gs1SiteActionKind>(action_kind);
    event.payload.site_action_request.flags = static_cast<std::uint8_t>(flags);
    event.payload.site_action_request.quantity = static_cast<std::uint16_t>(quantity);
    event.payload.site_action_request.target_tile_x = static_cast<std::int32_t>(tile_x);
    event.payload.site_action_request.target_tile_y = static_cast<std::int32_t>(tile_y);
    event.payload.site_action_request.primary_subject_id = static_cast<std::uint32_t>(primary_subject_id);
    event.payload.site_action_request.secondary_subject_id = static_cast<std::uint32_t>(secondary_subject_id);
    event.payload.site_action_request.item_id = static_cast<std::uint32_t>(item_id);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        projection_revision_ += 1U;
        return false;
    }
    return true;
}

bool Gs1RuntimeNode::submit_site_action_cancel(std::int64_t action_id, std::int64_t flags)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_CANCEL;
    event.payload.site_action_cancel.action_id = static_cast<std::uint32_t>(action_id);
    event.payload.site_action_cancel.flags = static_cast<std::uint32_t>(flags);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        projection_revision_ += 1U;
        return false;
    }
    return true;
}

bool Gs1RuntimeNode::submit_site_storage_view(std::int64_t storage_id, std::int64_t event_kind)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_SITE_STORAGE_VIEW;
    event.payload.site_storage_view.storage_id = static_cast<std::uint32_t>(storage_id);
    event.payload.site_storage_view.event_kind = static_cast<Gs1InventoryViewEventKind>(event_kind);
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
        projection_revision_ += 1U;
        return false;
    }
    return true;
}

String Gs1RuntimeNode::get_runtime_summary() const
{
    String summary;
    summary += runtime_session_.is_running() ? "Runtime running" : "Runtime idle";
    if (!last_error_.empty())
    {
        summary += String(" | Error: ") + to_godot_string(last_error_);
    }
    return summary;
}

Dictionary Gs1RuntimeNode::get_projection() const
{
    return build_projection_dictionary();
}

bool Gs1RuntimeNode::has_active_site() const
{
    return projection_cache_.state().active_site.has_value();
}

int Gs1RuntimeNode::get_site_width() const
{
    return projection_cache_.state().active_site.has_value() ? projection_cache_.state().active_site->width : 0;
}

int Gs1RuntimeNode::get_site_height() const
{
    return projection_cache_.state().active_site.has_value() ? projection_cache_.state().active_site->height : 0;
}

double Gs1RuntimeNode::get_worker_tile_x() const
{
    if (!projection_cache_.state().active_site.has_value() || !projection_cache_.state().active_site->worker.has_value())
    {
        return -1.0;
    }
    return projection_cache_.state().active_site->worker->tile_x;
}

double Gs1RuntimeNode::get_worker_tile_y() const
{
    if (!projection_cache_.state().active_site.has_value() || !projection_cache_.state().active_site->worker.has_value())
    {
        return -1.0;
    }
    return projection_cache_.state().active_site->worker->tile_y;
}

int Gs1RuntimeNode::get_tile_count() const
{
    return projection_cache_.state().active_site.has_value()
        ? static_cast<int>(projection_cache_.state().active_site->tiles.size())
        : 0;
}

Dictionary Gs1RuntimeNode::get_tile_at_index(int index) const
{
    Dictionary tile_dict;
    if (!projection_cache_.state().active_site.has_value())
    {
        return tile_dict;
    }

    const auto& tiles = projection_cache_.state().active_site->tiles;
    if (index < 0 || static_cast<std::size_t>(index) >= tiles.size())
    {
        return tile_dict;
    }

    const auto& tile = tiles[static_cast<std::size_t>(index)];
    tile_dict["x"] = static_cast<int>(tile.x);
    tile_dict["y"] = static_cast<int>(tile.y);
    tile_dict["terrain_type_id"] = static_cast<int64_t>(tile.terrain_type_id);
    tile_dict["plant_type_id"] = static_cast<int64_t>(tile.plant_type_id);
    tile_dict["structure_type_id"] = static_cast<int64_t>(tile.structure_type_id);
    tile_dict["ground_cover_type_id"] = static_cast<int64_t>(tile.ground_cover_type_id);
    tile_dict["plant_density"] = tile.plant_density;
    tile_dict["sand_burial"] = tile.sand_burial;
    tile_dict["local_wind"] = tile.local_wind;
    tile_dict["wind_protection"] = tile.wind_protection;
    tile_dict["heat_protection"] = tile.heat_protection;
    tile_dict["dust_protection"] = tile.dust_protection;
    tile_dict["moisture"] = tile.moisture;
    tile_dict["soil_fertility"] = tile.soil_fertility;
    tile_dict["soil_salinity"] = tile.soil_salinity;
    tile_dict["device_integrity_quantized"] = static_cast<int>(tile.device_integrity_quantized);
    tile_dict["excavation_depth"] = static_cast<int>(tile.excavation_depth);
    tile_dict["visible_excavation_depth"] = static_cast<int>(tile.visible_excavation_depth);
    tile_dict["plant_name"] = tile.plant_type_id != 0U ? plant_display_name(tile.plant_type_id) : String();
    tile_dict["structure_name"] = tile.structure_type_id != 0U ? structure_display_name(tile.structure_type_id) : String();
    return tile_dict;
}

Dictionary Gs1RuntimeNode::build_projection_dictionary() const
{
    Dictionary projection;
    const auto& state = projection_cache_.state();
    projection["running"] = runtime_session_.is_running();
    projection["app_state"] = state.current_app_state.has_value() ? static_cast<int>(state.current_app_state.value()) : 0;
    projection["selected_site_id"] = state.selected_site_id.has_value() ? static_cast<int64_t>(state.selected_site_id.value()) : static_cast<int64_t>(0);
    projection["last_error"] = to_godot_string(last_error_);
    projection["ui_setups"] = build_ui_setups_array();
    projection["ui_panels"] = build_ui_panels_array();
    projection["regional_map"] = build_regional_map_dictionary();
    projection["campaign_resources"] = build_campaign_resources_dictionary();
    projection["hud"] = build_hud_dictionary();
    projection["site_action"] = build_site_action_dictionary();
    projection["site_result"] = build_site_result_dictionary();
    projection["active_site"] = build_active_site_dictionary();
    projection["recent_one_shot_cues"] = build_recent_one_shot_cues_array();
    return projection;
}

Dictionary Gs1RuntimeNode::build_regional_map_dictionary() const
{
    Dictionary map_dict;
    Array sites;
    Array links;
    const auto& state = projection_cache_.state();
    for (const auto& site : state.regional_map_sites)
    {
        Dictionary site_dict;
        site_dict["site_id"] = static_cast<int64_t>(site.site_id);
        site_dict["site_state"] = static_cast<int>(site.site_state);
        site_dict["site_archetype_id"] = static_cast<int64_t>(site.site_archetype_id);
        site_dict["flags"] = static_cast<int64_t>(site.flags);
        site_dict["map_x"] = site.map_x;
        site_dict["map_y"] = site.map_y;
        site_dict["support_package_id"] = static_cast<int64_t>(site.support_package_id);
        site_dict["support_preview_mask"] = static_cast<int64_t>(site.support_preview_mask);
        sites.push_back(site_dict);
    }
    for (const auto& link : state.regional_map_links)
    {
        Dictionary link_dict;
        link_dict["from_site_id"] = static_cast<int64_t>(link.from_site_id);
        link_dict["to_site_id"] = static_cast<int64_t>(link.to_site_id);
        link_dict["flags"] = static_cast<int64_t>(link.flags);
        links.push_back(link_dict);
    }
    map_dict["sites"] = sites;
    map_dict["links"] = links;
    return map_dict;
}

Dictionary Gs1RuntimeNode::build_campaign_resources_dictionary() const
{
    Dictionary dict;
    if (!projection_cache_.state().campaign_resources.has_value())
    {
        return dict;
    }

    const auto& resources = projection_cache_.state().campaign_resources.value();
    dict["current_money"] = resources.current_money;
    dict["total_reputation"] = resources.total_reputation;
    dict["village_reputation"] = resources.village_reputation;
    dict["forestry_reputation"] = resources.forestry_reputation;
    dict["university_reputation"] = resources.university_reputation;
    return dict;
}

Dictionary Gs1RuntimeNode::build_hud_dictionary() const
{
    Dictionary dict;
    if (!projection_cache_.state().hud.has_value())
    {
        return dict;
    }

    const auto& hud = projection_cache_.state().hud.value();
    dict["player_health"] = hud.player_health;
    dict["player_hydration"] = hud.player_hydration;
    dict["player_nourishment"] = hud.player_nourishment;
    dict["player_energy"] = hud.player_energy;
    dict["player_morale"] = hud.player_morale;
    dict["current_money"] = hud.current_money;
    dict["site_completion_normalized"] = hud.site_completion_normalized;
    dict["active_task_count"] = static_cast<int>(hud.active_task_count);
    dict["warning_code"] = static_cast<int>(hud.warning_code);
    dict["current_action_kind"] = static_cast<int>(hud.current_action_kind);
    return dict;
}

Dictionary Gs1RuntimeNode::build_site_action_dictionary() const
{
    Dictionary dict;
    if (!projection_cache_.state().site_action.has_value())
    {
        return dict;
    }

    const auto& action = projection_cache_.state().site_action.value();
    dict["action_id"] = static_cast<int64_t>(action.action_id);
    dict["target_tile_x"] = action.target_tile_x;
    dict["target_tile_y"] = action.target_tile_y;
    dict["action_kind"] = static_cast<int>(action.action_kind);
    dict["flags"] = static_cast<int64_t>(action.flags);
    dict["progress_normalized"] = action.progress_normalized;
    dict["duration_minutes"] = action.duration_minutes;
    return dict;
}

Dictionary Gs1RuntimeNode::build_site_result_dictionary() const
{
    Dictionary dict;
    if (!projection_cache_.state().site_result.has_value())
    {
        return dict;
    }

    const auto& result = projection_cache_.state().site_result.value();
    dict["site_id"] = static_cast<int64_t>(result.site_id);
    dict["result"] = static_cast<int>(result.result);
    dict["newly_revealed_site_count"] = static_cast<int>(result.newly_revealed_site_count);
    return dict;
}

Array Gs1RuntimeNode::build_ui_setups_array() const
{
    Array setups;
    for (const auto& setup : projection_cache_.state().active_ui_setups)
    {
        Dictionary setup_dict;
        setup_dict["setup_id"] = static_cast<int>(setup.setup_id);
        setup_dict["presentation_type"] = static_cast<int>(setup.presentation_type);
        setup_dict["context_id"] = static_cast<int64_t>(setup.context_id);

        Array elements;
        for (const auto& element : setup.elements)
        {
            Dictionary element_dict;
            element_dict["element_id"] = static_cast<int64_t>(element.element_id);
            element_dict["element_type"] = static_cast<int>(element.element_type);
            element_dict["flags"] = static_cast<int64_t>(element.flags);
            element_dict["text"] = to_godot_string(element.text);
            element_dict["action"] = ui_action_to_dictionary(element.action);
            elements.push_back(element_dict);
        }

        setup_dict["elements"] = elements;
        setups.push_back(setup_dict);
    }
    return setups;
}

Array Gs1RuntimeNode::build_ui_panels_array() const
{
    Array panels;
    for (const auto& panel : projection_cache_.state().active_ui_panels)
    {
        Dictionary panel_dict;
        panel_dict["panel_id"] = static_cast<int>(panel.panel_id);
        panel_dict["context_id"] = static_cast<int64_t>(panel.context_id);

        Array text_lines;
        for (const auto& text_line : panel.text_lines)
        {
            Dictionary line_dict;
            line_dict["line_id"] = static_cast<int>(text_line.line_id);
            line_dict["flags"] = static_cast<int64_t>(text_line.flags);
            line_dict["text"] = to_godot_string(text_line.text);
            text_lines.push_back(line_dict);
        }
        panel_dict["text_lines"] = text_lines;

        Array slot_actions;
        for (const auto& slot_action : panel.slot_actions)
        {
            Dictionary action_dict;
            action_dict["slot_id"] = static_cast<int>(slot_action.slot_id);
            action_dict["flags"] = static_cast<int64_t>(slot_action.flags);
            action_dict["label"] = to_godot_string(slot_action.label);
            action_dict["action"] = ui_action_to_dictionary(slot_action.action);
            slot_actions.push_back(action_dict);
        }
        panel_dict["slot_actions"] = slot_actions;

        Array list_items;
        for (const auto& list_item : panel.list_items)
        {
            Dictionary item_dict;
            item_dict["item_id"] = static_cast<int64_t>(list_item.item_id);
            item_dict["list_id"] = static_cast<int>(list_item.list_id);
            item_dict["flags"] = static_cast<int64_t>(list_item.flags);
            item_dict["primary_text"] = to_godot_string(list_item.primary_text);
            item_dict["secondary_text"] = to_godot_string(list_item.secondary_text);
            list_items.push_back(item_dict);
        }
        panel_dict["list_items"] = list_items;

        Array list_actions;
        for (const auto& list_action : panel.list_actions)
        {
            Dictionary action_dict;
            action_dict["item_id"] = static_cast<int64_t>(list_action.item_id);
            action_dict["list_id"] = static_cast<int>(list_action.list_id);
            action_dict["role"] = static_cast<int>(list_action.role);
            action_dict["flags"] = static_cast<int64_t>(list_action.flags);
            action_dict["action"] = ui_action_to_dictionary(list_action.action);
            list_actions.push_back(action_dict);
        }
        panel_dict["list_actions"] = list_actions;

        panels.push_back(panel_dict);
    }

    return panels;
}

Array Gs1RuntimeNode::build_inventory_slot_array(const std::vector<Gs1RuntimeInventorySlotProjection>& slots) const
{
    Array result;
    for (const auto& slot : slots)
    {
        Dictionary slot_dict;
        const auto* item_def = gs1::find_item_def(gs1::ItemId {slot.item_id});
        slot_dict["item_id"] = static_cast<int64_t>(slot.item_id);
        slot_dict["item_instance_id"] = static_cast<int64_t>(slot.item_instance_id);
        slot_dict["condition"] = slot.condition;
        slot_dict["freshness"] = slot.freshness;
        slot_dict["storage_id"] = static_cast<int64_t>(slot.storage_id);
        slot_dict["container_owner_id"] = static_cast<int64_t>(slot.container_owner_id);
        slot_dict["quantity"] = static_cast<int>(slot.quantity);
        slot_dict["slot_index"] = static_cast<int>(slot.slot_index);
        slot_dict["container_tile_x"] = slot.container_tile_x;
        slot_dict["container_tile_y"] = slot.container_tile_y;
        slot_dict["container_kind"] = static_cast<int>(slot.container_kind);
        slot_dict["flags"] = static_cast<int64_t>(slot.flags);
        slot_dict["item_name"] = item_display_name(slot.item_id);
        slot_dict["icon_content_kind"] = k_content_resource_kind_item;
        slot_dict["icon_content_id"] = static_cast<int64_t>(slot.item_id);
        slot_dict["icon_path"] = GodotProgressionResourceDatabase::instance().content_icon_path(
            static_cast<std::uint8_t>(k_content_resource_kind_item),
            slot.item_id);
        slot_dict["capability_flags"] = item_def != nullptr ? static_cast<int64_t>(item_def->capability_flags) : static_cast<int64_t>(0);
        slot_dict["linked_plant_id"] = item_def != nullptr ? static_cast<int64_t>(item_def->linked_plant_id.value) : static_cast<int64_t>(0);
        slot_dict["linked_structure_id"] = item_def != nullptr ? static_cast<int64_t>(item_def->linked_structure_id.value) : static_cast<int64_t>(0);
        result.push_back(slot_dict);
    }
    return result;
}

Dictionary Gs1RuntimeNode::build_active_site_dictionary() const
{
    Dictionary dict;
    if (!projection_cache_.state().active_site.has_value())
    {
        return dict;
    }

    const auto& site = projection_cache_.state().active_site.value();
    dict["site_id"] = static_cast<int64_t>(site.site_id);
    dict["site_archetype_id"] = static_cast<int64_t>(site.site_archetype_id);
    dict["width"] = static_cast<int>(site.width);
    dict["height"] = static_cast<int>(site.height);
    dict["worker_pack_open"] = site.worker_pack_open;

    Array tiles;
    for (std::size_t i = 0; i < site.tiles.size(); ++i)
    {
        tiles.push_back(get_tile_at_index(static_cast<int>(i)));
    }
    dict["tiles"] = tiles;

    if (site.worker.has_value())
    {
        Dictionary worker_dict;
        const auto& worker = site.worker.value();
        worker_dict["entity_id"] = static_cast<int64_t>(worker.entity_id);
        worker_dict["tile_x"] = worker.tile_x;
        worker_dict["tile_y"] = worker.tile_y;
        worker_dict["facing_degrees"] = worker.facing_degrees;
        worker_dict["health_normalized"] = worker.health_normalized;
        worker_dict["hydration_normalized"] = worker.hydration_normalized;
        worker_dict["energy_normalized"] = worker.energy_normalized;
        worker_dict["flags"] = static_cast<int>(worker.flags);
        worker_dict["current_action_kind"] = static_cast<int>(worker.current_action_kind);
        dict["worker"] = worker_dict;
    }

    if (site.weather.has_value())
    {
        Dictionary weather_dict;
        const auto& weather = site.weather.value();
        weather_dict["heat"] = weather.heat;
        weather_dict["wind"] = weather.wind;
        weather_dict["dust"] = weather.dust;
        weather_dict["wind_direction_degrees"] = weather.wind_direction_degrees;
        weather_dict["event_template_id"] = static_cast<int64_t>(weather.event_template_id);
        weather_dict["event_start_time_minutes"] = weather.event_start_time_minutes;
        weather_dict["event_peak_time_minutes"] = weather.event_peak_time_minutes;
        weather_dict["event_peak_duration_minutes"] = weather.event_peak_duration_minutes;
        weather_dict["event_end_time_minutes"] = weather.event_end_time_minutes;
        dict["weather"] = weather_dict;
    }

    Dictionary phone_panel;
    phone_panel["active_section"] = static_cast<int>(site.phone_panel.active_section);
    phone_panel["visible_task_count"] = static_cast<int64_t>(site.phone_panel.visible_task_count);
    phone_panel["accepted_task_count"] = static_cast<int64_t>(site.phone_panel.accepted_task_count);
    phone_panel["completed_task_count"] = static_cast<int64_t>(site.phone_panel.completed_task_count);
    phone_panel["claimed_task_count"] = static_cast<int64_t>(site.phone_panel.claimed_task_count);
    phone_panel["buy_listing_count"] = static_cast<int64_t>(site.phone_panel.buy_listing_count);
    phone_panel["sell_listing_count"] = static_cast<int64_t>(site.phone_panel.sell_listing_count);
    phone_panel["service_listing_count"] = static_cast<int64_t>(site.phone_panel.service_listing_count);
    phone_panel["cart_item_count"] = static_cast<int64_t>(site.phone_panel.cart_item_count);
    phone_panel["flags"] = static_cast<int64_t>(site.phone_panel.flags);
    dict["phone_panel"] = phone_panel;

    Dictionary overlay;
    overlay["mode"] = static_cast<int>(site.protection_overlay.mode);
    dict["protection_overlay"] = overlay;

    Array storages;
    for (const auto& storage : site.inventory_storages)
    {
        Dictionary storage_dict;
        const auto* tile = find_tile_at(site, storage.tile_x, storage.tile_y);
        const auto structure_id = tile != nullptr ? tile->structure_type_id : 0U;
        const auto* structure_def = structure_id != 0U ? gs1::find_structure_def(gs1::StructureId {structure_id}) : nullptr;
        storage_dict["storage_id"] = static_cast<int64_t>(storage.storage_id);
        storage_dict["owner_entity_id"] = static_cast<int64_t>(storage.owner_entity_id);
        storage_dict["slot_count"] = static_cast<int>(storage.slot_count);
        storage_dict["tile_x"] = storage.tile_x;
        storage_dict["tile_y"] = storage.tile_y;
        storage_dict["container_kind"] = static_cast<int>(storage.container_kind);
        storage_dict["flags"] = static_cast<int64_t>(storage.flags);
        storage_dict["structure_type_id"] = static_cast<int64_t>(structure_id);
        if (structure_def != nullptr)
        {
            storage_dict["label"] = to_godot_string(structure_def->display_name);
        }
        else if ((storage.flags & GS1_INVENTORY_STORAGE_FLAG_DELIVERY_BOX) != 0U)
        {
            storage_dict["label"] = "Delivery Box";
        }
        else if (storage.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
        {
            storage_dict["label"] = "Worker Pack";
        }
        else
        {
            storage_dict["label"] = String("Storage ") + String::num_int64(storage.storage_id);
        }
        storage_dict["is_crafting_station"] = structure_def != nullptr && structure_def->crafting_station_kind != gs1::CraftingStationKind::None;
        storages.push_back(storage_dict);
    }
    dict["inventory_storages"] = storages;
    dict["worker_pack_slots"] = build_inventory_slot_array(site.worker_pack_slots);

    if (site.opened_storage.has_value())
    {
        Dictionary opened_storage;
        opened_storage["storage_id"] = static_cast<int64_t>(site.opened_storage->storage_id);
        opened_storage["slot_count"] = static_cast<int>(site.opened_storage->slot_count);
        opened_storage["slots"] = build_inventory_slot_array(site.opened_storage->slots);
        dict["opened_storage"] = opened_storage;
    }

    if (site.craft_context.has_value())
    {
        Dictionary craft_context;
        craft_context["tile_x"] = site.craft_context->tile_x;
        craft_context["tile_y"] = site.craft_context->tile_y;
        craft_context["flags"] = static_cast<int64_t>(site.craft_context->flags);
        Array options;
        for (const auto& option : site.craft_context->options)
        {
            Dictionary option_dict;
            option_dict["recipe_id"] = static_cast<int64_t>(option.recipe_id);
            option_dict["output_item_id"] = static_cast<int64_t>(option.output_item_id);
            option_dict["flags"] = static_cast<int64_t>(option.flags);
            option_dict["output_item_name"] = recipe_output_name(option.recipe_id, option.output_item_id);
            option_dict["icon_content_kind"] = k_content_resource_kind_item;
            option_dict["icon_content_id"] = static_cast<int64_t>(option.output_item_id);
            option_dict["icon_path"] = GodotProgressionResourceDatabase::instance().content_icon_path(
                static_cast<std::uint8_t>(k_content_resource_kind_item),
                option.output_item_id);
            options.push_back(option_dict);
        }
        craft_context["options"] = options;
        dict["craft_context"] = craft_context;
    }

    if (site.placement_preview.has_value())
    {
        Dictionary preview;
        preview["tile_x"] = site.placement_preview->tile_x;
        preview["tile_y"] = site.placement_preview->tile_y;
        preview["blocked_mask"] = static_cast<int64_t>(site.placement_preview->blocked_mask);
        preview["item_id"] = static_cast<int64_t>(site.placement_preview->item_id);
        preview["item_name"] = site.placement_preview->item_id != 0U ? item_display_name(site.placement_preview->item_id) : String();
        preview["icon_content_kind"] = k_content_resource_kind_item;
        preview["icon_content_id"] = static_cast<int64_t>(site.placement_preview->item_id);
        preview["icon_path"] = site.placement_preview->item_id != 0U
            ? GodotProgressionResourceDatabase::instance().content_icon_path(
                static_cast<std::uint8_t>(k_content_resource_kind_item),
                site.placement_preview->item_id)
            : String();
        preview["preview_tile_count"] = static_cast<int64_t>(site.placement_preview->preview_tile_count);
        preview["footprint_width"] = static_cast<int64_t>(site.placement_preview->footprint_width);
        preview["footprint_height"] = static_cast<int64_t>(site.placement_preview->footprint_height);
        preview["action_kind"] = static_cast<int>(site.placement_preview->action_kind);
        preview["flags"] = static_cast<int64_t>(site.placement_preview->flags);
        dict["placement_preview"] = preview;
    }

    Array preview_tiles;
    for (const auto& preview_tile : site.placement_preview_tiles)
    {
        Dictionary preview_tile_dict;
        preview_tile_dict["x"] = preview_tile.x;
        preview_tile_dict["y"] = preview_tile.y;
        preview_tile_dict["flags"] = static_cast<int64_t>(preview_tile.flags);
        preview_tile_dict["wind_protection"] = preview_tile.wind_protection;
        preview_tile_dict["heat_protection"] = preview_tile.heat_protection;
        preview_tile_dict["dust_protection"] = preview_tile.dust_protection;
        preview_tile_dict["final_wind_protection"] = preview_tile.final_wind_protection;
        preview_tile_dict["final_heat_protection"] = preview_tile.final_heat_protection;
        preview_tile_dict["final_dust_protection"] = preview_tile.final_dust_protection;
        preview_tile_dict["occupant_condition"] = preview_tile.occupant_condition;
        preview_tiles.push_back(preview_tile_dict);
    }
    dict["placement_preview_tiles"] = preview_tiles;

    if (site.placement_failure.has_value())
    {
        Dictionary failure;
        failure["tile_x"] = site.placement_failure->tile_x;
        failure["tile_y"] = site.placement_failure->tile_y;
        failure["blocked_mask"] = static_cast<int64_t>(site.placement_failure->blocked_mask);
        failure["sequence_id"] = static_cast<int64_t>(site.placement_failure->sequence_id);
        failure["action_kind"] = static_cast<int>(site.placement_failure->action_kind);
        failure["flags"] = static_cast<int64_t>(site.placement_failure->flags);
        dict["placement_failure"] = failure;
    }

    Array tasks;
    for (const auto& task : site.tasks)
    {
        Dictionary task_dict;
        task_dict["task_instance_id"] = static_cast<int64_t>(task.task_instance_id);
        task_dict["task_template_id"] = static_cast<int64_t>(task.task_template_id);
        task_dict["publisher_faction_id"] = static_cast<int64_t>(task.publisher_faction_id);
        task_dict["current_progress"] = static_cast<int>(task.current_progress);
        task_dict["target_progress"] = static_cast<int>(task.target_progress);
        task_dict["list_kind"] = static_cast<int>(task.list_kind);
        task_dict["flags"] = static_cast<int64_t>(task.flags);
        task_dict["resource_icon_path"] =
            GodotProgressionResourceDatabase::instance().task_template_icon_path(task.task_template_id);
        if (const auto* task_def = gs1::find_task_template_def(gs1::TaskTemplateId {task.task_template_id}))
        {
            task_dict["task_tier_id"] = static_cast<int64_t>(task_def->task_tier_id);
            task_dict["progress_kind"] = static_cast<int64_t>(task_def->progress_kind);
        }
        tasks.push_back(task_dict);
    }
    dict["tasks"] = tasks;

    Array modifiers;
    for (const auto& modifier : site.active_modifiers)
    {
        Dictionary modifier_dict;
        modifier_dict["modifier_id"] = static_cast<int64_t>(modifier.modifier_id);
        modifier_dict["remaining_game_hours"] = static_cast<int>(modifier.remaining_game_hours);
        modifier_dict["flags"] = static_cast<int64_t>(modifier.flags);
        modifier_dict["resource_icon_path"] =
            GodotProgressionResourceDatabase::instance().modifier_icon_path(modifier.modifier_id);
        if (const auto* modifier_def = gs1::find_modifier_def(gs1::ModifierId {modifier.modifier_id}))
        {
            modifier_dict["display_name"] = to_godot_string(modifier_def->display_name);
            modifier_dict["description"] = to_godot_string(modifier_def->description);
        }
        modifiers.push_back(modifier_dict);
    }
    dict["active_modifiers"] = modifiers;

    Array phone_listings;
    for (const auto& listing : site.phone_listings)
    {
        Dictionary listing_dict;
        listing_dict["listing_id"] = static_cast<int64_t>(listing.listing_id);
        listing_dict["item_or_unlockable_id"] = static_cast<int64_t>(listing.item_or_unlockable_id);
        listing_dict["price"] = listing.price;
        listing_dict["related_site_id"] = static_cast<int64_t>(listing.related_site_id);
        listing_dict["quantity"] = static_cast<int>(listing.quantity);
        listing_dict["cart_quantity"] = static_cast<int>(listing.cart_quantity);
        listing_dict["listing_kind"] = static_cast<int>(listing.listing_kind);
        listing_dict["flags"] = static_cast<int64_t>(listing.flags);
        const bool is_unlockable = listing.listing_kind == GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE;
        listing_dict["label"] = item_display_name(listing.item_or_unlockable_id);
        listing_dict["icon_content_kind"] = k_content_resource_kind_item;
        listing_dict["icon_content_id"] = static_cast<int64_t>(listing.item_or_unlockable_id);
        listing_dict["is_unlockable"] = is_unlockable;
        listing_dict["icon_path"] = !is_unlockable
            ? GodotProgressionResourceDatabase::instance().content_icon_path(
                static_cast<std::uint8_t>(k_content_resource_kind_item),
                listing.item_or_unlockable_id)
            : String();
        phone_listings.push_back(listing_dict);
    }
    dict["phone_listings"] = phone_listings;

    return dict;
}

Array Gs1RuntimeNode::build_recent_one_shot_cues_array() const
{
    Array cues;
    for (const auto& cue : projection_cache_.state().recent_one_shot_cues)
    {
        Dictionary cue_dict;
        cue_dict["sequence_id"] = static_cast<int64_t>(cue.sequence_id);
        cue_dict["subject_id"] = static_cast<int64_t>(cue.subject_id);
        cue_dict["arg0"] = static_cast<int64_t>(cue.arg0);
        cue_dict["arg1"] = static_cast<int64_t>(cue.arg1);
        cue_dict["world_x"] = cue.world_x;
        cue_dict["world_y"] = cue.world_y;
        cue_dict["cue_kind"] = static_cast<int>(cue.cue_kind);
        cue_dict["cue_name"] = one_shot_cue_name(cue.cue_kind);
        cues.push_back(cue_dict);
    }
    return cues;
}

void Gs1RuntimeNode::refresh_gameplay_dll_path()
{
    gameplay_dll_path_ = std::filesystem::path {compute_default_gameplay_dll_path()};
}

void Gs1RuntimeNode::refresh_project_config_root()
{
    project_config_root_ = std::filesystem::path {compute_default_project_config_root()};
}

std::string Gs1RuntimeNode::compute_default_gameplay_dll_path() const
{
    return "bin/win64/gs1_game.dll";
}

std::string Gs1RuntimeNode::compute_default_project_config_root() const
{
    return "project";
}
