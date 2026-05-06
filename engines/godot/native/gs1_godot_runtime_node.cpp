#include "gs1_godot_runtime_node.h"

#include "godot_progression_resources.h"
#include "gs1_godot_projection_types.h"

#include "host/adapter_metadata_catalog.h"

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/project_settings.hpp>
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

std::filesystem::path globalize_res_path(std::string_view res_path)
{
    godot::ProjectSettings* project_settings = godot::ProjectSettings::get_singleton();
    if (project_settings == nullptr)
    {
        return std::filesystem::path {res_path};
    }

    const godot::String absolute_path = project_settings->globalize_path(
        String::utf8(res_path.data(), static_cast<int64_t>(res_path.size())));
    return std::filesystem::path {absolute_path.utf8().get_data()};
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
        globalize_res_path("res://gs1/godot_config"));

    notify_runtime_message_reset();
    if (!runtime_session_.start(gameplay_dll_path_, project_config_root_, &adapter_config_))
    {
        last_error_ = runtime_session_.last_error();
        return;
    }

    if (!drain_projection_messages())
    {
        runtime_session_.stop();
        notify_runtime_message_reset();
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
        notify_runtime_message_reset();
        return;
    }

    if (!drain_projection_messages())
    {
        runtime_session_.stop();
        notify_runtime_message_reset();
        return;
    }

    last_error_.clear();
}

bool Gs1RuntimeNode::drain_projection_messages()
{
    const Gs1RuntimeApi* api = runtime_session_.api();
    if (api == nullptr || api->pop_engine_message == nullptr || runtime_session_.runtime() == nullptr)
    {
        last_error_ = "Runtime session is not ready to drain engine messages.";
        return false;
    }

    Gs1EngineMessage message {};
    while (true)
    {
        const Gs1Status status = api->pop_engine_message(runtime_session_.runtime(), &message);
        if (status == GS1_STATUS_BUFFER_EMPTY)
        {
            break;
        }
        if (status != GS1_STATUS_OK)
        {
            last_error_ = "gs1_pop_engine_message failed with status " + std::to_string(static_cast<unsigned>(status));
            return false;
        }

        dispatch_engine_message(message);
    }

    return true;
}

void Gs1RuntimeNode::subscribe_engine_messages(IGs1GodotEngineMessageSubscriber& subscriber)
{
    if (std::find(engine_message_subscribers_.begin(), engine_message_subscribers_.end(), &subscriber) != engine_message_subscribers_.end())
    {
        return;
    }
    engine_message_subscribers_.push_back(&subscriber);
    subscriber.handle_runtime_message_reset();
}

void Gs1RuntimeNode::unsubscribe_engine_messages(IGs1GodotEngineMessageSubscriber& subscriber)
{
    engine_message_subscribers_.erase(
        std::remove(engine_message_subscribers_.begin(), engine_message_subscribers_.end(), &subscriber),
        engine_message_subscribers_.end());
}

void Gs1RuntimeNode::notify_runtime_message_reset()
{
    for (IGs1GodotEngineMessageSubscriber* subscriber : engine_message_subscribers_)
    {
        if (subscriber != nullptr)
        {
            subscriber->handle_runtime_message_reset();
        }
    }
}

void Gs1RuntimeNode::dispatch_engine_message(const Gs1EngineMessage& message)
{
    const std::vector<IGs1GodotEngineMessageSubscriber*> subscribers = engine_message_subscribers_;
    for (IGs1GodotEngineMessageSubscriber* subscriber : subscribers)
    {
        if (subscriber == nullptr || !subscriber->handles_engine_message(message.type))
        {
            continue;
        }
        subscriber->handle_engine_message(message);
    }
}

bool Gs1RuntimeNode::submit_ui_action(const Gs1UiAction& action)
{
    Gs1HostEvent event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    if (!runtime_session_.submit_host_events(&event, 1U))
    {
        last_error_ = runtime_session_.last_error();
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

void Gs1RuntimeNode::refresh_gameplay_dll_path()
{
    gameplay_dll_path_ = std::filesystem::path {compute_default_gameplay_dll_path()};
}

void Gs1RuntimeNode::refresh_project_config_root()
{
    project_config_root_ = std::filesystem::path {compute_default_project_config_root()};
    load_adapter_metadata_catalog_from_project_root(project_config_root_);
}

std::string Gs1RuntimeNode::compute_default_gameplay_dll_path() const
{
    return globalize_res_path("res://bin/win64/gs1_game.dll").string();
}

std::string Gs1RuntimeNode::compute_default_project_config_root() const
{
    return globalize_res_path("res://gs1/runtime").string();
}
