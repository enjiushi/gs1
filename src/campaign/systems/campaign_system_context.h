#pragma once

#include "campaign/campaign_state.h"
#include "runtime/runtime_state_access.h"
#include "runtime/state_set.h"

namespace gs1
{
struct CampaignSystemContext final
{
    CampaignState& campaign;
};

class CampaignStateAccess final
{
public:
    explicit CampaignStateAccess(RuntimeInvocation& invocation) noexcept
        : invocation_(&invocation)
    {
    }

    [[nodiscard]] bool has_campaign() const noexcept
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignCore>(*invocation_->owned_state())
                .has_value();
        }

        return runtime_invocation_state_ref<RuntimeCampaignTag>(*invocation_).has_value();
    }

    [[nodiscard]] Gs1AppState& app_state() const noexcept
    {
        return runtime_invocation_state_ref<RuntimeAppStateTag>(*invocation_);
    }

    [[nodiscard]] CampaignId& campaign_id() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignCore>(*invocation_->owned_state())
                ->campaign_id;
        }

        return aggregate_campaign().campaign_id;
    }

    [[nodiscard]] std::uint64_t& campaign_seed() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignCore>(*invocation_->owned_state())
                ->campaign_seed;
        }

        return aggregate_campaign().campaign_seed;
    }

    [[nodiscard]] double& campaign_clock_minutes_elapsed() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignCore>(*invocation_->owned_state())
                ->campaign_clock_minutes_elapsed;
        }

        return aggregate_campaign().campaign_clock_minutes_elapsed;
    }

    [[nodiscard]] std::uint32_t& campaign_days_total() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignCore>(*invocation_->owned_state())
                ->campaign_days_total;
        }

        return aggregate_campaign().campaign_days_total;
    }

    [[nodiscard]] std::uint32_t& campaign_days_remaining() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignCore>(*invocation_->owned_state())
                ->campaign_days_remaining;
        }

        return aggregate_campaign().campaign_days_remaining;
    }

    [[nodiscard]] std::optional<SiteId>& active_site_id() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignCore>(*invocation_->owned_state())
                ->active_site_id;
        }

        return aggregate_campaign().active_site_id;
    }

    [[nodiscard]] RegionalMapState& regional_map() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignRegionalMap>(*invocation_->owned_state())
                .value();
        }

        return aggregate_campaign().regional_map_state;
    }

    [[nodiscard]] std::vector<FactionProgressState>& faction_progress() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignFactionProgress>(*invocation_->owned_state())
                .value();
        }

        return aggregate_campaign().faction_progress;
    }

    [[nodiscard]] TechnologyState& technology() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignTechnology>(*invocation_->owned_state())
                .value();
        }

        return aggregate_campaign().technology_state;
    }

    [[nodiscard]] LoadoutPlannerState& loadout_planner() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignLoadoutPlanner>(*invocation_->owned_state())
                .value();
        }

        return aggregate_campaign().loadout_planner_state;
    }

    [[nodiscard]] std::vector<SiteMetaState>& sites() const
    {
        if (using_split_state_sets())
        {
            return invocation_->state_manager()
                ->state<StateSetId::CampaignSites>(*invocation_->owned_state())
                .value();
        }

        return aggregate_campaign().sites;
    }

    void clear_campaign() const
    {
        if (using_split_state_sets())
        {
            invocation_->state_manager()->state<StateSetId::CampaignCore>(*invocation_->owned_state()).reset();
            invocation_->state_manager()->state<StateSetId::CampaignRegionalMap>(*invocation_->owned_state())
                .reset();
            invocation_->state_manager()->state<StateSetId::CampaignFactionProgress>(*invocation_->owned_state())
                .reset();
            invocation_->state_manager()->state<StateSetId::CampaignTechnology>(*invocation_->owned_state())
                .reset();
            invocation_->state_manager()
                ->state<StateSetId::CampaignLoadoutPlanner>(*invocation_->owned_state())
                .reset();
            invocation_->state_manager()->state<StateSetId::CampaignSites>(*invocation_->owned_state()).reset();
            return;
        }

        runtime_invocation_state_ref<RuntimeCampaignTag>(*invocation_).reset();
    }

    void assign_from(const CampaignState& campaign) const
    {
        app_state() = campaign.app_state;

        if (using_split_state_sets())
        {
            invocation_->state_manager()->state<StateSetId::CampaignCore>(*invocation_->owned_state()) =
                CampaignCoreState {
                    campaign.campaign_id,
                    campaign.campaign_seed,
                    campaign.campaign_clock_minutes_elapsed,
                    campaign.campaign_days_total,
                    campaign.campaign_days_remaining,
                    campaign.app_state,
                    campaign.active_site_id};
            invocation_->state_manager()->state<StateSetId::CampaignRegionalMap>(*invocation_->owned_state()) =
                campaign.regional_map_state;
            invocation_->state_manager()
                ->state<StateSetId::CampaignFactionProgress>(*invocation_->owned_state()) =
                campaign.faction_progress;
            invocation_->state_manager()->state<StateSetId::CampaignTechnology>(*invocation_->owned_state()) =
                campaign.technology_state;
            invocation_->state_manager()
                ->state<StateSetId::CampaignLoadoutPlanner>(*invocation_->owned_state()) =
                campaign.loadout_planner_state;
            invocation_->state_manager()->state<StateSetId::CampaignSites>(*invocation_->owned_state()) =
                campaign.sites;
            return;
        }

        runtime_invocation_state_ref<RuntimeCampaignTag>(*invocation_) = campaign;
    }

private:
    [[nodiscard]] bool using_split_state_sets() const noexcept
    {
        return invocation_->owned_state() != nullptr && invocation_->state_manager() != nullptr;
    }

    [[nodiscard]] CampaignState& aggregate_campaign() const
    {
        return runtime_invocation_state_ref<RuntimeCampaignTag>(*invocation_).value();
    }

    RuntimeInvocation* invocation_ {nullptr};
};

[[nodiscard]] inline auto make_campaign_state_access(RuntimeInvocation& invocation) noexcept
    -> CampaignStateAccess
{
    return CampaignStateAccess {invocation};
}
}  // namespace gs1
