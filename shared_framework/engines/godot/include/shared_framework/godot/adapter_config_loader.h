#pragma once

#include <filesystem>
#include <string>

namespace shared_framework::godot {
struct AdapterConfigBlob final
{
    std::string json_utf8 {};
};

[[nodiscard]] AdapterConfigBlob load_adapter_config_blob(
    const std::filesystem::path& project_config_root,
    const std::filesystem::path& adapter_config_directory);

}  // namespace shared_framework::godot
