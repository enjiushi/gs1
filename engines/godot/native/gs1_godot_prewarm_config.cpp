#include "gs1_godot_prewarm_config.h"

#include "toml.hpp"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <array>
#include <filesystem>
#include <unordered_map>
#include <utility>

using namespace godot;

namespace
{
struct ParsedGroupDef final
{
    std::vector<Gs1GodotPrewarmTargetDef> targets {};
};

constexpr const char* k_prewarm_config_res_path = "res://gs1/runtime/adapter_metadata/godot_prewarm.toml";

std::filesystem::path globalize_res_path(std::string_view res_path)
{
    ProjectSettings* project_settings = ProjectSettings::get_singleton();
    if (project_settings == nullptr)
    {
        return std::filesystem::path {res_path};
    }

    const String absolute_path = project_settings->globalize_path(
        String::utf8(res_path.data(), static_cast<int64_t>(res_path.size())));
    return std::filesystem::path {absolute_path.utf8().get_data()};
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

Gs1GodotDirectorScreenKind parse_screen_kind(std::string_view value)
{
    if (value == "main_menu")
    {
        return GS1_GODOT_SCREEN_KIND_MAIN_MENU;
    }
    if (value == "regional_map")
    {
        return GS1_GODOT_SCREEN_KIND_REGIONAL_MAP;
    }
    if (value == "site_session")
    {
        return GS1_GODOT_SCREEN_KIND_SITE_SESSION;
    }

    ERR_FAIL_V_MSG(
        GS1_GODOT_SCREEN_KIND_NONE,
        vformat(
            "Unsupported Godot prewarm screen id '%s'",
            String::utf8(value.data(), static_cast<int64_t>(value.size()))));
}

Gs1GodotPrewarmTargetKind parse_target_kind(std::string_view value)
{
    if (value == "scene")
    {
        return Gs1GodotPrewarmTargetKind::Scene;
    }
    if (value == "resource")
    {
        return Gs1GodotPrewarmTargetKind::Resource;
    }

    ERR_FAIL_V_MSG(
        Gs1GodotPrewarmTargetKind::Resource,
        vformat(
            "Unsupported Godot prewarm target kind '%s'",
            String::utf8(value.data(), static_cast<int64_t>(value.size()))));
}

Gs1GodotPrewarmRetainPolicy parse_retain_policy(std::string_view value)
{
    if (value == "until_transition_consumed")
    {
        return Gs1GodotPrewarmRetainPolicy::UntilTransitionConsumed;
    }
    if (value == "until_screen_exit")
    {
        return Gs1GodotPrewarmRetainPolicy::UntilScreenExit;
    }
    if (value == "manual")
    {
        return Gs1GodotPrewarmRetainPolicy::Manual;
    }

    ERR_FAIL_V_MSG(
        Gs1GodotPrewarmRetainPolicy::UntilScreenExit,
        vformat(
            "Unsupported Godot prewarm retain policy '%s'",
            String::utf8(value.data(), static_cast<int64_t>(value.size()))));
}

std::size_t screen_kind_index(Gs1GodotDirectorScreenKind screen_kind) noexcept
{
    return static_cast<std::size_t>(screen_kind);
}

Gs1GodotPrewarmTargetDef parse_target_def(
    const std::filesystem::path& path,
    const toml::table& table)
{
    Gs1GodotPrewarmTargetDef target {};
    target.kind = parse_target_kind(require_string(path, table, "kind"));
    target.path = require_string(path, table, "path");
    if (const auto retain_value = table["retain"].value<std::string>())
    {
        target.retain_policy = parse_retain_policy(*retain_value);
    }
    else
    {
        target.retain_policy = Gs1GodotPrewarmRetainPolicy::Inherit;
    }

    if (const auto resource_type = table["resource_type"].value<std::string>())
    {
        target.resource_type = *resource_type;
    }
    else if (target.kind == Gs1GodotPrewarmTargetKind::Scene)
    {
        target.resource_type = "PackedScene";
    }

    return target;
}

Gs1GodotPrewarmTargetDef resolve_target_def(
    Gs1GodotPrewarmTargetDef target,
    Gs1GodotPrewarmRetainPolicy transition_default_retain)
{
    if (target.retain_policy == Gs1GodotPrewarmRetainPolicy::Inherit)
    {
        target.retain_policy = transition_default_retain;
    }
    return target;
}
}

const Gs1GodotPrewarmConfig& Gs1GodotPrewarmConfig::instance()
{
    static Gs1GodotPrewarmConfig config {};
    return config;
}

const std::vector<Gs1GodotPrewarmTransitionDef>& Gs1GodotPrewarmConfig::transitions_from(
    Gs1GodotDirectorScreenKind screen_kind) const noexcept
{
    const std::size_t index = screen_kind_index(screen_kind);
    static const std::vector<Gs1GodotPrewarmTransitionDef> k_empty {};
    return index < std::size(transitions_by_from_) ? transitions_by_from_[index] : k_empty;
}

Gs1GodotPrewarmConfig::Gs1GodotPrewarmConfig()
{
    const std::filesystem::path config_path = globalize_res_path(k_prewarm_config_res_path);
    if (!std::filesystem::exists(config_path))
    {
        UtilityFunctions::push_warning(vformat(
            "GS1 Godot prewarm config missing at %s",
            String(config_path.string().c_str())));
        return;
    }

#if TOML_EXCEPTIONS
    const toml::table document = toml::parse_file(config_path.string());
#else
    auto parse_result = toml::parse_file(config_path.string());
    if (parse_result.failed())
    {
        const auto description = parse_result.error().description();
        ERR_FAIL_MSG(vformat(
            "Failed to load Godot prewarm config from %s: %s",
            String(config_path.string().c_str()),
            String::utf8(description.data(), static_cast<int64_t>(description.size()))));
    }
    const toml::table document = std::move(parse_result).table();
#endif

    std::unordered_map<std::string, ParsedGroupDef> groups {};
    if (const auto* group_array = document["group"].as_array())
    {
        for (const auto& node : *group_array)
        {
            const auto* table = node.as_table();
            if (table == nullptr)
            {
                continue;
            }

            ParsedGroupDef group {};
            const std::string group_id = require_string(config_path, *table, "id");
            if (const auto* target_array = (*table)["targets"].as_array())
            {
                for (const auto& target_node : *target_array)
                {
                    const auto* target_table = target_node.as_table();
                    if (target_table == nullptr)
                    {
                        continue;
                    }
                    group.targets.push_back(parse_target_def(config_path, *target_table));
                }
            }

            groups.insert_or_assign(group_id, std::move(group));
        }
    }

    if (const auto* transition_array = document["transition"].as_array())
    {
        for (const auto& node : *transition_array)
        {
            const auto* table = node.as_table();
            if (table == nullptr)
            {
                continue;
            }

            Gs1GodotPrewarmTransitionDef transition {};
            transition.from = parse_screen_kind(require_string(config_path, *table, "from"));
            transition.to = parse_screen_kind(require_string(config_path, *table, "to"));
            Gs1GodotPrewarmRetainPolicy default_retain = Gs1GodotPrewarmRetainPolicy::UntilScreenExit;
            if (const auto retain_value = (*table)["retain"].value<std::string>())
            {
                default_retain = parse_retain_policy(*retain_value);
            }

            if (const auto* groups_array = (*table)["groups"].as_array())
            {
                for (const auto& group_node : *groups_array)
                {
                    const auto group_id = group_node.value<std::string>();
                    if (!group_id.has_value())
                    {
                        continue;
                    }

                    const auto found = groups.find(*group_id);
                    if (found == groups.end())
                    {
                        ERR_FAIL_MSG(vformat(
                            "Unknown Godot prewarm group '%s' referenced by %s",
                            String(group_id->c_str()),
                            String(config_path.string().c_str())));
                    }

                    for (const Gs1GodotPrewarmTargetDef& group_target : found->second.targets)
                    {
                        transition.targets.push_back(resolve_target_def(group_target, default_retain));
                    }
                }
            }

            if (const auto* target_array = (*table)["targets"].as_array())
            {
                for (const auto& target_node : *target_array)
                {
                    const auto* target_table = target_node.as_table();
                    if (target_table == nullptr)
                    {
                        continue;
                    }

                    transition.targets.push_back(resolve_target_def(
                        parse_target_def(config_path, *target_table),
                        default_retain));
                }
            }

            const std::size_t index = screen_kind_index(transition.from);
            if (index < std::size(transitions_by_from_))
            {
                transitions_by_from_[index].push_back(std::move(transition));
            }
        }
    }
}
