#pragma once

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "content/content_index.h"
#include "content/prototype_content.h"

#include <deque>
#include <string>
#include <vector>

namespace gs1
{
struct ContentDatabase final
{
    const PrototypeCampaignContent* prototype_campaign {nullptr};
    std::vector<ItemDef> item_defs {};
    std::vector<PlantDef> plant_defs {};
    std::vector<StructureDef> structure_defs {};
    std::vector<CraftRecipeDef> craft_recipe_defs {};
    std::deque<std::string> owned_strings {};
    ContentIndex index {};
};
}  // namespace gs1
