#pragma once

#include "gs1_godot_projection_types.h"
#include "gs1_godot_runtime_node.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>

#include <optional>

class Gs1GodotCraftPanelController final : public IGs1GodotEngineMessageSubscriber
{
public:
    void cache_ui_references(godot::Control& owner);
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;
    void refresh_if_needed();

private:
    [[nodiscard]] godot::String item_name_for(int item_id) const;

    godot::RichTextLabel* craft_summary_ {nullptr};
    std::optional<Gs1RuntimePlacementPreviewProjection> placement_preview_ {};
    std::optional<Gs1RuntimePlacementFailureProjection> placement_failure_ {};
    std::optional<Gs1RuntimeCraftContextProjection> craft_context_ {};
    bool dirty_ {true};
};
