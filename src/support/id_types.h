#pragma once

#include <cstdint>

namespace gs1
{
template<typename Tag>
struct Id final
{
    std::uint32_t value {0};

    constexpr bool operator==(const Id&) const = default;
    constexpr bool operator<(const Id& other) const noexcept
    {
        return value < other.value;
    }
};

struct SiteIdTag;
struct CampaignIdTag;
struct SiteRunIdTag;
struct FactionIdTag;
struct TechNodeIdTag;
struct ItemIdTag;
struct RecipeIdTag;
struct PlantIdTag;
struct StructureIdTag;
struct TaskTemplateIdTag;
struct TaskInstanceIdTag;
struct RewardCandidateIdTag;
struct EventTemplateIdTag;
struct ModifierIdTag;
struct DeliveryIdTag;
struct WorkOrderIdTag;
struct RuntimeActionIdTag;

using CampaignId = Id<CampaignIdTag>;
using SiteId = Id<SiteIdTag>;
using SiteRunId = Id<SiteRunIdTag>;
using FactionId = Id<FactionIdTag>;
using TechNodeId = Id<TechNodeIdTag>;
using ItemId = Id<ItemIdTag>;
using RecipeId = Id<RecipeIdTag>;
using PlantId = Id<PlantIdTag>;
using StructureId = Id<StructureIdTag>;
using TaskTemplateId = Id<TaskTemplateIdTag>;
using TaskInstanceId = Id<TaskInstanceIdTag>;
using RewardCandidateId = Id<RewardCandidateIdTag>;
using EventTemplateId = Id<EventTemplateIdTag>;
using ModifierId = Id<ModifierIdTag>;
using DeliveryId = Id<DeliveryIdTag>;
using WorkOrderId = Id<WorkOrderIdTag>;
using RuntimeActionId = Id<RuntimeActionIdTag>;

struct TileCoord final
{
    std::int32_t x {0};
    std::int32_t y {0};
};
}  // namespace gs1
