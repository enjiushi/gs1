#include "host/adapter_metadata_catalog.h"

#include "toml.hpp"

#include <cstdlib>
#include <cstdio>

namespace
{
[[noreturn]] void fail_metadata_load(
    const std::filesystem::path& path,
    std::size_t line_number,
    std::string_view message)
{
    if (line_number == 0U)
    {
        std::fprintf(
            stderr,
            "Failed to load adapter metadata file %s: %.*s\n",
            path.string().c_str(),
            static_cast<int>(message.size()),
            message.data());
    }
    else
    {
        std::fprintf(
            stderr,
            "Failed to load adapter metadata file %s:%zu: %.*s\n",
            path.string().c_str(),
            line_number,
            static_cast<int>(message.size()),
            message.data());
    }

    std::abort();
}

[[nodiscard]] std::size_t toml_line_number(const toml::node& node) noexcept
{
    return node.source().begin ? static_cast<std::size_t>(node.source().begin.line) : 0U;
}

[[nodiscard]] toml::table load_toml_document(const std::filesystem::path& path)
{
#if TOML_EXCEPTIONS
    try
    {
        return toml::parse_file(path.string());
    }
    catch (const toml::parse_error& error)
    {
        fail_metadata_load(path, static_cast<std::size_t>(error.source().begin.line), error.description());
    }
#else
    auto parse_result = toml::parse_file(path.string());
    if (parse_result.failed())
    {
        fail_metadata_load(
            path,
            parse_result.error().source().begin ? static_cast<std::size_t>(parse_result.error().source().begin.line) : 0U,
            parse_result.error().description());
    }
    return std::move(parse_result).table();
#endif
}

[[nodiscard]] const toml::array& require_toml_array(
    const std::filesystem::path& path,
    const toml::table& table,
    const char* key)
{
    if (const auto* value = table[key].as_array())
    {
        return *value;
    }

    fail_metadata_load(path, toml_line_number(table), "missing required array");
}

[[nodiscard]] const toml::table& require_array_entry_table(
    const std::filesystem::path& path,
    const toml::node& node,
    const char* array_name)
{
    if (const auto* value = node.as_table())
    {
        return *value;
    }

    std::string message = "array entry in ";
    message += array_name;
    message += " must be a table";
    fail_metadata_load(path, toml_line_number(node), message);
}

template <typename IntegerType>
[[nodiscard]] IntegerType require_toml_integer(
    const std::filesystem::path& path,
    const toml::table& table,
    const char* key)
{
    if (const auto value = table[key].value<std::int64_t>())
    {
        return static_cast<IntegerType>(*value);
    }

    std::string message = "missing integer field '";
    message += key;
    message += "'";
    fail_metadata_load(path, toml_line_number(table), message);
}

[[nodiscard]] std::string require_toml_string(
    const std::filesystem::path& path,
    const toml::table& table,
    const char* key)
{
    if (const auto value = table[key].value<std::string>())
    {
        return *value;
    }

    std::string message = "missing string field '";
    message += key;
    message += "'";
    fail_metadata_load(path, toml_line_number(table), message);
}

[[nodiscard]] std::optional<std::string> optional_toml_string(
    const toml::table& table,
    const char* key)
{
    if (const auto value = table[key].value<std::string>())
    {
        return *value;
    }

    return std::nullopt;
}

[[nodiscard]] Gs1AdapterMetadataCatalog& mutable_catalog() noexcept
{
    static Gs1AdapterMetadataCatalog catalog {};
    return catalog;
}
}

void Gs1AdapterMetadataCatalog::load_from_project_root(const std::filesystem::path& project_root)
{
    modifier_entries_.clear();
    reputation_unlock_entries_.clear();
    technology_node_entries_.clear();

    const std::filesystem::path metadata_root = project_root / "adapter_metadata";
    load_modifier_metadata(metadata_root / "modifier_presets.toml");
    load_reputation_unlock_metadata(metadata_root / "reputation_unlocks.toml");
    load_technology_node_metadata(metadata_root / "technology_nodes.toml");
}

const Gs1AdapterMetadataEntry* Gs1AdapterMetadataCatalog::find(
    const Gs1AdapterMetadataDomain domain,
    const std::uint32_t id) const noexcept
{
    const auto* map = [&]() -> const std::unordered_map<std::uint32_t, Gs1AdapterMetadataEntry>*
    {
        switch (domain)
        {
        case Gs1AdapterMetadataDomain::Modifier:
            return &modifier_entries_;
        case Gs1AdapterMetadataDomain::ReputationUnlock:
            return &reputation_unlock_entries_;
        case Gs1AdapterMetadataDomain::TechnologyNode:
            return &technology_node_entries_;
        default:
            return nullptr;
        }
    }();

    if (map == nullptr)
    {
        return nullptr;
    }

    const auto found = map->find(id);
    return found == map->end() ? nullptr : &found->second;
}

void Gs1AdapterMetadataCatalog::load_modifier_metadata(const std::filesystem::path& path)
{
    const auto& document = load_toml_document(path);
    const auto& entries = require_toml_array(path, document, "modifier_presets");
    for (const auto& node : entries)
    {
        const auto& entry = require_array_entry_table(path, node, "modifier_presets");
        const std::uint32_t modifier_id = require_toml_integer<std::uint32_t>(path, entry, "modifier_id");
        modifier_entries_.insert_or_assign(
            modifier_id,
            Gs1AdapterMetadataEntry {
                optional_toml_string(entry, "display_name").value_or(std::string {}),
                optional_toml_string(entry, "description").value_or(std::string {})});
    }
}

void Gs1AdapterMetadataCatalog::load_reputation_unlock_metadata(const std::filesystem::path& path)
{
    const auto& document = load_toml_document(path);
    const auto& entries = require_toml_array(path, document, "reputation_unlocks");
    for (const auto& node : entries)
    {
        const auto& entry = require_array_entry_table(path, node, "reputation_unlocks");
        const std::uint32_t unlock_id = require_toml_integer<std::uint32_t>(path, entry, "unlock_id");
        reputation_unlock_entries_.insert_or_assign(
            unlock_id,
            Gs1AdapterMetadataEntry {
                require_toml_string(path, entry, "display_name"),
                require_toml_string(path, entry, "description")});
    }
}

void Gs1AdapterMetadataCatalog::load_technology_node_metadata(const std::filesystem::path& path)
{
    const auto& document = load_toml_document(path);
    const auto& entries = require_toml_array(path, document, "technology_nodes");
    for (const auto& node : entries)
    {
        const auto& entry = require_array_entry_table(path, node, "technology_nodes");
        const std::uint32_t tech_node_id = require_toml_integer<std::uint32_t>(path, entry, "tech_node_id");
        technology_node_entries_.insert_or_assign(
            tech_node_id,
            Gs1AdapterMetadataEntry {
                require_toml_string(path, entry, "display_name"),
                require_toml_string(path, entry, "description")});
    }
}

const Gs1AdapterMetadataCatalog& adapter_metadata_catalog() noexcept
{
    return mutable_catalog();
}

void load_adapter_metadata_catalog_from_project_root(const std::filesystem::path& project_root)
{
    mutable_catalog().load_from_project_root(project_root);
}
