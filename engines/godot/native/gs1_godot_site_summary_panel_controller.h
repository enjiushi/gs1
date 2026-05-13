#pragma once

#include "gs1_godot_adapter_service.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/binder_common.hpp>

#include <optional>

class Gs1GodotSiteSummaryPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotSiteSummaryPanelController, godot::Control)

public:
    Gs1GodotSiteSummaryPanelController() = default;
    ~Gs1GodotSiteSummaryPanelController() override = default;

    void _ready() override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    struct State final
    {
        std::uint32_t site_id {0};
        std::uint32_t width {0};
        std::uint32_t height {0};
        std::int32_t worker_tile_x {0};
        std::int32_t worker_tile_y {0};
        float weather_heat {0.0f};
        float weather_wind {0.0f};
        float weather_dust {0.0f};
        float health {0.0f};
        float hydration {0.0f};
        float nourishment {0.0f};
        float energy {0.0f};
        float morale {0.0f};
        Gs1SiteActionKind action_kind {GS1_SITE_ACTION_NONE};
        float action_progress {0.0f};
        bool has_worker {false};
        bool has_action {false};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void refresh_from_game_state_view();
    void rebuild_summary();

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Label* site_title_ {nullptr};
    godot::RichTextLabel* site_summary_ {nullptr};
    std::optional<State> state_ {};
};
