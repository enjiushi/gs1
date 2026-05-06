#pragma once

#include "gs1_godot_panel_state_reducers.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>

#include <optional>

class Gs1GodotRegionalSummaryPanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

private:
    [[nodiscard]] godot::String build_regional_map_overview_text(
        const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
        const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links) const;

    godot::RichTextLabel* regional_map_summary_ {nullptr};
    godot::RichTextLabel* regional_map_graph_ {nullptr};
    Gs1GodotRegionalMapStateReducer regional_map_state_reducer_ {};
    std::optional<Gs1AppState> current_app_state_ {};
    std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources_ {};
    bool dirty_ {true};
};
