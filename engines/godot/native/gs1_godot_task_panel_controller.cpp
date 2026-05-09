#include "gs1_godot_task_panel_controller.h"

#include "gs1_godot_controller_context.h"
#include "godot_progression_resources.h"

#include "content/defs/task_defs.h"
#include "host/adapter_metadata_catalog.h"

#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <algorithm>
#include <string_view>
#include <utility>

using namespace godot;

namespace
{
template <typename T>
T* resolve_object(const ObjectID object_id)
{
    return Object::cast_to<T>(ObjectDB::get_instance(object_id));
}

String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
}

Ref<Texture2D> load_texture_2d(const String& path)
{
    if (path.is_empty())
    {
        return Ref<Texture2D> {};
    }

    if (ResourceLoader* loader = ResourceLoader::get_singleton())
    {
        return loader->load(path);
    }
    return Ref<Texture2D> {};
}
}

void Gs1GodotTaskPanelController::_bind_methods()
{
}

void Gs1GodotTaskPanelController::_ready()
{
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
    set_process(true);
}

void Gs1GodotTaskPanelController::_process(double delta)
{
    (void)delta;
}

void Gs1GodotTaskPanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotTaskPanelController::cache_ui_references(Control& owner)
{
    owner_control_ = &owner;
    if (panel_ == nullptr)
    {
        panel_ = Object::cast_to<Control>(owner.find_child("TaskPanel", true, false));
    }
    if (task_summary_ == nullptr)
    {
        task_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("TaskSummary", true, false));
    }
    if (task_rows_ == nullptr)
    {
        task_rows_ = Object::cast_to<VBoxContainer>(owner.find_child("TaskRows", true, false));
    }
    if (modifier_rows_ == nullptr)
    {
        modifier_rows_ = Object::cast_to<VBoxContainer>(owner.find_child("ModifierRows", true, false));
    }
    update_task_summary();
    reconcile_task_rows();
    reconcile_modifier_rows();
}

void Gs1GodotTaskPanelController::cache_adapter_service()
{
    if (adapter_service_ != nullptr)
    {
        return;
    }

    adapter_service_ = gs1_resolve_adapter_service(this);
    if (adapter_service_ != nullptr)
    {
        adapter_service_->subscribe_matching_messages(*this);
    }
}

Control* Gs1GodotTaskPanelController::resolve_owner_control()
{
    if (owner_control_ != nullptr)
    {
        return owner_control_;
    }
    owner_control_ = Object::cast_to<Control>(get_parent());
    if (owner_control_ == nullptr)
    {
        owner_control_ = this;
    }
    return owner_control_;
}

bool Gs1GodotTaskPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_TASK_PANEL_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_REMOVE:
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_LIST_BEGIN:
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_UPSERT:
    case GS1_ENGINE_MESSAGE_END_SITE_TASK_PANEL_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1GodotTaskPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_TASK_PANEL_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
        in_site_snapshot_ = true;
        pending_task_indices_.clear();
        pending_modifier_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            tasks_.clear();
            active_modifiers_.clear();
        }
        else
        {
            for (std::size_t index = 0; index < tasks_.size(); ++index)
            {
                pending_task_indices_[tasks_[index].task_instance_id] = index;
            }
            for (std::size_t index = 0; index < active_modifiers_.size(); ++index)
            {
                pending_modifier_indices_[active_modifiers_[index].modifier_id] = index;
            }
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_UPSERT:
    {
        Gs1RuntimeTaskProjection projection = message.payload_as<Gs1EngineMessageTaskData>();
        const auto found = pending_task_indices_.find(projection.task_instance_id);
        if (found != pending_task_indices_.end() && found->second < tasks_.size())
        {
            tasks_[found->second] = projection;
        }
        else
        {
            pending_task_indices_[projection.task_instance_id] = tasks_.size();
            tasks_.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_TASK_REMOVE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageTaskData>();
        const auto found = pending_task_indices_.find(payload.task_instance_id);
        if (found != pending_task_indices_.end() && found->second < tasks_.size())
        {
            const std::size_t index = found->second;
            const std::size_t last_index = tasks_.size() - 1U;
            if (index != last_index)
            {
                tasks_[index] = std::move(tasks_[last_index]);
                pending_task_indices_[tasks_[index].task_instance_id] = index;
            }
            tasks_.pop_back();
            pending_task_indices_.erase(found);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_LIST_BEGIN:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteModifierListData>();
        pending_modifier_indices_.clear();
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            active_modifiers_.clear();
        }
        else
        {
            for (std::size_t index = 0; index < active_modifiers_.size(); ++index)
            {
                pending_modifier_indices_[active_modifiers_[index].modifier_id] = index;
            }
        }
        active_modifiers_.reserve(payload.modifier_count);
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_TASK_PANEL_MODIFIER_UPSERT:
    {
        Gs1RuntimeModifierProjection projection = message.payload_as<Gs1EngineMessageSiteModifierData>();
        const auto found = pending_modifier_indices_.find(projection.modifier_id);
        if (found != pending_modifier_indices_.end() && found->second < active_modifiers_.size())
        {
            active_modifiers_[found->second] = projection;
        }
        else
        {
            pending_modifier_indices_[projection.modifier_id] = active_modifiers_.size();
            active_modifiers_.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_SITE_TASK_PANEL_SNAPSHOT:
        std::sort(tasks_.begin(), tasks_.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.list_kind != rhs.list_kind)
            {
                return lhs.list_kind < rhs.list_kind;
            }
            return lhs.task_instance_id < rhs.task_instance_id;
        });
        std::sort(active_modifiers_.begin(), active_modifiers_.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.flags != rhs.flags)
            {
                return lhs.flags > rhs.flags;
            }
            return lhs.modifier_id < rhs.modifier_id;
        });
        pending_task_indices_.clear();
        pending_modifier_indices_.clear();
        in_site_snapshot_ = false;
        break;
    default:
        break;
    }

    update_task_summary();
    reconcile_task_rows();
    reconcile_modifier_rows();
}

void Gs1GodotTaskPanelController::handle_runtime_message_reset()
{
    tasks_.clear();
    active_modifiers_.clear();
    pending_task_indices_.clear();
    pending_modifier_indices_.clear();
    in_site_snapshot_ = false;
    prune_button_registry(task_buttons_, {});
    prune_button_registry(modifier_buttons_, {});
    update_task_summary();
}

void Gs1GodotTaskPanelController::update_task_summary()
{
    if (task_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Tasks[/b]");
    for (const auto& task : tasks_)
    {
        lines.push_back(vformat(
            "Task %d  progress %d / %d  list %d",
            static_cast<int>(task.task_template_id),
            static_cast<int>(task.current_progress),
            static_cast<int>(task.target_progress),
            static_cast<int>(task.list_kind)));
    }

    task_summary_->set_text(String("\n").join(lines));
}

void Gs1GodotTaskPanelController::reconcile_task_rows()
{
    if (task_rows_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (const auto& task : tasks_)
    {
        const int task_instance_id = static_cast<int>(task.task_instance_id);
        const std::uint64_t stable_key = static_cast<std::uint64_t>(task_instance_id);
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            task_rows_,
            task_buttons_,
            stable_key,
            vformat("DynamicTask_%d", task_instance_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const TaskTemplateUiCacheEntry& ui_entry = task_template_ui_for(task.task_template_id);
        if (ui_entry.icon_texture.is_valid())
        {
            button->set_button_icon(ui_entry.icon_texture);
        }
        else
        {
            button->set_button_icon(Ref<Texture2D> {});
        }

        button->set_text(vformat(
            "Task %d  progress %d/%d  tier %d",
            static_cast<int>(task.task_template_id),
            static_cast<int>(task.current_progress),
            static_cast<int>(task.target_progress),
            ui_entry.task_tier_id));
        button->set_tooltip_text(vformat(
            "Task template %d, progress kind %d, list %d",
            static_cast<int>(task.task_template_id),
            ui_entry.progress_kind,
            static_cast<int>(task.list_kind)));
        button->set_disabled(true);
        button->set_focus_mode(Control::FOCUS_NONE);
    }

    prune_button_registry(task_buttons_, desired_keys);
}

void Gs1GodotTaskPanelController::reconcile_modifier_rows()
{
    if (modifier_rows_ == nullptr)
    {
        return;
    }

    std::unordered_set<std::uint64_t> desired_keys;
    int desired_index = 0;
    for (std::size_t index = 0; index < active_modifiers_.size(); ++index)
    {
        const auto& modifier = active_modifiers_[index];
        const int modifier_id = static_cast<int>(modifier.modifier_id);
        const std::uint64_t stable_key =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(modifier_id)) << 32U) |
            static_cast<std::uint32_t>(index);
        desired_keys.insert(stable_key);
        Button* button = upsert_button_node(
            modifier_rows_,
            modifier_buttons_,
            stable_key,
            vformat("DynamicModifier_%d", modifier_id),
            desired_index++);
        if (button == nullptr)
        {
            continue;
        }

        const ModifierUiCacheEntry& ui_entry = modifier_ui_for(modifier.modifier_id);
        if (ui_entry.icon_texture.is_valid())
        {
            button->set_button_icon(ui_entry.icon_texture);
        }
        else
        {
            button->set_button_icon(Ref<Texture2D> {});
        }
        button->set_text(vformat(
            "%s  %s",
            ui_entry.label,
            (modifier.flags & 1U) != 0
                ? vformat("%dh left", static_cast<int>(modifier.remaining_game_hours))
                : String("Permanent")));
        const auto* metadata = adapter_metadata_catalog().find(
            Gs1AdapterMetadataDomain::Modifier,
            modifier.modifier_id);
        button->set_tooltip_text(
            metadata == nullptr || metadata->description.empty()
                ? String()
                : string_from_view(metadata->description));
        button->set_disabled(true);
        button->set_focus_mode(Control::FOCUS_NONE);
    }

    prune_button_registry(modifier_buttons_, desired_keys);
}

Button* Gs1GodotTaskPanelController::upsert_button_node(
    Node* container,
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    std::uint64_t stable_key,
    const String& node_name,
    int desired_index)
{
    if (container == nullptr)
    {
        return nullptr;
    }

    Button* button = nullptr;
    auto found = registry.find(stable_key);
    if (found != registry.end())
    {
        button = resolve_object<Button>(found->second.object_id);
    }

    if (button == nullptr)
    {
        button = memnew(Button);
        button->set_name(node_name);
        button->set_clip_text(true);
        button->set_custom_minimum_size(Vector2(0.0F, 44.0F));
        container->add_child(button);
        registry[stable_key].object_id = button->get_instance_id();
    }

    const int child_count = container->get_child_count();
    const int clamped_index = std::clamp(desired_index, 0, child_count - 1);
    if (button->get_index() != clamped_index)
    {
        container->move_child(button, clamped_index);
    }

    return button;
}

void Gs1GodotTaskPanelController::prune_button_registry(
    std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
    const std::unordered_set<std::uint64_t>& desired_keys)
{
    for (auto it = registry.begin(); it != registry.end();)
    {
        if (desired_keys.contains(it->first))
        {
            ++it;
            continue;
        }

        if (Button* button = resolve_object<Button>(it->second.object_id))
        {
            button->queue_free();
        }
        it = registry.erase(it);
    }
}

Ref<Texture2D> Gs1GodotTaskPanelController::load_cached_texture(const String& path) const
{
    if (path.is_empty())
    {
        return Ref<Texture2D> {};
    }

    const std::string key = path.utf8().get_data();
    auto found = texture_cache_.find(key);
    if (found != texture_cache_.end())
    {
        return found->second;
    }

    return texture_cache_.emplace(key, load_texture_2d(path)).first->second;
}

const Gs1GodotTaskPanelController::TaskTemplateUiCacheEntry&
Gs1GodotTaskPanelController::task_template_ui_for(std::uint32_t task_template_id) const
{
    auto found = task_template_ui_cache_.find(task_template_id);
    if (found != task_template_ui_cache_.end())
    {
        return found->second;
    }

    TaskTemplateUiCacheEntry entry {};
    entry.icon_texture = load_cached_texture(
        GodotProgressionResourceDatabase::instance().task_template_icon_path(task_template_id));
    if (const auto* task_def = gs1::find_task_template_def(gs1::TaskTemplateId {task_template_id}))
    {
        entry.task_tier_id = static_cast<int>(task_def->task_tier_id);
        entry.progress_kind = static_cast<int>(task_def->progress_kind);
    }

    return task_template_ui_cache_.emplace(task_template_id, std::move(entry)).first->second;
}

const Gs1GodotTaskPanelController::ModifierUiCacheEntry&
Gs1GodotTaskPanelController::modifier_ui_for(std::uint32_t modifier_id) const
{
    auto found = modifier_ui_cache_.find(modifier_id);
    if (found != modifier_ui_cache_.end())
    {
        return found->second;
    }

    ModifierUiCacheEntry entry {};
    entry.icon_texture = load_cached_texture(
        GodotProgressionResourceDatabase::instance().modifier_icon_path(modifier_id));
    const auto* metadata = adapter_metadata_catalog().find(
        Gs1AdapterMetadataDomain::Modifier,
        modifier_id);
    entry.label = metadata == nullptr || metadata->display_name.empty()
        ? vformat("Modifier %d", static_cast<int>(modifier_id))
        : string_from_view(metadata->display_name);
    return modifier_ui_cache_.emplace(modifier_id, std::move(entry)).first->second;
}
