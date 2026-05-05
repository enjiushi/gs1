#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace godot
{
class Texture2D;
}

struct GodotTechnologyResourceDef final
{
    std::uint32_t tech_node_id {0};
    std::string icon_texture_path {};
};

enum class GodotRenderResourceDomain : std::uint8_t
{
    Unknown = 0,
    Unlockable = 1,
    Content = 2,
    Modifier = 3,
    TaskTemplate = 4,
    RewardCandidate = 5,
    Site = 6
};

struct GodotContentResourceKey final
{
    GodotRenderResourceDomain domain {GodotRenderResourceDomain::Unknown};
    std::uint8_t content_kind {0};
    std::uint8_t reserved0[3] {};
    std::uint32_t content_id {0};

    [[nodiscard]] bool operator==(const GodotContentResourceKey& other) const noexcept = default;
};

struct GodotContentResourceDef final
{
    GodotContentResourceKey key {};
    std::string icon_texture_path {};
    std::string thumbnail_texture_path {};
    std::string scene_path {};
};

struct GodotContentResourceKeyHash final
{
    [[nodiscard]] std::size_t operator()(const GodotContentResourceKey& key) const noexcept;
};

class GodotProgressionResourceDatabase final
{
public:
    static GodotProgressionResourceDatabase& instance();

    [[nodiscard]] const GodotTechnologyResourceDef* find_technology_resource(std::uint32_t tech_node_id) const noexcept;
    [[nodiscard]] const GodotContentResourceDef* find_unlockable_resource(
        std::uint8_t content_kind,
        std::uint32_t content_id) const noexcept;
    [[nodiscard]] const GodotContentResourceDef* find_content_resource(
        std::uint8_t content_kind,
        std::uint32_t content_id) const noexcept;
    [[nodiscard]] const GodotContentResourceDef* find_modifier_resource(std::uint32_t modifier_id) const noexcept;
    [[nodiscard]] const GodotContentResourceDef* find_task_template_resource(std::uint32_t task_template_id) const noexcept;
    [[nodiscard]] const GodotContentResourceDef* find_reward_candidate_resource(std::uint32_t reward_candidate_id) const noexcept;
    [[nodiscard]] const GodotContentResourceDef* find_site_resource(std::uint32_t site_id) const noexcept;

    [[nodiscard]] godot::String technology_icon_path(std::uint32_t tech_node_id) const;
    [[nodiscard]] godot::String unlockable_icon_path(std::uint8_t content_kind, std::uint32_t content_id) const;
    [[nodiscard]] godot::String content_icon_path(std::uint8_t content_kind, std::uint32_t content_id) const;
    [[nodiscard]] godot::String modifier_icon_path(std::uint32_t modifier_id) const;
    [[nodiscard]] godot::String task_template_icon_path(std::uint32_t task_template_id) const;
    [[nodiscard]] godot::String reward_candidate_icon_path(std::uint32_t reward_candidate_id) const;
    [[nodiscard]] godot::String site_icon_path(std::uint32_t site_id) const;
    [[nodiscard]] godot::String site_thumbnail_path(std::uint32_t site_id) const;
    [[nodiscard]] godot::String site_scene_path(std::uint32_t site_id) const;
    [[nodiscard]] godot::String content_scene_path(std::uint8_t content_kind, std::uint32_t content_id) const;

private:
    GodotProgressionResourceDatabase();

    void load(const std::filesystem::path& path);
    [[nodiscard]] const GodotContentResourceDef* find_resource(
        GodotRenderResourceDomain domain,
        std::uint8_t content_kind,
        std::uint32_t content_id) const noexcept;
    [[nodiscard]] godot::String resource_icon_path(
        GodotRenderResourceDomain domain,
        std::uint8_t content_kind,
        std::uint32_t content_id) const;
    [[nodiscard]] godot::String resource_thumbnail_path(
        GodotRenderResourceDomain domain,
        std::uint8_t content_kind,
        std::uint32_t content_id) const;
    [[nodiscard]] godot::String resource_scene_path(
        GodotRenderResourceDomain domain,
        std::uint8_t content_kind,
        std::uint32_t content_id) const;

    std::unordered_map<std::uint32_t, GodotTechnologyResourceDef> technology_resources_ {};
    std::unordered_map<GodotContentResourceKey, GodotContentResourceDef, GodotContentResourceKeyHash>
        render_resources_ {};
};

[[nodiscard]] std::uint8_t parse_unlockable_content_kind(std::string_view value);
[[nodiscard]] std::uint8_t parse_content_resource_kind(std::string_view value);
[[nodiscard]] GodotRenderResourceDomain parse_render_resource_domain(std::string_view value);
