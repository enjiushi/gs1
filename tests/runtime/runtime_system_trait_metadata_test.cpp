#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_progression_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "campaign/systems/technology_system.h"
#include "runtime/game_systems.h"
#include "runtime/system_pack.h"
#include "runtime/system_interface.h"
#include "site/systems/action_execution_system.h"
#include "site/systems/camp_durability_system.h"
#include "site/systems/craft_system.h"
#include "site/systems/device_maintenance_system.h"
#include "site/systems/device_support_system.h"
#include "site/systems/device_weather_contribution_system.h"
#include "site/systems/ecology_system.h"
#include "site/systems/economy_phone_system.h"
#include "site/systems/failure_recovery_system.h"
#include "site/systems/inventory_system.h"
#include "site/systems/local_weather_resolve_system.h"
#include "site/systems/modifier_system.h"
#include "site/systems/plant_weather_contribution_system.h"
#include "site/systems/placement_validation_system.h"
#include "site/systems/site_completion_system.h"
#include "site/systems/site_flow_system.h"
#include "site/systems/site_time_system.h"
#include "site/systems/task_board_system.h"
#include "site/systems/weather_event_system.h"
#include "site/systems/worker_condition_system.h"

#include <optional>
#include <type_traits>

namespace
{
using gs1::CampaignFlowSystem;
using gs1::GameSystems;
using gs1::GameMessage;
using gs1::GameMessageSubscriptionSpan;
using gs1::HostMessageSubscriptionSpan;
using gs1::IRuntimeSystem;
using gs1::RuntimeInvocation;
using gs1::SiteTimeSystem;

using ExpectedRuntimeMessageManifest = gs1::type_list<
    gs1::runtime_message_type_constant<GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE>,
    gs1::runtime_message_type_constant<GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE>,
    gs1::runtime_message_type_constant<GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE>,
    gs1::runtime_message_type_constant<GS1_ENGINE_MESSAGE_LOG_TEXT>>;

template <typename System>
constexpr bool has_declared_runtime_metadata_v =
    gs1::runtime_profile_system_id_v<System>.has_value() ||
    gs1::runtime_fixed_step_order_v<System>.has_value();

template <typename System>
constexpr bool has_declared_runtime_message_manifest_v =
    requires { typename System::emitted_runtime_messages; };

struct FallbackOnlySystem final : IRuntimeSystem
{
    [[nodiscard]] const char* name() const noexcept override { return "FallbackOnlySystem"; }
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override
    {
        return {};
    }
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override
    {
        return {};
    }
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override
    {
        return GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW;
    }
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override
    {
        return 42U;
    }
    [[nodiscard]] Gs1Status process_game_message(RuntimeInvocation&, const GameMessage&) override
    {
        return GS1_STATUS_OK;
    }
    [[nodiscard]] Gs1Status process_host_message(RuntimeInvocation&, const Gs1HostMessage&) override
    {
        return GS1_STATUS_OK;
    }
    void run(RuntimeInvocation&) override {}
};

template <typename... Systems>
constexpr bool all_game_systems_declare_runtime_metadata(gs1::system_pack<Systems...>) noexcept
{
    return (has_declared_runtime_metadata_v<Systems> && ...);
}
}  // namespace

int main()
{
    static_assert(gs1::runtime_profile_system_id_v<CampaignFlowSystem> == GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW);
    static_assert(!gs1::runtime_fixed_step_order_v<CampaignFlowSystem>.has_value());
    static_assert(gs1::runtime_profile_system_id_v<SiteTimeSystem> == GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME);
    static_assert(gs1::runtime_fixed_step_order_v<SiteTimeSystem> == 1U);
    static_assert(!gs1::runtime_profile_system_id_v<FallbackOnlySystem>.has_value());
    static_assert(!gs1::runtime_fixed_step_order_v<FallbackOnlySystem>.has_value());
    static_assert(gs1::type_list_size_v<gs1::runtime_emitted_runtime_messages_t<FallbackOnlySystem>> == 0U);
    static_assert(all_game_systems_declare_runtime_metadata(GameSystems {}));
    static_assert(has_declared_runtime_message_manifest_v<CampaignFlowSystem>);
    static_assert(!has_declared_runtime_message_manifest_v<FallbackOnlySystem>);
    static_assert(std::is_same_v<
                  gs1::runtime_emitted_runtime_message_manifest_t<GameSystems>,
                  ExpectedRuntimeMessageManifest>);
    static_assert(gs1::type_list_contains_v<
                  gs1::runtime_message_type_constant<GS1_ENGINE_MESSAGE_LOG_TEXT>,
                  gs1::runtime_emitted_runtime_messages_t<gs1::EcologySystem>>);
    static_assert(gs1::type_list_contains_v<
                  gs1::runtime_message_type_constant<GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE>,
                  gs1::runtime_emitted_runtime_messages_t<gs1::ActionExecutionSystem>>);

    FallbackOnlySystem fallback {};
    if (gs1::runtime_profile_system_id_for<FallbackOnlySystem>(fallback) != GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW)
    {
        return 1;
    }
    if (gs1::runtime_fixed_step_order_for<FallbackOnlySystem>(fallback) != 42U)
    {
        return 2;
    }

    CampaignFlowSystem campaign_flow {};
    if (gs1::runtime_profile_system_id_for<CampaignFlowSystem>(campaign_flow) !=
        GS1_RUNTIME_PROFILE_SYSTEM_CAMPAIGN_FLOW)
    {
        return 3;
    }
    if (gs1::runtime_fixed_step_order_for<CampaignFlowSystem>(campaign_flow).has_value())
    {
        return 4;
    }

    SiteTimeSystem site_time {};
    if (gs1::runtime_profile_system_id_for<SiteTimeSystem>(site_time) != GS1_RUNTIME_PROFILE_SYSTEM_SITE_TIME)
    {
        return 5;
    }
    if (gs1::runtime_fixed_step_order_for<SiteTimeSystem>(site_time) != 1U)
    {
        return 6;
    }

    return 0;
}
