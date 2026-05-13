#pragma once

#include "gs1_godot_adapter_service.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/string.hpp>

class Gs1GodotStatusPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotStatusPanelController, godot::Control)

public:
    Gs1GodotStatusPanelController() = default;
    ~Gs1GodotStatusPanelController() override = default;

    void _ready() override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void set_last_action_message(const godot::String& message);

protected:
    static void _bind_methods();

private:
    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void refresh_from_game_state_view();
    void rebuild_status_label();

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::RichTextLabel* status_label_ {nullptr};
    godot::String last_action_message_ {};
    Gs1AppState app_state_ {GS1_APP_STATE_BOOT};
    std::int32_t total_reputation_ {0};
    std::int32_t village_reputation_ {0};
    std::int32_t forestry_reputation_ {0};
    std::int32_t university_reputation_ {0};
    std::optional<float> site_cash_ {};
    std::optional<Gs1SiteActionKind> current_action_kind_ {};
};
