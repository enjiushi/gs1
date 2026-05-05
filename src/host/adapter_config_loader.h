#pragma once

#include <filesystem>
#include <string>

struct Gs1AdapterConfigBlob final
{
    std::string json_utf8 {};
};

[[nodiscard]] Gs1AdapterConfigBlob load_adapter_config_blob(
    const std::filesystem::path& project_config_root,
    const std::filesystem::path& adapter_config_directory);
