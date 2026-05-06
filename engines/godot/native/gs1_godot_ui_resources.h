#pragma once

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

class Gs1GodotUiResources final
{
public:
    struct TaskTemplateUiCacheEntry final
    {
        int task_tier_id {0};
        int progress_kind {0};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    struct ModifierUiCacheEntry final
    {
        godot::String label {};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    struct TechnologyUiCacheEntry final
    {
        godot::String title {};
        godot::String faction_name {};
        godot::String tooltip {};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    struct UnlockableUiCacheEntry final
    {
        godot::String title {};
        godot::String subtitle {};
        godot::String tooltip {};
        int content_kind {-1};
        godot::Ref<godot::Texture2D> icon_texture {};
    };

    [[nodiscard]] godot::String item_name_for(int item_id) const;
    [[nodiscard]] godot::String faction_name_for(int faction_id) const;
    [[nodiscard]] godot::String plant_name_for(int plant_id) const;
    [[nodiscard]] godot::String structure_name_for(int structure_id) const;
    [[nodiscard]] godot::String recipe_output_name_for(int recipe_id, int output_item_id) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> load_cached_texture(const godot::String& path) const;
    [[nodiscard]] godot::Ref<godot::Texture2D> fallback_icon_texture(const godot::String& icon_text) const;
    [[nodiscard]] const TaskTemplateUiCacheEntry& task_template_ui_for(std::uint32_t task_template_id) const;
    [[nodiscard]] const ModifierUiCacheEntry& modifier_ui_for(std::uint32_t modifier_id) const;
    [[nodiscard]] const TechnologyUiCacheEntry& technology_ui_for(std::uint32_t tech_node_id) const;
    [[nodiscard]] const UnlockableUiCacheEntry& unlockable_ui_for(std::uint32_t unlock_id) const;

private:
    [[nodiscard]] godot::Color icon_background_color(const godot::String& icon_text) const;

    mutable std::unordered_map<int, godot::String> item_name_cache_ {};
    mutable std::unordered_map<int, godot::String> plant_name_cache_ {};
    mutable std::unordered_map<int, godot::String> structure_name_cache_ {};
    mutable std::unordered_map<int, godot::String> faction_name_cache_ {};
    mutable std::unordered_map<std::uint64_t, godot::String> recipe_output_name_cache_ {};
    mutable std::unordered_map<std::uint32_t, TaskTemplateUiCacheEntry> task_template_ui_cache_ {};
    mutable std::unordered_map<std::uint32_t, ModifierUiCacheEntry> modifier_ui_cache_ {};
    mutable std::unordered_map<std::uint32_t, TechnologyUiCacheEntry> technology_ui_cache_ {};
    mutable std::unordered_map<std::uint32_t, UnlockableUiCacheEntry> unlockable_ui_cache_ {};
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> texture_cache_ {};
    mutable std::unordered_map<std::string, godot::Ref<godot::Texture2D>> fallback_icon_texture_cache_ {};
};
