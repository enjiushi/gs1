#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace gs1::testing
{
inline constexpr std::string_view k_system_test_asset_extension = ".gs1systemtest";

struct SystemTestAssetDocument final
{
    std::filesystem::path path {};
    std::string system_name {};
    std::string test_name {};
    std::string runner_name {};
    std::string description {};
    std::map<std::string, std::string, std::less<>> metadata {};
    std::string payload_text {};
};

[[nodiscard]] bool is_system_test_asset_path(const std::filesystem::path& path);

[[nodiscard]] bool load_system_test_asset_document(
    const std::filesystem::path& path,
    SystemTestAssetDocument& out_document,
    std::string& out_error);

[[nodiscard]] bool discover_system_test_asset_documents(
    const std::filesystem::path& root,
    std::vector<SystemTestAssetDocument>& out_documents,
    std::vector<std::string>& out_errors);
}  // namespace gs1::testing
