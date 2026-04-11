#pragma once

#include "content/content_index.h"
#include "content/prototype_content.h"

namespace gs1
{
struct ContentDatabase final
{
    const PrototypeCampaignContent* prototype_campaign {nullptr};
    ContentIndex index {};
};
}  // namespace gs1
