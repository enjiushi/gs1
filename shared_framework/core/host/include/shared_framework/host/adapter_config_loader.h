#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace shared_framework::host {
struct AdapterConfigBlob final
{
    std::string json_utf8 {};
};

[[nodiscard]] AdapterConfigBlob load_adapter_config_blob(
    const std::filesystem::path& project_config_root,
    const std::filesystem::path& adapter_config_directory);

[[nodiscard]] std::filesystem::path extract_project_config_root_from_adapter_config_json(
    std::string_view adapter_config_json_utf8);

}  // namespace shared_framework::host
