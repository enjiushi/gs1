#include "smoke_log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <share.h>

namespace
{
std::mutex g_log_mutex {};
FILE* g_log_file = nullptr;

void write_formatted(FILE* stream, const char* format, va_list arguments)
{
    std::vfprintf(stream, format, arguments);
    std::fflush(stream);
}

void write_message(FILE* stream, const char* format, va_list arguments)
{
    std::lock_guard<std::mutex> lock {g_log_mutex};

    va_list stream_arguments {};
    va_copy(stream_arguments, arguments);
    write_formatted(stream, format, stream_arguments);
    va_end(stream_arguments);

    if (g_log_file != nullptr)
    {
        va_list file_arguments {};
        va_copy(file_arguments, arguments);
        write_formatted(g_log_file, format, file_arguments);
        va_end(file_arguments);
    }
}
}  // namespace

namespace smoke_log
{
bool initialize_file_sink(const std::filesystem::path& file_path)
{
    std::lock_guard<std::mutex> lock {g_log_mutex};

    shutdown_file_sink();

    if (!file_path.parent_path().empty())
    {
        std::error_code error {};
        std::filesystem::create_directories(file_path.parent_path(), error);
        if (error)
        {
            return false;
        }
    }

    FILE* file = _fsopen(file_path.string().c_str(), "w", _SH_DENYNO);
    if (file == nullptr)
    {
        return false;
    }

    g_log_file = file;
    return true;
}

void shutdown_file_sink() noexcept
{
    if (g_log_file != nullptr)
    {
        std::fflush(g_log_file);
        std::fclose(g_log_file);
        g_log_file = nullptr;
    }
}

void infof(const char* format, ...)
{
    va_list arguments {};
    va_start(arguments, format);
    write_message(stdout, format, arguments);
    va_end(arguments);
}

void errorf(const char* format, ...)
{
    va_list arguments {};
    va_start(arguments, format);
    write_message(stderr, format, arguments);
    va_end(arguments);
}
}  // namespace smoke_log
