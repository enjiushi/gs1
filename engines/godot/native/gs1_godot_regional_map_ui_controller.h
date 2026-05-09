#pragma once

#include "gs1_godot_adapter_service.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/core/binder_common.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include <cstdint>

class Gs1GodotRegionalMapUiController final
    : public godot::Control
    , public IGs1GodotEngineMessageSubscriber
{
    GDCLASS(Gs1GodotRegionalMapUiController, godot::Control)

public:
    Gs1GodotRegionalMapUiController() = default;
    ~Gs1GodotRegionalMapUiController() override = default;

    void _ready() override;
    void _exit_tree() override;
    void set_ui_root_path(const godot::NodePath& path);
    [[nodiscard]] godot::NodePath get_ui_root_path() const;
    [[nodiscard]] bool handles_engine_message(Gs1EngineMessageType type) const noexcept override;
    void handle_engine_message(const Gs1EngineMessage& message) override;
    void handle_runtime_message_reset() override;

protected:
    static void _bind_methods();

private:
    void cache_adapter_service();
    [[nodiscard]] godot::Control* resolve_ui_root();
    void apply_surface_visibility();

private:
    godot::NodePath ui_root_path_ {".."};
    Gs1GodotAdapterService* adapter_service_ {nullptr};
    godot::Control* ui_root_ {nullptr};
    bool selection_visible_ {false};
    bool tech_tree_visible_ {false};
};
