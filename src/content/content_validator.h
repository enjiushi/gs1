#pragma once

#include <cstdint>

namespace gs1
{
enum class ContentValidationSeverity : std::uint32_t
{
    Info = 0,
    Warning = 1,
    Error = 2
};

struct ContentValidationIssue final
{
    ContentValidationSeverity severity {ContentValidationSeverity::Info};
    const char* message {nullptr};
};
}  // namespace gs1
