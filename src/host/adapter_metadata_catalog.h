#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

enum class Gs1AdapterMetadataDomain : std::uint8_t
{
    Modifier = 0,
    ReputationUnlock = 1,
    TechnologyNode = 2
};

struct Gs1AdapterMetadataEntry final
{
    std::string display_name {};
    std::string description {};
};

class Gs1AdapterMetadataCatalog final
{
public:
    Gs1AdapterMetadataCatalog() = default;

    void load_from_project_root(const std::filesystem::path& project_root);

    [[nodiscard]] const Gs1AdapterMetadataEntry* find(
        Gs1AdapterMetadataDomain domain,
        std::uint32_t id) const noexcept;

private:
    void load_modifier_metadata(const std::filesystem::path& path);
    void load_reputation_unlock_metadata(const std::filesystem::path& path);
    void load_technology_node_metadata(const std::filesystem::path& path);

    std::unordered_map<std::uint32_t, Gs1AdapterMetadataEntry> modifier_entries_ {};
    std::unordered_map<std::uint32_t, Gs1AdapterMetadataEntry> reputation_unlock_entries_ {};
    std::unordered_map<std::uint32_t, Gs1AdapterMetadataEntry> technology_node_entries_ {};
};

[[nodiscard]] const Gs1AdapterMetadataCatalog& adapter_metadata_catalog() noexcept;
void load_adapter_metadata_catalog_from_project_root(const std::filesystem::path& project_root);

