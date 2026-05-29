#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class EcologySystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        SiteGroundCoverPlacedMessage,
        SiteTilePlantingCompletedMessage,
        SiteTileWateredMessage,
        SiteTileBurialClearedMessage,
        SiteTileHarvestedMessage>;
    using emitted_runtime_messages =
        type_list<runtime_message_type_constant<GS1_ENGINE_MESSAGE_LOG_TEXT>>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_ECOLOGY;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 13U;

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<EcologySystem>();
    }

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] GameMessageSubscriptionSpan subscribed_game_messages() const noexcept override;
    [[nodiscard]] HostMessageSubscriptionSpan subscribed_host_messages() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRunStartedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteGroundCoverPlacedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteTilePlantingCompletedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteTileWateredMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteTileBurialClearedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteTileHarvestedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "EcologySystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::TileWeather,
                SiteComponent::TilePlantWeatherContribution,
                SiteComponent::TileDeviceWeatherContribution,
                SiteComponent::Modifier,
                SiteComponent::Counters,
                SiteComponent::Objective),
            site_component_mask_of(
                SiteComponent::TileEcology,
                SiteComponent::Counters)};
    }
};

template <>
struct system_state_tags<EcologySystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
