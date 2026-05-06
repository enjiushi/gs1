#pragma once

#include "gs1_godot_projection_types.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>

#include <optional>

class Gs1GodotSiteSummaryPanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

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

    godot::Label* site_title_ {nullptr};
    godot::RichTextLabel* site_summary_ {nullptr};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<State> state_ {};
    bool dirty_ {true};
};
