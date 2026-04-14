#include "testing/system_test_asset.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace gs1::testing
{
namespace
{
std::string trim_copy(std::string_view value)
{
    std::size_t start = 0U;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        start += 1U;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1U])) != 0)
    {
        end -= 1U;
    }

    return std::string(value.substr(start, end - start));
}

std::string lowercase_ascii_copy(std::string_view value)
{
    std::string result(value);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return result;
}

std::string path_text(const std::filesystem::path& path)
{
    return path.lexically_normal().string();
}
}  // namespace

bool is_system_test_asset_path(const std::filesystem::path& path)
{
    return lowercase_ascii_copy(path.extension().string()) == k_system_test_asset_extension;
}

bool load_system_test_asset_document(
    const std::filesystem::path& path,
    SystemTestAssetDocument& out_document,
    std::string& out_error)
{
    out_document = {};
    out_error.clear();

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        out_error = "Unable to open system test asset: " + path_text(path);
        return false;
    }

    out_document.path = path;

    bool reading_payload = false;
    std::ostringstream payload_stream {};
    std::string line {};
    std::uint32_t line_number = 0U;

    while (std::getline(input, line))
    {
        line_number += 1U;

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (reading_payload)
        {
            payload_stream << line << '\n';
            continue;
        }

        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.front() == '#')
        {
            continue;
        }

        if (trimmed == "---")
        {
            reading_payload = true;
            continue;
        }

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string::npos)
        {
            out_error = path_text(path) + ":" + std::to_string(line_number) +
                " expected 'key = value' metadata before the payload delimiter.";
            return false;
        }

        const std::string key = lowercase_ascii_copy(trim_copy(trimmed.substr(0U, separator)));
        const std::string value = trim_copy(trimmed.substr(separator + 1U));

        if (key.empty())
        {
            out_error = path_text(path) + ":" + std::to_string(line_number) +
                " contains an empty metadata key.";
            return false;
        }

        if (value.empty())
        {
            out_error = path_text(path) + ":" + std::to_string(line_number) +
                " contains an empty value for key '" + key + "'.";
            return false;
        }

        const auto [it, inserted] = out_document.metadata.emplace(key, value);
        if (!inserted)
        {
            out_error = path_text(path) + ":" + std::to_string(line_number) +
                " defines metadata key '" + key + "' more than once.";
            return false;
        }
    }

    auto metadata_value = [&](std::string_view key) -> const std::string* {
        const auto it = out_document.metadata.find(std::string(key));
        return it == out_document.metadata.end() ? nullptr : &it->second;
    };

    const std::string* system_name = metadata_value("system");
    const std::string* test_name = metadata_value("name");
    const std::string* runner_name = metadata_value("runner");

    if (system_name == nullptr || test_name == nullptr || runner_name == nullptr)
    {
        out_error = "System test asset " + path_text(path) +
            " must declare 'system', 'name', and 'runner' metadata keys.";
        return false;
    }

    out_document.system_name = *system_name;
    out_document.test_name = *test_name;
    out_document.runner_name = *runner_name;

    if (const std::string* description = metadata_value("description"); description != nullptr)
    {
        out_document.description = *description;
    }

    out_document.payload_text = payload_stream.str();
    return true;
}

bool discover_system_test_asset_documents(
    const std::filesystem::path& root,
    std::vector<SystemTestAssetDocument>& out_documents,
    std::vector<std::string>& out_errors)
{
    out_documents.clear();
    out_errors.clear();

    if (root.empty() || !std::filesystem::exists(root))
    {
        return true;
    }

    if (!std::filesystem::is_directory(root))
    {
        out_errors.push_back("System test asset root is not a directory: " + path_text(root));
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || !is_system_test_asset_path(entry.path()))
        {
            continue;
        }

        SystemTestAssetDocument document {};
        std::string error {};
        if (!load_system_test_asset_document(entry.path(), document, error))
        {
            out_errors.push_back(std::move(error));
            continue;
        }

        out_documents.push_back(std::move(document));
    }

    std::sort(
        out_documents.begin(),
        out_documents.end(),
        [](const SystemTestAssetDocument& left, const SystemTestAssetDocument& right) {
            if (left.system_name != right.system_name)
            {
                return left.system_name < right.system_name;
            }

            if (left.test_name != right.test_name)
            {
                return left.test_name < right.test_name;
            }

            return left.path.lexically_normal().string() < right.path.lexically_normal().string();
        });

    return out_errors.empty();
}
}  // namespace gs1::testing
