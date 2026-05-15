#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace gs1
{
struct LoadoutSlot final
{
    ItemId item_id {};
    std::uint32_t quantity {0};
    bool occupied {false};
};

struct LoadoutPlannerMetaState final
{
    SiteId selected_target_site_id {};
    std::uint32_t support_quota_per_contributor {0};
    std::uint32_t support_quota {0};
    bool has_selected_target_site_id {false};
};

struct LoadoutPlannerState final
{
    std::optional<SiteId> selected_target_site_id {};
    std::vector<LoadoutSlot> baseline_deployment_items {};
    std::vector<LoadoutSlot> available_exported_support_items {};
    std::vector<LoadoutSlot> selected_loadout_slots {};
    std::vector<ModifierId> active_nearby_aura_modifier_ids {};
    std::uint32_t support_quota_per_contributor {0};
    std::uint32_t support_quota {0};
};
}  // namespace gs1
