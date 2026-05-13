#pragma once

#include "gs1_godot_adapter_service.h"

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

    struct RegionalSiteState final
    {
        std::uint32_t site_id {0};
        Gs1SiteState site_state {GS1_SITE_STATE_LOCKED};
        std::int32_t grid_x {0};
        std::int32_t grid_y {0};
        std::uint32_t support_package_id {0};
        std::uint32_t exported_support_item_count {0};
        std::uint32_t nearby_aura_modifier_count {0};
    };

    struct RegionalLinkState final
    {
        std::uint32_t from_site_id {0};
        std::uint32_t to_site_id {0};
    };

    void _ready() override;
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
    void refresh_from_game_state_view();
    void apply_summary_text();
    void apply_graph_text();
    [[nodiscard]] godot::String build_regional_map_overview_text(
        const std::vector<RegionalSiteState>& sites,
        const std::vector<RegionalLinkState>& links) const;

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::RichTextLabel* regional_map_summary_ {nullptr};
    godot::RichTextLabel* regional_map_graph_ {nullptr};
    std::optional<std::uint32_t> selected_site_id_ {};
    std::vector<RegionalSiteState> sites_ {};
    std::vector<RegionalLinkState> links_ {};
    std::int32_t total_reputation_ {0};
};
