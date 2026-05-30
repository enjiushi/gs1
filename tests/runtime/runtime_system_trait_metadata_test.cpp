#include "campaign/systems/campaign_flow_system.h"
#include "campaign/systems/campaign_progression_system.h"
#include "campaign/systems/campaign_time_system.h"
#include "campaign/systems/loadout_planner_system.h"
#include "campaign/systems/technology_system.h"
#include "messages/game_message.h"
#include "runtime/game_runtime.h"
#include "runtime/game_systems.h"
#include "runtime/system_pack.h"
#include "runtime/system_interface.h"
#include "runtime/typed_gameplay_dispatch_traits.h"
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

#include <array>
#include <optional>
#include <type_traits>

namespace gs1
{
struct GameRuntimeProjectionTestAccess
{
    using system_storage_type = decltype(std::declval<GameRuntime&>().systems_);

    [[nodiscard]] static std::size_t fixed_step_system_count(const GameRuntime& runtime)
    {
        return runtime.fixed_step_systems_.size();
    }

    [[nodiscard]] static std::uint32_t fixed_step_order(
        const GameRuntime& runtime,
        std::size_t index)
    {
        return runtime.fixed_step_systems_[index].order;
    }
};
}  // namespace gs1

namespace
{
using gs1::CampaignFlowSystem;
using gs1::GameSystems;
using gs1::GameMessage;
using gs1::GameMessageType;
using gs1::GameRuntime;
using gs1::IRuntimeSystem;
using gs1::RuntimeInvocation;
using gs1::SiteTimeSystem;
using gs1::StartNewCampaignMessage;
using gs1::TechnologyState;
using gs1::type_list_size_v;

constexpr std::array<std::uint32_t, 20U> k_expected_fixed_step_orders {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 19U, 20U};

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
constexpr bool has_declared_subscribed_messages_v =
    requires { typename System::subscribed_messages; };

template <typename System>
constexpr bool has_declared_runtime_message_manifest_v =
    requires { typename System::emitted_runtime_messages; };

template <typename Message>
constexpr bool has_typed_dispatch_v =
    gs1::typed_gameplay_dispatch_traits<Message>::enabled;

template <typename Message>
constexpr bool typed_dispatch_has_subscribers_v =
    type_list_size_v<typename gs1::typed_gameplay_dispatch_traits<Message>::subscribers> > 0U;

struct FallbackOnlySystem final : IRuntimeSystem
{
    [[nodiscard]] const char* name() const noexcept override { return "FallbackOnlySystem"; }
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override
    {
        return GS1_RUNTIME_PROFILE_SYSTEM_SITE_FLOW;
    }
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override
    {
        return 42U;
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
    static_assert(has_declared_subscribed_messages_v<gs1::CampaignTimeSystem>);
    static_assert(has_declared_subscribed_messages_v<gs1::DeviceSupportSystem>);
    static_assert(has_declared_subscribed_messages_v<gs1::SiteFlowSystem>);
    static_assert(has_declared_subscribed_messages_v<gs1::SiteTimeSystem>);
    static_assert(has_declared_runtime_message_manifest_v<CampaignFlowSystem>);
    static_assert(!has_declared_runtime_message_manifest_v<FallbackOnlySystem>);
    static_assert(std::is_same_v<
                  gs1::GameRuntimeProjectionTestAccess::system_storage_type,
                  GameSystems::tuple_type>);
    static_assert(std::is_same_v<
                  gs1::runtime_emitted_runtime_message_manifest_t<GameSystems>,
                  ExpectedRuntimeMessageManifest>);
    static_assert(gs1::runtime_emitted_runtime_message_manifest_covers_v<
                  GameSystems,
                  ExpectedRuntimeMessageManifest>);
    static_assert(has_typed_dispatch_v<gs1::StartNewCampaignMessage>);
    static_assert(has_typed_dispatch_v<gs1::TaskAcceptRequestedMessage>);
    static_assert(has_typed_dispatch_v<gs1::TaskRewardClaimRequestedMessage>);
    static_assert(has_typed_dispatch_v<gs1::SiteModifierEndRequestedMessage>);
    static_assert(has_typed_dispatch_v<gs1::PhoneListingPurchaseRequestedMessage>);
    static_assert(has_typed_dispatch_v<gs1::PhoneListingSaleRequestedMessage>);
    static_assert(has_typed_dispatch_v<gs1::ContractorHireRequestedMessage>);
    static_assert(has_typed_dispatch_v<gs1::SiteUnlockablePurchaseRequestedMessage>);
    static_assert(has_typed_dispatch_v<gs1::PlacementModeCursorMovedMessage>);
    static_assert(has_typed_dispatch_v<gs1::TechnologyNodeClaimRequestedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::StartNewCampaignMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::TaskAcceptRequestedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::TaskRewardClaimRequestedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::SiteModifierEndRequestedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::PhoneListingPurchaseRequestedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::PhoneListingSaleRequestedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::ContractorHireRequestedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::SiteUnlockablePurchaseRequestedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::PlacementModeCursorMovedMessage>);
    static_assert(typed_dispatch_has_subscribers_v<gs1::TechnologyNodeClaimRequestedMessage>);
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

    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime runtime {create_desc};
    if (gs1::GameRuntimeProjectionTestAccess::fixed_step_system_count(runtime) !=
        k_expected_fixed_step_orders.size())
    {
        return 7;
    }
    for (std::size_t index = 0; index < k_expected_fixed_step_orders.size(); ++index)
    {
        if (gs1::GameRuntimeProjectionTestAccess::fixed_step_order(runtime, index) !=
            k_expected_fixed_step_orders[index])
        {
            return static_cast<int>(8 + index);
        }
    }

    if (runtime.handle_message(StartNewCampaignMessage {42ULL, 30U}) != GS1_STATUS_OK)
    {
        return 40;
    }

    if (!runtime.state().campaign_core.has_value())
    {
        return 41;
    }

    GameRuntime host_runtime {create_desc};
    const Gs1GameplayAction start_campaign_action {
        GS1_GAMEPLAY_ACTION_START_NEW_CAMPAIGN,
        0U,
        77ULL,
        45ULL};
    if (host_runtime.submit_gameplay_action(start_campaign_action) != GS1_STATUS_OK)
    {
        return 42;
    }

    Gs1Phase2Request phase2_request {};
    phase2_request.struct_size = sizeof(Gs1Phase2Request);
    Gs1Phase2Result phase2_result {};
    if (host_runtime.run_phase2(phase2_request, phase2_result) != GS1_STATUS_OK)
    {
        return 43;
    }

    if (!host_runtime.state().campaign_core.has_value())
    {
        return 44;
    }

    const Gs1GameplayAction claim_technology_action {
        GS1_GAMEPLAY_ACTION_CLAIM_TECHNOLOGY_NODE,
        1U,
        1ULL,
        0ULL};
    if (host_runtime.submit_gameplay_action(claim_technology_action) != GS1_STATUS_OK)
    {
        return 45;
    }

    if (host_runtime.run_phase2(phase2_request, phase2_result) != GS1_STATUS_OK)
    {
        return 46;
    }

    if (!host_runtime.state().campaign_technology.has_value())
    {
        return 47;
    }

    return 0;
}
