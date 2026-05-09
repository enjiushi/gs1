#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_panel_state_reducers.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/binder_common.hpp>

#include <optional>

class Gs1GodotRegionalSummaryPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalSummaryPanelController, godot::Control)

public:
    Gs1GodotRegionalSummaryPanelController() = default;
    ~Gs1GodotRegionalSummaryPanelController() override = default;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void rebuild_summary();
    [[nodiscard]] godot::String build_regional_map_overview_text(
        const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
        const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links) const;

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::RichTextLabel* regional_map_summary_ {nullptr};
    godot::RichTextLabel* regional_map_graph_ {nullptr};
    Gs1GodotRegionalMapStateReducer regional_map_state_reducer_ {};
    std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources_ {};
};
