#pragma once

#include "gs1_godot_adapter_service.h"
#include "gs1_godot_projection_types.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/core/binder_common.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class Gs1GodotRegionalMapHudController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalMapHudController, godot::Control)

public:
    using SubmitUiActionFn = std::function<void(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1)>;

    Gs1GodotRegionalMapHudController() = default;
    ~Gs1GodotRegionalMapHudController() override = default;

    void _ready() override;
    void _exit_tree() override;

    void cache_ui_references(godot::Control& owner);
    void set_submit_ui_action_callback(SubmitUiActionFn callback);
    void handle_tech_button_pressed();
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
    void submit_ui_action(std::int64_t action_type, std::int64_t target_id, std::int64_t arg0, std::int64_t arg1);
    void reset_regional_map_state() noexcept;
    void apply_regional_map_message(const Gs1EngineMessage& message);
    void apply_selected_site_summary();
    void apply_campaign_summary();
    void apply_tech_button();
    [[nodiscard]] godot::String selected_site_text() const;
    [[nodiscard]] godot::String campaign_summary_text() const;
    [[nodiscard]] godot::String site_state_name(int site_state) const;
    [[nodiscard]] godot::String support_preview_text(int preview_mask) const;
    [[nodiscard]] godot::String tech_button_label() const;
    [[nodiscard]] Gs1UiAction tech_button_action() const;
    [[nodiscard]] const Gs1RuntimeRegionalMapSiteProjection* selected_site() const;

    godot::Control* owner_control_ {nullptr};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Control* hud_ {nullptr};
    godot::RichTextLabel* selected_site_summary_ {nullptr};
    godot::RichTextLabel* campaign_summary_ {nullptr};
    godot::Button* tech_button_ {nullptr};
    SubmitUiActionFn submit_ui_action_ {};
    std::optional<std::uint32_t> selected_site_id_ {};
    std::vector<Gs1RuntimeRegionalMapSiteProjection> sites_ {};
    std::vector<Gs1RuntimeRegionalMapLinkProjection> links_ {};
    std::optional<PendingRegionalMapState> pending_regional_map_state_ {};
    std::optional<Gs1RuntimeCampaignResourcesProjection> campaign_resources_ {};
    bool tech_button_connected_ {false};
};
