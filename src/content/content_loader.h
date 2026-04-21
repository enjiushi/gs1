#pragma once

#include "content/content_database.h"

namespace gs1
{
class ContentLoader final
{
public:
    [[nodiscard]] static ContentDatabase load_prototype_content();
};

[[nodiscard]] const ContentDatabase& prototype_content_database() noexcept;
}  // namespace gs1
