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

namespace gs1
{
struct GameRuntimeProjectionTestAccess
{
    [[nodiscard]] static std::size_t legacy_game_message_subscriber_count(
        const GameRuntime& runtime,
        GameMessageType type)
    {
        return runtime.message_subscribers_[static_cast<std::size_t>(type)].size();
    }

    [[nodiscard]] static std::size_t host_message_subscriber_count(
        const GameRuntime& runtime,
        Gs1HostMessageType type)
    {
        return runtime.host_message_subscribers_[static_cast<std::size_t>(type)].size();
    }
};
}  // namespace gs1

namespace
{
using gs1::CampaignFlowSystem;
using gs1::GameSystems;
using gs1::GameMessage;
using gs1::GameMessageType;
using gs1::GameMessageSubscriptionSpan;
using gs1::GameRuntime;
using gs1::HostMessageSubscriptionSpan;
using gs1::IRuntimeSystem;
using gs1::RuntimeInvocation;
using gs1::SiteTimeSystem;
using gs1::StartNewCampaignMessage;
using gs1::TechnologyState;

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
    static_assert(has_declared_subscribed_messages_v<gs1::CampaignTimeSystem>);
    static_assert(has_declared_subscribed_messages_v<gs1::DeviceSupportSystem>);
    static_assert(has_declared_subscribed_messages_v<gs1::SiteFlowSystem>);
    static_assert(has_declared_subscribed_messages_v<gs1::SiteTimeSystem>);
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

    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;

    GameRuntime runtime {create_desc};
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            runtime,
            GameMessageType::StartNewCampaign) != 0U)
    {
        return 7;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            runtime,
            GameMessageType::TaskAcceptRequested) != 0U)
    {
        return 8;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            runtime,
            GameMessageType::TaskRewardClaimRequested) != 0U)
    {
        return 9;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            runtime,
            GameMessageType::SiteModifierEndRequested) != 0U)
    {
        return 10;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            runtime,
            GameMessageType::PhoneListingPurchaseRequested) != 0U)
    {
        return 11;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            runtime,
            GameMessageType::PhoneListingSaleRequested) != 0U)
    {
        return 12;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            runtime,
            GameMessageType::ContractorHireRequested) != 0U)
    {
        return 13;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            runtime,
            GameMessageType::SiteUnlockablePurchaseRequested) != 0U)
    {
        return 14;
    }
    if (gs1::GameRuntimeProjectionTestAccess::host_message_subscriber_count(
            runtime,
            GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP) != 0U)
    {
        return 15;
    }

    GameMessage start_campaign_message {};
    start_campaign_message.type = GameMessageType::StartNewCampaign;
    start_campaign_message.set_payload(StartNewCampaignMessage {42ULL, 30U});
    if (runtime.handle_message(start_campaign_message) != GS1_STATUS_OK)
    {
        return 16;
    }

    if (!runtime.state().campaign_core.has_value())
    {
        return 17;
    }

    GameRuntime host_runtime {create_desc};
    if (gs1::GameRuntimeProjectionTestAccess::host_message_subscriber_count(
            host_runtime,
            GS1_HOST_EVENT_GAMEPLAY_ACTION) != 0U)
    {
        return 18;
    }
    if (gs1::GameRuntimeProjectionTestAccess::host_message_subscriber_count(
            host_runtime,
            GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP) != 0U)
    {
        return 19;
    }

    Gs1HostMessage host_message {};
    host_message.type = GS1_HOST_EVENT_GAMEPLAY_ACTION;
    host_message.payload.gameplay_action.action = Gs1GameplayAction {
        GS1_GAMEPLAY_ACTION_START_NEW_CAMPAIGN,
        0U,
        77ULL,
        45ULL};
    if (host_runtime.submit_host_messages(&host_message, 1U) != GS1_STATUS_OK)
    {
        return 20;
    }

    Gs1Phase2Request phase2_request {};
    phase2_request.struct_size = sizeof(Gs1Phase2Request);
    Gs1Phase2Result phase2_result {};
    if (host_runtime.run_phase2(phase2_request, phase2_result) != GS1_STATUS_OK)
    {
        return 21;
    }

    if (!host_runtime.state().campaign_core.has_value())
    {
        return 22;
    }

    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            host_runtime,
            GameMessageType::TechnologyNodeClaimRequested) != 0U)
    {
        return 23;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            host_runtime,
            GameMessageType::TaskAcceptRequested) != 0U)
    {
        return 24;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            host_runtime,
            GameMessageType::TaskRewardClaimRequested) != 0U)
    {
        return 25;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            host_runtime,
            GameMessageType::SiteModifierEndRequested) != 0U)
    {
        return 26;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            host_runtime,
            GameMessageType::PhoneListingPurchaseRequested) != 0U)
    {
        return 27;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            host_runtime,
            GameMessageType::PhoneListingSaleRequested) != 0U)
    {
        return 28;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            host_runtime,
            GameMessageType::ContractorHireRequested) != 0U)
    {
        return 29;
    }
    if (gs1::GameRuntimeProjectionTestAccess::legacy_game_message_subscriber_count(
            host_runtime,
            GameMessageType::SiteUnlockablePurchaseRequested) != 0U)
    {
        return 30;
    }

    host_message.payload.gameplay_action.action = Gs1GameplayAction {
        GS1_GAMEPLAY_ACTION_CLAIM_TECHNOLOGY_NODE,
        1U,
        1ULL,
        0ULL};
    if (host_runtime.submit_host_messages(&host_message, 1U) != GS1_STATUS_OK)
    {
        return 31;
    }

    if (host_runtime.run_phase2(phase2_request, phase2_result) != GS1_STATUS_OK)
    {
        return 32;
    }

    if (!host_runtime.state().campaign_technology.has_value())
    {
        return 33;
    }

    return 0;
}
