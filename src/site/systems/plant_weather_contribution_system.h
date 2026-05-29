#pragma once

#include "gs1/status.h"
#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class PlantWeatherContributionSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        TileEcologyChangedMessage,
        TileEcologyBatchChangedMessage>;
    using emitted_runtime_messages = type_list<>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 6U;

    [[nodiscard]] std::span<const StateSetId> owned_state_sets() const noexcept override
    {
        return site_access_owned_state_sets<PlantWeatherContributionSystem>();
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
        const TileEcologyChangedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const TileEcologyBatchChangedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "PlantWeatherContributionSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::TilePlantWeatherContribution,
                SiteComponent::Weather,
                SiteComponent::PlantWeatherRuntime),
            site_component_mask_of(
                SiteComponent::TilePlantWeatherContribution,
                SiteComponent::PlantWeatherRuntime)};
    }
};

template <>
struct system_state_tags<PlantWeatherContributionSystem>
{
    using type = type_list<>;
};
}  // namespace gs1
