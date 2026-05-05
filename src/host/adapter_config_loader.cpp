#include "host/adapter_config_loader.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
struct SerializedAdapterConfigFile final
{
    std::string relative_path_utf8 {};
    std::string content_utf8 {};
};

std::string escape_json_string(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 16U);
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result.push_back(ch);
            break;
        }
    }
    return result;
}

std::string read_file_utf8(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}
}

Gs1AdapterConfigBlob load_adapter_config_blob(
    const std::filesystem::path& project_config_root,
    const std::filesystem::path& adapter_config_directory)
{
    std::vector<SerializedAdapterConfigFile> files;
    if (std::filesystem::exists(adapter_config_directory))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(adapter_config_directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            SerializedAdapterConfigFile file {};
            file.relative_path_utf8 = std::filesystem::relative(entry.path(), adapter_config_directory).generic_string();
            file.content_utf8 = read_file_utf8(entry.path());
            files.push_back(std::move(file));
        }
    }

    std::sort(files.begin(), files.end(), [](const SerializedAdapterConfigFile& lhs, const SerializedAdapterConfigFile& rhs) {
        return lhs.relative_path_utf8 < rhs.relative_path_utf8;
    });

    Gs1AdapterConfigBlob blob {};
    std::ostringstream json;
    json << "{";
    json << "\"project_config_root\":\""
         << escape_json_string(project_config_root.generic_string())
         << "\",";
    json << "\"adapter_config_root\":\""
         << escape_json_string(adapter_config_directory.generic_string())
         << "\",";
    json << "\"files\":[";
    for (std::size_t index = 0; index < files.size(); ++index)
    {
        if (index > 0U)
        {
            json << ",";
        }

        json << "{";
        json << "\"relative_path\":\""
             << escape_json_string(files[index].relative_path_utf8)
             << "\",";
        json << "\"content\":\""
             << escape_json_string(files[index].content_utf8)
             << "\"";
        json << "}";
    }
    json << "]";
    json << "}";
    blob.json_utf8 = json.str();
    return blob;
}
