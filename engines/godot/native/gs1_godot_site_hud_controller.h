#pragma once

#include "gs1_godot_adapter_service.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/core/binder_common.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class Gs1GodotSiteHudController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotSiteHudController, godot::Control)

public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;
    using SubmitStorageViewFn = std::function<void(int storage_id, int event_kind)>;
    using SubmitContextRequestFn = std::function<void(int tile_x, int tile_y, int flags)>;

    Gs1GodotSiteHudController() = default;
    ~Gs1GodotSiteHudController() override = default;

    void _ready() override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void set_submit_storage_view_callback(SubmitStorageViewFn callback);
    void set_submit_context_request_callback(SubmitContextRequestFn callback);
    void set_selected_tile(int tile_x, int tile_y);

    void handle_phone_pressed();
    void handle_pack_pressed();
    void handle_tasks_pressed();
    void handle_craft_pressed();
    void handle_protection_pressed();
    void handle_tech_pressed();

    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    struct HudState final
    {
        float player_health {0.0f};
        float player_hydration {0.0f};
        float player_nourishment {0.0f};
        float player_energy {0.0f};
        float player_morale {0.0f};
        float current_money {0.0f};
        float site_completion_normalized {0.0f};
        std::uint32_t active_task_count {0U};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void refresh_from_game_state_view();
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1);
    void submit_storage_view(int storage_id, int event_kind);
    void submit_context_request(int tile_x, int tile_y, int flags);
    [[nodiscard]] int worker_pack_storage_id() const noexcept;
    void rebuild_hud();
    void refresh_meter(godot::ProgressBar* bar, godot::Label* label, const char* name, float value);
    void refresh_button_badges();

    godot::Control* hud_root_ {nullptr};
    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::ProgressBar* health_bar_ {nullptr};
    godot::ProgressBar* hydration_bar_ {nullptr};
    godot::ProgressBar* nourishment_bar_ {nullptr};
    godot::ProgressBar* energy_bar_ {nullptr};
    godot::ProgressBar* morale_bar_ {nullptr};
    godot::Label* health_label_ {nullptr};
    godot::Label* hydration_label_ {nullptr};
    godot::Label* nourishment_label_ {nullptr};
    godot::Label* energy_label_ {nullptr};
    godot::Label* morale_label_ {nullptr};
    godot::Label* cash_label_ {nullptr};
    godot::Label* completion_label_ {nullptr};
    godot::Label* phone_badge_label_ {nullptr};
    godot::Label* task_badge_label_ {nullptr};
    godot::Button* phone_button_ {nullptr};
    godot::Button* pack_button_ {nullptr};
    godot::Button* tasks_button_ {nullptr};
    godot::Button* craft_button_ {nullptr};
    godot::Button* protection_button_ {nullptr};
    godot::Button* tech_button_ {nullptr};

    SubmitUiActionFn submit_ui_action_ {};
    SubmitStorageViewFn submit_storage_view_ {};
    SubmitContextRequestFn submit_context_request_ {};
    std::optional<HudState> hud_ {};
    int worker_pack_storage_id_ {0};
    int selected_tile_x_ {0};
    int selected_tile_y_ {0};
};
