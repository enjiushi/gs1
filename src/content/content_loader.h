#pragma once

#include "content/content_database.h"

#include <filesystem>

namespace gs1
{
class ContentLoader final
{
public:
    [[nodiscard]] static ContentDatabase load_prototype_content(
        const std::filesystem::path& content_root);
};

[[nodiscard]] const ContentDatabase& prototype_content_database() noexcept;
void set_prototype_content_root(std::filesystem::path content_root);
}  // namespace gs1
