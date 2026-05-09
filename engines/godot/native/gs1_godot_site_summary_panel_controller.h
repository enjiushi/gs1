#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_projection_types.h"

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
    void _process(double delta) override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

protected:
    static void _bind_methods();

private:
    struct State final
    {
        std::uint32_t site_id {0};
        std::uint16_t width {0};
        std::uint16_t height {0};
        std::optional<Gs1RuntimeWorkerProjection> worker {};
        std::optional<Gs1RuntimeWeatherProjection> weather {};
        std::optional<Gs1RuntimeHudProjection> hud {};
        std::optional<Gs1RuntimeSiteActionProjection> site_action {};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Label* site_title_ {nullptr};
    godot::RichTextLabel* site_summary_ {nullptr};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<State> state_ {};
    bool dirty_ {true};
};
