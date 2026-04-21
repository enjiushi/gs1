#pragma once

#include <cstdint>

#include "content/content_database.h"

#include <vector>

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

[[nodiscard]] std::vector<ContentValidationIssue> validate_content_database(
    const ContentDatabase& content);
}  // namespace gs1
