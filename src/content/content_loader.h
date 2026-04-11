#pragma once

#include "content/content_database.h"

namespace gs1
{
class ContentLoader final
{
public:
    [[nodiscard]] static ContentDatabase load_prototype_content();
};
}  // namespace gs1
