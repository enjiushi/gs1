#include "gs1_godot_inventory_panel_controller.h"

#include "gs1_godot_controller_context.h"
#include "content/defs/item_defs.h"
#include "godot_progression_resources.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

#include <algorithm>
#include <string_view>

using namespace godot;

namespace
{
constexpr std::uint8_t k_inventory_slot_flag_occupied = 1U;
constexpr std::uint8_t k_content_resource_kind_item = 1U;

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

template <typename T>
T* resolve_object(const ObjectID object_id)
{
    return Object::cast_to<T>(ObjectDB::get_instance(object_id));
}

Gs1GodotInventoryPanelController* resolve_controller(std::int64_t controller_bits)
{
    return reinterpret_cast<Gs1GodotInventoryPanelController*>(static_cast<std::uintptr_t>(controller_bits));
}

void dispatch_slot_pressed(std::int64_t controller_bits, std::int64_t slot_key)
{
    if (Gs1GodotInventoryPanelController* controller = resolve_controller(controller_bits))
    {
        controller->handle_slot_pressed(slot_key);
    }
}
}

void Gs1GodotInventoryPanelController::_bind_methods()
{
}

void Gs1GodotInventoryPanelController::_ready()
{
    set_submit_inventory_slot_tap_callback([this](int storage_id, int container_kind, int slot_index, int item_instance_id) {
        submit_inventory_slot_tap(storage_id, container_kind, slot_index, item_instance_id);
    });
    cache_adapter_service();
    if (Control* owner = resolve_owner_control())
    {
        cache_ui_references(*owner);
    }
    set_process(true);
}

void Gs1GodotInventoryPanelController::_process(double delta)
{
    (void)delta;
    refresh_if_needed();
}

void Gs1GodotInventoryPanelController::_exit_tree()
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->unsubscribe_all(*this);
        adapter_service_ = nullptr;
    }
}

void Gs1GodotInventoryPanelController::cache_ui_references(Control& owner)
{
    if (panel_ == nullptr)
    {
        panel_ = Object::cast_to<Control>(owner.find_child("InventoryPanel", true, false));
    }
    if (inventory_title_ == nullptr)
    {
        inventory_title_ = Object::cast_to<Label>(owner.find_child("InventoryTitle", true, false));
    }
    if (worker_pack_title_ == nullptr)
    {
        worker_pack_title_ = Object::cast_to<Label>(owner.find_child("WorkerPackTitle", true, false));
    }
    if (worker_pack_slots_grid_ == nullptr)
    {
        worker_pack_slots_grid_ = Object::cast_to<GridContainer>(owner.find_child("WorkerPackSlots", true, false));
    }
    if (opened_storage_title_ == nullptr)
    {
        opened_storage_title_ = Object::cast_to<Label>(owner.find_child("OpenedStorageTitle", true, false));
    }
    if (opened_storage_slots_grid_ == nullptr)
    {
        opened_storage_slots_grid_ = Object::cast_to<GridContainer>(owner.find_child("OpenedStorageSlots", true, false));
    }
    refresh_if_needed();
}

void Gs1GodotInventoryPanelController::set_submit_inventory_slot_tap_callback(SubmitInventorySlotTapFn callback)
{
    submit_inventory_slot_tap_ = std::move(callback);
}

void Gs1GodotInventoryPanelController::cache_adapter_service()
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

Control* Gs1GodotInventoryPanelController::resolve_owner_control()
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

void Gs1GodotInventoryPanelController::submit_inventory_slot_tap(
    int storage_id,
    int container_kind,
    int slot_index,
    int item_instance_id)
{
    if (adapter_service_ != nullptr)
    {
        adapter_service_->submit_site_inventory_slot_tap(storage_id, container_kind, slot_index, item_instance_id);
    }
}

bool Gs1GodotInventoryPanelController::handles_engine_message(Gs1EngineMessageType type) const noexcept
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void Gs1GodotInventoryPanelController::handle_engine_message(const Gs1EngineMessage& message)
{
    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        current_app_state_ = payload.app_state;
        if (payload.app_state == GS1_APP_STATE_MAIN_MENU ||
            payload.app_state == GS1_APP_STATE_REGIONAL_MAP ||
            payload.app_state == GS1_APP_STATE_CAMPAIGN_END)
        {
            inventory_storages_.clear();
            worker_pack_slots_.clear();
            opened_storage_.reset();
            worker_pack_open_ = false;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
        in_site_snapshot_ = true;
        if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
        {
            inventory_storages_.clear();
            worker_pack_slots_.clear();
            opened_storage_.reset();
            worker_pack_open_ = false;
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
    {
        const auto projection = message.payload_as<Gs1EngineMessageInventoryStorageData>();
        const auto found = std::find_if(
            inventory_storages_.begin(),
            inventory_storages_.end(),
            [&](const auto& existing) { return existing.storage_id == projection.storage_id; });
        if (found != inventory_storages_.end())
        {
            *found = projection;
        }
        else
        {
            inventory_storages_.push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
    {
        const auto projection = message.payload_as<Gs1EngineMessageInventorySlotData>();
        std::vector<Gs1RuntimeInventorySlotProjection>* slots = nullptr;
        if (projection.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
        {
            slots = &worker_pack_slots_;
        }
        else if (opened_storage_.has_value() && opened_storage_->storage_id == projection.storage_id)
        {
            slots = &opened_storage_->slots;
        }
        if (slots == nullptr)
        {
            break;
        }
        const auto found = std::find_if(
            slots->begin(),
            slots->end(),
            [&](const auto& existing) {
                return existing.storage_id == projection.storage_id && existing.slot_index == projection.slot_index;
            });
        if (found != slots->end())
        {
            *found = projection;
        }
        else
        {
            slots->push_back(projection);
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageInventoryViewData>();
        const auto* storage = find_storage(payload.storage_id);
        const bool is_worker_pack_storage =
            storage != nullptr && storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;

        if (is_worker_pack_storage)
        {
            worker_pack_open_ = payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT;
            break;
        }

        if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE)
        {
            if (opened_storage_.has_value() && opened_storage_->storage_id == payload.storage_id)
            {
                opened_storage_.reset();
            }
            break;
        }

        if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            opened_storage_ = Gs1RuntimeInventoryViewProjection {};
            opened_storage_->storage_id = payload.storage_id;
            opened_storage_->slot_count = payload.slot_count;
            opened_storage_->slots.clear();
        }
        break;
    }
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        in_site_snapshot_ = false;
        break;
    default:
        break;
    }

    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotInventoryPanelController::handle_runtime_message_reset()
{
    current_app_state_.reset();
    inventory_storages_.clear();
    worker_pack_slots_.clear();
    opened_storage_.reset();
    prune_slot_registry(worker_pack_slot_buttons_, {});
    prune_slot_registry(opened_storage_slot_buttons_, {});
    worker_pack_open_ = false;
    in_site_snapshot_ = false;
    dirty_ = true;
    refresh_if_needed();
}

void Gs1GodotInventoryPanelController::refresh_if_needed()
{
    if (!dirty_)
    {
        return;
    }

    const int app_state = current_app_state_.has_value() ? static_cast<int>(current_app_state_.value()) : 0;
    const bool site_visible = app_state >= GS1_APP_STATE_SITE_LOADING && app_state <= GS1_APP_STATE_SITE_RESULT;
    const bool panel_visible = site_visible && (worker_pack_open_ || opened_storage_.has_value());
    if (panel_ != nullptr)
    {
        panel_->set_visible(panel_visible);
    }
    if (!panel_visible)
    {
        prune_slot_registry(worker_pack_slot_buttons_, {});
        prune_slot_registry(opened_storage_slot_buttons_, {});
        dirty_ = false;
        return;
    }

    if (inventory_title_ != nullptr)
    {
        inventory_title_->set_text("Inventory");
    }

    const auto* worker_pack_storage = find_worker_pack_storage();
    const std::uint16_t worker_pack_slot_count =
        worker_pack_storage != nullptr ? worker_pack_storage->slot_count : static_cast<std::uint16_t>(worker_pack_slots_.size());
    if (worker_pack_title_ != nullptr)
    {
        worker_pack_title_->set_text(container_title_for(worker_pack_storage));
    }
    reconcile_slot_grid(
        worker_pack_slots_grid_,
        worker_pack_slot_buttons_,
        GS1_INVENTORY_CONTAINER_WORKER_PACK,
        worker_pack_storage != nullptr ? worker_pack_storage->storage_id : 0U,
        worker_pack_slot_count,
        worker_pack_slots_);

    if (opened_storage_.has_value())
    {
        const auto* storage = find_storage(opened_storage_->storage_id);
        if (opened_storage_title_ != nullptr)
        {
            opened_storage_title_->set_visible(true);
            opened_storage_title_->set_text(container_title_for(storage));
        }
        if (opened_storage_slots_grid_ != nullptr)
        {
            opened_storage_slots_grid_->set_visible(true);
        }
        reconcile_slot_grid(
            opened_storage_slots_grid_,
            opened_storage_slot_buttons_,
            GS1_INVENTORY_CONTAINER_DEVICE_STORAGE,
            opened_storage_->storage_id,
            opened_storage_->slot_count,
            opened_storage_->slots);
    }
    else
    {
        if (opened_storage_title_ != nullptr)
        {
            opened_storage_title_->set_visible(false);
        }
        if (opened_storage_slots_grid_ != nullptr)
        {
            opened_storage_slots_grid_->set_visible(false);
        }
        prune_slot_registry(opened_storage_slot_buttons_, {});
    }

    dirty_ = false;
}

void Gs1GodotInventoryPanelController::handle_slot_pressed(std::int64_t slot_key_value)
{
    const auto slot_key = static_cast<std::uint64_t>(slot_key_value);
    Button* button = nullptr;
    if (const auto found = worker_pack_slot_buttons_.find(slot_key); found != worker_pack_slot_buttons_.end())
    {
        button = resolve_object<Button>(found->second.object_id);
    }
    if (button == nullptr)
    {
        if (const auto found = opened_storage_slot_buttons_.find(slot_key); found != opened_storage_slot_buttons_.end())
        {
            button = resolve_object<Button>(found->second.object_id);
        }
    }
    if (button == nullptr || !submit_inventory_slot_tap_ || !bool(button->get_meta("occupied", false)))
    {
        return;
    }

    submit_inventory_slot_tap_(
        static_cast<int>(int64_t(button->get_meta("storage_id", 0))),
        static_cast<int>(int64_t(button->get_meta("container_kind", 0))),
        static_cast<int>(int64_t(button->get_meta("slot_index", 0))),
        static_cast<int>(int64_t(button->get_meta("item_instance_id", 0))));
}

String Gs1GodotInventoryPanelController::item_name_for(int item_id) const
{
    if (const auto* item_def = gs1::find_item_def(gs1::ItemId {static_cast<std::uint32_t>(item_id)}))
    {
        return string_from_view(item_def->display_name);
    }
    return String("Item #") + String::num_int64(item_id);
}

Ref<Texture2D> Gs1GodotInventoryPanelController::item_icon_for(std::uint32_t item_id) const
{
    return load_cached_texture(
        GodotProgressionResourceDatabase::instance().content_icon_path(k_content_resource_kind_item, item_id));
}

Ref<Texture2D> Gs1GodotInventoryPanelController::load_cached_texture(const String& path) const
{
    if (path.is_empty())
    {
        return Ref<Texture2D> {};
    }

    const std::string key = path.utf8().get_data();
    if (const auto found = texture_cache_.find(key); found != texture_cache_.end())
    {
        return found->second;
    }

    return texture_cache_.emplace(key, load_texture_2d(path)).first->second;
}

String Gs1GodotInventoryPanelController::slot_text_for(const Gs1RuntimeInventorySlotProjection& slot) const
{
    if ((slot.flags & k_inventory_slot_flag_occupied) == 0U || slot.item_id == 0U || slot.quantity == 0U)
    {
        return String();
    }
    return slot.quantity > 1U ? vformat("x%d", static_cast<int>(slot.quantity)) : String();
}

String Gs1GodotInventoryPanelController::container_title_for(const Gs1RuntimeInventoryStorageProjection* storage) const
{
    if (storage == nullptr)
    {
        return "Worker Pack";
    }
    if (storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
    {
        return vformat("Worker Pack  %d slots", static_cast<int>(storage->slot_count));
    }
    return vformat(
        "Storage %d  %d slots",
        static_cast<int>(storage->storage_id),
        static_cast<int>(storage->slot_count));
}

std::uint64_t Gs1GodotInventoryPanelController::slot_key(
    Gs1InventoryContainerKind container_kind,
    std::uint32_t storage_id,
    std::uint16_t slot_index) const noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint8_t>(container_kind)) << 56U) |
        (static_cast<std::uint64_t>(storage_id) << 16U) |
        static_cast<std::uint64_t>(slot_index);
}

const Gs1RuntimeInventoryStorageProjection* Gs1GodotInventoryPanelController::find_storage(std::uint32_t storage_id) const
{
    const auto found = std::find_if(
        inventory_storages_.begin(),
        inventory_storages_.end(),
        [&](const auto& storage) { return storage.storage_id == storage_id; });
    return found == inventory_storages_.end() ? nullptr : &*found;
}

const Gs1RuntimeInventoryStorageProjection* Gs1GodotInventoryPanelController::find_worker_pack_storage() const
{
    const auto found = std::find_if(
        inventory_storages_.begin(),
        inventory_storages_.end(),
        [](const auto& storage) { return storage.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK; });
    return found == inventory_storages_.end() ? nullptr : &*found;
}

const Gs1RuntimeInventorySlotProjection* Gs1GodotInventoryPanelController::find_slot(
    const std::vector<Gs1RuntimeInventorySlotProjection>& slots,
    std::uint16_t slot_index) const
{
    const auto found = std::find_if(
        slots.begin(),
        slots.end(),
        [&](const auto& slot) { return slot.slot_index == slot_index; });
    return found == slots.end() ? nullptr : &*found;
}

void Gs1GodotInventoryPanelController::reconcile_slot_grid(
    GridContainer* grid,
    std::unordered_map<std::uint64_t, SlotButtonRecord>& registry,
    Gs1InventoryContainerKind container_kind,
    std::uint32_t storage_id,
    std::uint16_t slot_count,
    const std::vector<Gs1RuntimeInventorySlotProjection>& slots)
{
    if (grid == nullptr || storage_id == 0U)
    {
        prune_slot_registry(registry, {});
        return;
    }

    const int columns = std::clamp<int>(slot_count, 1, 4);
    grid->set_columns(columns);

    std::unordered_set<std::uint64_t> desired_keys {};
    for (std::uint16_t slot_index = 0; slot_index < slot_count; ++slot_index)
    {
        const auto stable_key = slot_key(container_kind, storage_id, slot_index);
        desired_keys.insert(stable_key);
        Button* button = upsert_slot_button(
            grid,
            registry,
            stable_key,
            vformat("InventorySlot_%d_%d", static_cast<int>(storage_id), static_cast<int>(slot_index)),
            static_cast<int>(slot_index));
        if (button == nullptr)
        {
            continue;
        }

        const auto* slot = find_slot(slots, slot_index);
        const bool occupied = slot != nullptr &&
            (slot->flags & k_inventory_slot_flag_occupied) != 0U &&
            slot->item_id != 0U &&
            slot->quantity > 0U;
        button->set_text(occupied ? slot_text_for(*slot) : String());
        button->set_button_icon(occupied ? item_icon_for(slot->item_id) : Ref<Texture2D> {});
        button->set_tooltip_text(occupied ? item_name_for(static_cast<int>(slot->item_id)) : String());
        button->set_disabled(!occupied);
        button->set_meta("occupied", occupied);
        button->set_meta("storage_id", static_cast<int>(storage_id));
        button->set_meta("container_kind", static_cast<int>(container_kind));
        button->set_meta("slot_index", static_cast<int>(slot_index));
        button->set_meta("item_instance_id", occupied ? static_cast<int>(slot->item_instance_id) : 0);
    }

    prune_slot_registry(registry, desired_keys);
}

Button* Gs1GodotInventoryPanelController::upsert_slot_button(
    GridContainer* grid,
    std::unordered_map<std::uint64_t, SlotButtonRecord>& registry,
    std::uint64_t stable_key,
    const String& node_name,
    int desired_index)
{
    Button* button = nullptr;
    if (const auto found = registry.find(stable_key); found != registry.end())
    {
        button = resolve_object<Button>(found->second.object_id);
    }

    if (button == nullptr)
    {
        button = memnew(Button);
        button->set_clip_text(true);
        button->set_custom_minimum_size(Vector2(72.0F, 72.0F));
        button->set_focus_mode(Control::FOCUS_NONE);
        button->set_expand_icon(true);
        button->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        button->set_vertical_icon_alignment(VERTICAL_ALIGNMENT_CENTER);
        button->set_text_alignment(HORIZONTAL_ALIGNMENT_CENTER);
        grid->add_child(button);
        registry[stable_key].object_id = button->get_instance_id();
        const Callable callback = callable_mp_static(&dispatch_slot_pressed).bind(
            static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(this)),
            static_cast<std::int64_t>(stable_key));
        button->connect("pressed", callback);
    }

    button->set_name(node_name);
    const int child_count = grid->get_child_count();
    const int target_index = std::clamp(desired_index, 0, child_count - 1);
    if (button->get_index() != target_index)
    {
        grid->move_child(button, target_index);
    }
    return button;
}

void Gs1GodotInventoryPanelController::prune_slot_registry(
    std::unordered_map<std::uint64_t, SlotButtonRecord>& registry,
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
