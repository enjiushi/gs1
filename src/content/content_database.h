#pragma once

#include "content/defs/craft_recipe_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/modifier_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/reward_defs.h"
#include "content/defs/structure_defs.h"
#include "content/defs/task_defs.h"
#include "content/defs/technology_defs.h"
#include "content/content_index.h"
#include "content/prototype_content.h"
#include "site/defs/site_action_defs.h"

#include <deque>
#include <string>
#include <vector>

namespace gs1
{
struct ContentDatabase final
{
    PrototypeCampaignContent prototype_campaign {};
    std::vector<ItemDef> item_defs {};
    std::vector<PlantDef> plant_defs {};
    std::vector<StructureDef> structure_defs {};
    std::vector<CraftRecipeDef> craft_recipe_defs {};
    std::vector<ModifierPresetDef> nearby_aura_modifier_presets {};
    std::vector<ModifierPresetDef> run_modifier_presets {};
    std::vector<TaskTemplateDef> task_template_defs {};
    std::vector<RewardCandidateDef> reward_candidate_defs {};
    std::vector<SiteActionDef> site_action_defs {};
    std::vector<TechnologyTierDef> technology_tier_defs {};
    std::vector<TotalReputationTierDef> total_reputation_tier_defs {};
    std::vector<ReputationUnlockDef> reputation_unlock_defs {};
    std::vector<TechnologyNodeDef> technology_node_defs {};
    std::vector<PlantId> initial_unlocked_plant_ids {};
    std::deque<std::string> owned_strings {};
    ContentIndex index {};
};
}  // namespace gs1
