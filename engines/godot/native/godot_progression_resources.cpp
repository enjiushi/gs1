#include "godot_progression_resources.h"

#include "toml.hpp"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <utility>

using namespace godot;

namespace
{
constexpr const char* k_default_progression_icon_path =
    "res://assets/field_command/progression_placeholder.png";
constexpr const char* k_default_site_marker_scene_path = "res://scenes/regional_site_marker.tscn";

std::size_t combine_hash(std::size_t lhs, std::size_t rhs) noexcept
{
    return lhs ^ (rhs + 0x9e3779b9U + (lhs << 6U) + (lhs >> 2U));
}

std::string require_string(
    const std::filesystem::path& path,
    const toml::table& table,
    const char* key)
{
    if (const auto value = table[key].value<std::string>())
    {
        return *value;
    }

    ERR_FAIL_V_MSG(
        std::string {},
        vformat("Missing string field '%s' in %s", key, String(path.string().c_str())));
}

std::uint32_t require_u32(
    const std::filesystem::path& path,
    const toml::table& table,
    const char* key)
{
    if (const auto value = table[key].value<std::int64_t>())
    {
        return static_cast<std::uint32_t>(*value);
    }

    ERR_FAIL_V_MSG(0U, vformat("Missing integer field '%s' in %s", key, String(path.string().c_str())));
}
}

std::size_t GodotContentResourceKeyHash::operator()(const GodotContentResourceKey& key) const noexcept
{
    const auto domain_hash = std::hash<std::uint8_t> {}(static_cast<std::uint8_t>(key.domain));
    const auto kind_hash = std::hash<std::uint8_t> {}(key.content_kind);
    const auto id_hash = std::hash<std::uint32_t> {}(key.content_id);
    return combine_hash(combine_hash(domain_hash, kind_hash), id_hash);
}

GodotProgressionResourceDatabase& GodotProgressionResourceDatabase::instance()
{
    static GodotProgressionResourceDatabase database {};
    return database;
}

const GodotTechnologyResourceDef* GodotProgressionResourceDatabase::find_technology_resource(
    std::uint32_t tech_node_id) const noexcept
{
    const auto found = technology_resources_.find(tech_node_id);
    return found != technology_resources_.end() ? &found->second : nullptr;
}

const GodotContentResourceDef* GodotProgressionResourceDatabase::find_unlockable_resource(
    std::uint8_t content_kind,
    std::uint32_t content_id) const noexcept
{
    return find_resource(GodotRenderResourceDomain::Unlockable, content_kind, content_id);
}

const GodotContentResourceDef* GodotProgressionResourceDatabase::find_content_resource(
    std::uint8_t content_kind,
    std::uint32_t content_id) const noexcept
{
    return find_resource(GodotRenderResourceDomain::Content, content_kind, content_id);
}

const GodotContentResourceDef* GodotProgressionResourceDatabase::find_modifier_resource(
    std::uint32_t modifier_id) const noexcept
{
    return find_resource(GodotRenderResourceDomain::Modifier, 0U, modifier_id);
}

const GodotContentResourceDef* GodotProgressionResourceDatabase::find_task_template_resource(
    std::uint32_t task_template_id) const noexcept
{
    return find_resource(GodotRenderResourceDomain::TaskTemplate, 0U, task_template_id);
}

const GodotContentResourceDef* GodotProgressionResourceDatabase::find_reward_candidate_resource(
    std::uint32_t reward_candidate_id) const noexcept
{
    return find_resource(GodotRenderResourceDomain::RewardCandidate, 0U, reward_candidate_id);
}

const GodotContentResourceDef* GodotProgressionResourceDatabase::find_site_resource(
    std::uint32_t site_id) const noexcept
{
    return find_resource(GodotRenderResourceDomain::Site, 0U, site_id);
}

String GodotProgressionResourceDatabase::technology_icon_path(std::uint32_t tech_node_id) const
{
    if (const auto* def = find_technology_resource(tech_node_id))
    {
        return String::utf8(def->icon_texture_path.c_str());
    }
    return String(k_default_progression_icon_path);
}

String GodotProgressionResourceDatabase::unlockable_icon_path(
    std::uint8_t content_kind,
    std::uint32_t content_id) const
{
    return resource_icon_path(GodotRenderResourceDomain::Unlockable, content_kind, content_id);
}

String GodotProgressionResourceDatabase::content_icon_path(
    std::uint8_t content_kind,
    std::uint32_t content_id) const
{
    return resource_icon_path(GodotRenderResourceDomain::Content, content_kind, content_id);
}

String GodotProgressionResourceDatabase::modifier_icon_path(std::uint32_t modifier_id) const
{
    return resource_icon_path(GodotRenderResourceDomain::Modifier, 0U, modifier_id);
}

String GodotProgressionResourceDatabase::task_template_icon_path(std::uint32_t task_template_id) const
{
    return resource_icon_path(GodotRenderResourceDomain::TaskTemplate, 0U, task_template_id);
}

String GodotProgressionResourceDatabase::reward_candidate_icon_path(std::uint32_t reward_candidate_id) const
{
    return resource_icon_path(GodotRenderResourceDomain::RewardCandidate, 0U, reward_candidate_id);
}

String GodotProgressionResourceDatabase::site_icon_path(std::uint32_t site_id) const
{
    return resource_icon_path(GodotRenderResourceDomain::Site, 0U, site_id);
}

String GodotProgressionResourceDatabase::site_thumbnail_path(std::uint32_t site_id) const
{
    return resource_thumbnail_path(GodotRenderResourceDomain::Site, 0U, site_id);
}

String GodotProgressionResourceDatabase::site_scene_path(std::uint32_t site_id) const
{
    const String path = resource_scene_path(GodotRenderResourceDomain::Site, 0U, site_id);
    return path.is_empty() ? String(k_default_site_marker_scene_path) : path;
}

String GodotProgressionResourceDatabase::content_scene_path(
    std::uint8_t content_kind,
    std::uint32_t content_id) const
{
    return resource_scene_path(GodotRenderResourceDomain::Content, content_kind, content_id);
}

GodotProgressionResourceDatabase::GodotProgressionResourceDatabase()
{
    load(std::filesystem::path {"project"} / "config" / "progression_resources.toml");
    load(std::filesystem::path {"project"} / "config" / "content_resources.toml");
    load(std::filesystem::path {"project"} / "config" / "render_resources.toml");
}

const GodotContentResourceDef* GodotProgressionResourceDatabase::find_resource(
    GodotRenderResourceDomain domain,
    std::uint8_t content_kind,
    std::uint32_t content_id) const noexcept
{
    const GodotContentResourceKey key {domain, content_kind, {0U, 0U, 0U}, content_id};
    const auto found = render_resources_.find(key);
    return found != render_resources_.end() ? &found->second : nullptr;
}

String GodotProgressionResourceDatabase::resource_icon_path(
    GodotRenderResourceDomain domain,
    std::uint8_t content_kind,
    std::uint32_t content_id) const
{
    if (const auto* def = find_resource(domain, content_kind, content_id))
    {
        if (!def->icon_texture_path.empty())
        {
            return String::utf8(def->icon_texture_path.c_str());
        }
    }
    return String(k_default_progression_icon_path);
}

String GodotProgressionResourceDatabase::resource_thumbnail_path(
    GodotRenderResourceDomain domain,
    std::uint8_t content_kind,
    std::uint32_t content_id) const
{
    if (const auto* def = find_resource(domain, content_kind, content_id))
    {
        if (!def->thumbnail_texture_path.empty())
        {
            return String::utf8(def->thumbnail_texture_path.c_str());
        }
        if (!def->icon_texture_path.empty())
        {
            return String::utf8(def->icon_texture_path.c_str());
        }
    }
    return String(k_default_progression_icon_path);
}

String GodotProgressionResourceDatabase::resource_scene_path(
    GodotRenderResourceDomain domain,
    std::uint8_t content_kind,
    std::uint32_t content_id) const
{
    if (const auto* def = find_resource(domain, content_kind, content_id))
    {
        if (!def->scene_path.empty())
        {
            return String::utf8(def->scene_path.c_str());
        }
    }
    return String();
}

void GodotProgressionResourceDatabase::load(const std::filesystem::path& path)
{
#if TOML_EXCEPTIONS
    const toml::table document = toml::parse_file(path.string());
#else
    auto parse_result = toml::parse_file(path.string());
    if (parse_result.failed())
    {
        const auto description = parse_result.error().description();
        ERR_FAIL_MSG(vformat(
            "Failed to load Godot progression resources from %s: %s",
            String(path.string().c_str()),
            String::utf8(description.data(), static_cast<int64_t>(description.size()))));
    }
    const toml::table document = std::move(parse_result).table();
#endif

    if (const auto* technology_array = document["technology_resources"].as_array())
    {
        for (const auto& node : *technology_array)
        {
            const auto* table = node.as_table();
            if (table == nullptr)
            {
                continue;
            }

            GodotTechnologyResourceDef def {};
            def.tech_node_id = require_u32(path, *table, "tech_node_id");
            def.icon_texture_path = require_string(path, *table, "icon_texture_path");
            technology_resources_.insert_or_assign(def.tech_node_id, std::move(def));
        }
    }

    if (const auto* unlockable_array = document["unlockable_resources"].as_array())
    {
        for (const auto& node : *unlockable_array)
        {
            const auto* table = node.as_table();
            if (table == nullptr)
            {
                continue;
            }

            GodotContentResourceDef def {};
            def.key.domain = GodotRenderResourceDomain::Unlockable;
            def.key.content_kind = parse_unlockable_content_kind(require_string(path, *table, "content_kind"));
            def.key.content_id = require_u32(path, *table, "content_id");
            def.icon_texture_path = require_string(path, *table, "icon_texture_path");
            if (const auto value = (*table)["thumbnail_texture_path"].value<std::string>())
            {
                def.thumbnail_texture_path = *value;
            }
            if (const auto value = (*table)["scene_path"].value<std::string>())
            {
                def.scene_path = *value;
            }
            render_resources_.insert_or_assign(def.key, std::move(def));
        }
    }

    if (const auto* content_array = document["content_resources"].as_array())
    {
        for (const auto& node : *content_array)
        {
            const auto* table = node.as_table();
            if (table == nullptr)
            {
                continue;
            }

            GodotContentResourceDef def {};
            def.key.domain = GodotRenderResourceDomain::Content;
            def.key.content_kind = parse_content_resource_kind(require_string(path, *table, "content_kind"));
            def.key.content_id = require_u32(path, *table, "content_id");
            def.icon_texture_path = require_string(path, *table, "icon_texture_path");
            if (const auto value = (*table)["thumbnail_texture_path"].value<std::string>())
            {
                def.thumbnail_texture_path = *value;
            }
            if (const auto value = (*table)["scene_path"].value<std::string>())
            {
                def.scene_path = *value;
            }
            render_resources_.insert_or_assign(def.key, std::move(def));
        }
    }

    if (const auto* render_array = document["render_resources"].as_array())
    {
        for (const auto& node : *render_array)
        {
            const auto* table = node.as_table();
            if (table == nullptr)
            {
                continue;
            }

            GodotContentResourceDef def {};
            def.key.domain = parse_render_resource_domain(require_string(path, *table, "domain"));
            def.key.content_kind = 0U;
            if (const auto value = (*table)["content_kind"].value<std::string>())
            {
                def.key.content_kind = parse_content_resource_kind(*value);
            }
            def.key.content_id = require_u32(path, *table, "content_id");
            def.icon_texture_path = require_string(path, *table, "icon_texture_path");
            if (const auto value = (*table)["thumbnail_texture_path"].value<std::string>())
            {
                def.thumbnail_texture_path = *value;
            }
            if (const auto value = (*table)["scene_path"].value<std::string>())
            {
                def.scene_path = *value;
            }
            render_resources_.insert_or_assign(def.key, std::move(def));
        }
    }
}

GodotRenderResourceDomain parse_render_resource_domain(std::string_view value)
{
    if (value == "Unlockable")
    {
        return GodotRenderResourceDomain::Unlockable;
    }
    if (value == "Content")
    {
        return GodotRenderResourceDomain::Content;
    }
    if (value == "Modifier")
    {
        return GodotRenderResourceDomain::Modifier;
    }
    if (value == "TaskTemplate")
    {
        return GodotRenderResourceDomain::TaskTemplate;
    }
    if (value == "RewardCandidate")
    {
        return GodotRenderResourceDomain::RewardCandidate;
    }
    if (value == "Site")
    {
        return GodotRenderResourceDomain::Site;
    }

    ERR_FAIL_V_MSG(
        GodotRenderResourceDomain::Unknown,
        vformat(
            "Unsupported Godot render resource domain '%s'",
            String::utf8(value.data(), static_cast<int64_t>(value.size()))));
}

std::uint8_t parse_unlockable_content_kind(std::string_view value)
{
    if (value == "Plant")
    {
        return 0U;
    }
    if (value == "Item")
    {
        return 1U;
    }
    if (value == "StructureRecipe")
    {
        return 2U;
    }
    if (value == "Recipe")
    {
        return 3U;
    }

    ERR_FAIL_V_MSG(
        0U,
        vformat(
            "Unsupported Godot unlockable content kind '%s'",
            String::utf8(value.data(), static_cast<int64_t>(value.size()))));
}

std::uint8_t parse_content_resource_kind(std::string_view value)
{
    if (value == "Plant")
    {
        return 0U;
    }
    if (value == "Item")
    {
        return 1U;
    }
    if (value == "Structure")
    {
        return 2U;
    }
    if (value == "Recipe")
    {
        return 3U;
    }

    ERR_FAIL_V_MSG(
        0U,
        vformat(
            "Unsupported Godot content resource kind '%s'",
            String::utf8(value.data(), static_cast<int64_t>(value.size()))));
}
