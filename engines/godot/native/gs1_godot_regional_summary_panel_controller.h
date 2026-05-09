#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_projection_types.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/binder_common.hpp>

#include <optional>
#include <vector>

class Gs1GodotRegionalSummaryPanelController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalSummaryPanelController, godot::Control)

public:
    Gs1GodotRegionalSummaryPanelController() = default;
    ~Gs1GodotRegionalSummaryPanelController() override = default;

    void _ready() override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    struct PendingRegionalMapState final
    {
        std::vector<Gs1RuntimeRegionalMapSiteProjection> sites {};
        std::vector<Gs1RuntimeRegionalMapLinkProjection> links {};
    };

    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_owner_control();
    void reset_regional_map_state() noexcept;
    void apply_regional_map_message(const Gs1EngineMessage& message);
    void apply_summary_text();
    void apply_graph_text();
    [[nodiscard]] godot::String build_regional_map_overview_text(
        const std::vector<Gs1RuntimeRegionalMapSiteProjection>& sites,
        const std::vector<Gs1RuntimeRegionalMapLinkProjection>& links) const;

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::RichTextLabel* regional_map_summary_ {nullptr};
    godot::RichTextLabel* regional_map_graph_ {nullptr};
    std::optional<std::uint32_t> selected_site_id_ {};
    std::vector<Gs1RuntimeRegionalMapSiteProjection> sites_ {};
    std::vector<Gs1RuntimeRegionalMapLinkProjection> links_ {};
    std::optional<PendingRegionalMapState> pending_regional_map_state_ {};
    std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources_ {};
};
