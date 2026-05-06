#pragma once

#include "gs1_godot_projection_types.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/object_id.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Gs1GodotPhonePanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_phone_listing_pressed(std::int64_t button_key);
    void handle_phone_section_pressed(std::int64_t section);
    void handle_close_phone_pressed();
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

private:
    struct ProjectedButtonRecord final
    {
        godot::ObjectID object_id {};
    };

    void reconcile_phone_listing_buttons();
    [[nodiscard]] godot::Button* upsert_button_node(
        godot::Node* container,
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        std::uint64_t stable_key,
        const godot::String& node_name,
        int desired_index);
    void prune_button_registry(
        std::unordered_map<std::uint64_t, ProjectedButtonRecord>& registry,
        const std::unordered_set<std::uint64_t>& desired_keys);
    [[nodiscard]] godot::String item_name_for(int item_id) const;

    godot::Control* owner_control_ {nullptr};
    godot::Control* panel_ {nullptr};
    godot::Label* phone_state_label_ {nullptr};
    godot::VBoxContainer* phone_listings_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<Gs1RuntimePhonePanelProjection> phone_panel_state_ {};
    std::vector<Gs1RuntimePhoneListingProjection> phone_listings_state_ {};
    std::unordered_map<std::uint32_t, std::size_t> pending_listing_indices_ {};
    std::unordered_map<std::uint64_t, ProjectedButtonRecord> phone_listing_buttons_ {};
    mutable std::unordered_map<int, godot::String> item_name_cache_ {};
    bool dirty_ {true};
};
