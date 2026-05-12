#pragma once

#include "gs1/status.h"
#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class DeviceWeatherContributionSystem final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status process_game_message(
        RuntimeInvocation& invocation,
        const GameMessage& message) override;
    [[nodiscard]] Gs1Status process_host_message(
        RuntimeInvocation& invocation,
        const Gs1HostMessage& message) override;
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "DeviceWeatherContributionSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileDeviceWeatherContribution,
                SiteComponent::DeviceCondition,
                SiteComponent::DeviceRuntime,
                SiteComponent::Weather,
                SiteComponent::DeviceWeatherRuntime),
            site_component_mask_of(
                SiteComponent::TileDeviceWeatherContribution,
                SiteComponent::DeviceWeatherRuntime)};
    }

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<DeviceWeatherContributionSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<DeviceWeatherContributionSystem>& context);
};

template <>
struct system_state_tags<DeviceWeatherContributionSystem>
{
    using type = type_list<
        RuntimeCampaignTag,
        RuntimeActiveSiteRunTag,
        RuntimeFixedStepSecondsTag,
        RuntimeMoveDirectionTag>;
};
}  // namespace gs1
