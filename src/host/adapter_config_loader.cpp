#include "host/adapter_config_loader.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string_view>
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

std::string unescape_json_string(std::string_view value)
{
    std::string result;
    result.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const char ch = value[index];
        if (ch != '\\' || (index + 1U) >= value.size())
        {
            result.push_back(ch);
            continue;
        }

        const char escaped = value[++index];
        switch (escaped)
        {
        case '\\':
            result.push_back('\\');
            break;
        case '"':
            result.push_back('"');
            break;
        case 'n':
            result.push_back('\n');
            break;
        case 'r':
            result.push_back('\r');
            break;
        case 't':
            result.push_back('\t');
            break;
        default:
            result.push_back(escaped);
            break;
        }
    }

    return result;
}

std::size_t skip_json_whitespace(std::string_view text, std::size_t index) noexcept
{
    while (index < text.size())
    {
        const char ch = text[index];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
        {
            break;
        }
        ++index;
    }
    return index;
}

std::size_t find_json_string_end(std::string_view text, std::size_t start_index) noexcept
{
    bool escaping = false;
    for (std::size_t index = start_index; index < text.size(); ++index)
    {
        const char ch = text[index];
        if (escaping)
        {
            escaping = false;
            continue;
        }
        if (ch == '\\')
        {
            escaping = true;
            continue;
        }
        if (ch == '"')
        {
            return index;
        }
    }

    return std::string_view::npos;
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

std::filesystem::path extract_project_config_root_from_adapter_config_json(
    const std::string_view adapter_config_json_utf8)
{
    constexpr std::string_view key = "\"project_config_root\"";
    const std::size_t key_pos = adapter_config_json_utf8.find(key);
    if (key_pos == std::string_view::npos)
    {
        return {};
    }

    std::size_t cursor = skip_json_whitespace(adapter_config_json_utf8, key_pos + key.size());
    if (cursor >= adapter_config_json_utf8.size() || adapter_config_json_utf8[cursor] != ':')
    {
        return {};
    }

    cursor = skip_json_whitespace(adapter_config_json_utf8, cursor + 1U);
    if (cursor >= adapter_config_json_utf8.size() || adapter_config_json_utf8[cursor] != '"')
    {
        return {};
    }

    const std::size_t value_start = cursor + 1U;
    const std::size_t value_end = find_json_string_end(adapter_config_json_utf8, value_start);
    if (value_end == std::string_view::npos)
    {
        return {};
    }

    return std::filesystem::path {unescape_json_string(adapter_config_json_utf8.substr(value_start, value_end - value_start))};
}
