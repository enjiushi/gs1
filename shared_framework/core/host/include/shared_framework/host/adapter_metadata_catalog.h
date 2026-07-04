#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace shared_framework::host {
enum class AdapterMetadataDomain : std::uint8_t
{
    Modifier = 0,
    ReputationUnlock = 1,
    TechnologyNode = 2
};

struct AdapterMetadataEntry final
{
    std::string display_name {};
    std::string description {};
};

class AdapterMetadataCatalog final
{
public:
    AdapterMetadataCatalog() = default;

    void load_from_project_root(const std::filesystem::path& project_root);

    [[nodiscard]] const AdapterMetadataEntry* find(
        AdapterMetadataDomain domain,
        std::uint32_t id) const noexcept;

private:
    void load_modifier_metadata(const std::filesystem::path& path);
    void load_reputation_unlock_metadata(const std::filesystem::path& path);
    void load_technology_node_metadata(const std::filesystem::path& path);

    std::unordered_map<std::uint32_t, AdapterMetadataEntry> modifier_entries_ {};
    std::unordered_map<std::uint32_t, AdapterMetadataEntry> reputation_unlock_entries_ {};
    std::unordered_map<std::uint32_t, AdapterMetadataEntry> technology_node_entries_ {};
};

[[nodiscard]] const AdapterMetadataCatalog& adapter_metadata_catalog() noexcept;
void load_adapter_metadata_catalog_from_project_root(const std::filesystem::path& project_root);


}  // namespace shared_framework::host
