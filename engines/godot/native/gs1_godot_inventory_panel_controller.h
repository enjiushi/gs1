#pragma once

#include "gs1_godot_projection_types.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/classes/ref.hpp>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <vector>

class Gs1GodotInventoryPanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    using SubmitInventorySlotTapFn = std::function<void(int storage_id, int container_kind, int slot_index, int item_instance_id)>;

    void cache_ui_references(godot::Control& owner);
    void set_submit_inventory_slot_tap_callback(SubmitInventorySlotTapFn callback);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();
    void handle_slot_pressed(std::int64_t slot_key);

private:
    struct SlotButtonRecord final
    {
        godot::ObjectID object_id {};
    };

    [[nodiscard]] godot::String item_name_for(int item_id) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> item_icon_for(std::uint32_t item_id) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> load_cached_texture(const godot::String& path) const;
    [[nodiscard]] godot::String slot_text_for(const Gs1RuntimeInventorySlotProjection& slot) const;
    [[nodiscard]] godot::String container_title_for(const Gs1RuntimeInventoryStorageProjection* storage) const;
    [[nodiscard]] std::uint64_t slot_key(
        Gs1InventoryContainerKind container_kind,
        std::uint32_t storage_id,
        std::uint16_t slot_index) const noexcept;
    [[nodiscard]] const Gs1RuntimeInventoryStorageProjection* find_storage(std::uint32_t storage_id) const;
    [[nodiscard]] const Gs1RuntimeInventoryStorageProjection* find_worker_pack_storage() const;
    [[nodiscard]] const Gs1RuntimeInventorySlotProjection* find_slot(
        const std::vector<Gs1RuntimeInventorySlotProjection>& slots,
        std::uint16_t slot_index) const;
    void reconcile_slot_grid(
        godot::GridContainer* grid,
        std::unordered_map<std::uint64_t, SlotButtonRecord>& registry,
        Gs1InventoryContainerKind container_kind,
        std::uint32_t storage_id,
        std::uint16_t slot_count,
        const std::vector<Gs1RuntimeInventorySlotProjection>& slots);
    [[nodiscard]] godot::Button* upsert_slot_button(
        godot::GridContainer* grid,
        std::unordered_map<std::uint64_t, SlotButtonRecord>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_slot_registry(
        std::unordered_map<std::uint64_t, SlotButtonRecord>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);

    godot::Control* panel_ {nullptr};
    godot::Label* inventory_title_ {nullptr};
    godot::Label* worker_pack_title_ {nullptr};
    godot::GridContainer* worker_pack_slots_grid_ {nullptr};
    godot::Label* opened_storage_title_ {nullptr};
    godot::GridContainer* opened_storage_slots_grid_ {nullptr};
    std::optional<Gs1AppState> current_app_state_ {};
    std::vector<Gs1RuntimeInventoryStorageProjection> inventory_storages_ {};
    std::vector<Gs1RuntimeInventorySlotProjection> worker_pack_slots_ {};
    std::optional<Gs1RuntimeInventoryViewProjection> opened_storage_ {};
    std::unordered_map<std::uint64_t, SlotButtonRecord> worker_pack_slot_buttons_ {};
    std::unordered_map<std::uint64_t, SlotButtonRecord> opened_storage_slot_buttons_ {};
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> texture_cache_ {};
    SubmitInventorySlotTapFn submit_inventory_slot_tap_ {};
    bool worker_pack_open_ {false};
    bool in_site_snapshot_ {false};
    bool dirty_ {true};
};
