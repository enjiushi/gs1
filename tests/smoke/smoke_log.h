#pragma once

#include <filesystem>

namespace smoke_log
{
bool initialize_file_sink(const std::filesystem::path& file_path);
void shutdown_file_sink() noexcept;

void infof(const char* format, ...);
void errorf(const char* format, ...);
}  // namespace smoke_log
