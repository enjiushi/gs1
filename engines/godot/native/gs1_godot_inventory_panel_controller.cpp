#include "gs1_godot_inventory_panel_controller.h"

#include "content/defs/item_defs.h"

#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace
{
String string_from_view(std::string_view value)
{
    return String::utf8(value.data(), static_cast<int>(value.size()));
}
}

void Gs1GodotInventoryPanelController::cache_ui_references(Control& owner)
{
    if (inventory_summary_ == nullptr)
    {
        inventory_summary_ = Object::cast_to<RichTextLabel>(owner.find_child("InventorySummary", true, false));
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
        const auto storage = std::find_if(
            inventory_storages_.begin(),
            inventory_storages_.end(),
            [&](const auto& projection) { return projection.storage_id == payload.storage_id; });
        const bool is_worker_pack_storage =
            storage != inventory_storages_.end() &&
            storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;

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
}

void Gs1GodotInventoryPanelController::handle_runtime_message_reset()
{
    inventory_storages_.clear();
    worker_pack_slots_.clear();
    opened_storage_.reset();
    worker_pack_open_ = false;
    in_site_snapshot_ = false;
    dirty_ = true;
}

void Gs1GodotInventoryPanelController::refresh_if_needed()
{
    if (!dirty_ || inventory_summary_ == nullptr)
    {
        return;
    }

    PackedStringArray lines;
    lines.push_back("[b]Inventory[/b]");
    lines.push_back(vformat("Worker Pack Open: %s", worker_pack_open_ ? "yes" : "no"));

    for (const auto& slot : worker_pack_slots_)
    {
        lines.push_back(vformat(
            "Pack %d: %s x%d",
            static_cast<int>(slot.slot_index),
            item_name_for(static_cast<int>(slot.item_id)),
            static_cast<int>(slot.quantity)));
    }

    if (opened_storage_.has_value())
    {
        lines.push_back(String());
        lines.push_back(vformat("Opened Storage %d", static_cast<int>(opened_storage_->storage_id)));
        for (const auto& slot : opened_storage_->slots)
        {
            lines.push_back(vformat(
                "Slot %d: %s x%d",
                static_cast<int>(slot.slot_index),
                item_name_for(static_cast<int>(slot.item_id)),
                static_cast<int>(slot.quantity)));
        }
    }

    inventory_summary_->set_text(String("\n").join(lines));
    dirty_ = false;
}

String Gs1GodotInventoryPanelController::item_name_for(int item_id) const
{
    if (const auto* item_def = gs1::find_item_def(gs1::ItemId {static_cast<std::uint32_t>(item_id)}))
    {
        return string_from_view(item_def->display_name);
    }
    return String("Item #") + String::num_int64(item_id);
}
