#pragma once

#include "gs1_godot_projection_types.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>

#include <optional>
#include <vector>

class Gs1GodotInventoryPanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

private:
    [[nodiscard]] godot::String item_name_for(int item_id) const;

    godot::RichTextLabel* inventory_summary_ {nullptr};
    std::vector<Gs1RuntimeInventoryStorageProjection> inventory_storages_ {};
    std::vector<Gs1RuntimeInventorySlotProjection> worker_pack_slots_ {};
    std::optional<Gs1RuntimeInventoryViewProjection> opened_storage_ {};
    bool worker_pack_open_ {false};
    bool in_site_snapshot_ {false};
    bool dirty_ {true};
};
