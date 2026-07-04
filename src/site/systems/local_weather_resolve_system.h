#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class LocalWeatherResolveSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<SiteRunStartedMessage>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_LOCAL_WEATHER_RESOLVE;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 8U;

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRunStartedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "LocalWeatherResolveSystem",
            site_component_mask_of(
                SiteComponent::TileLayout,
                SiteComponent::TileEcology,
                SiteComponent::TileWeather,
                SiteComponent::TilePlantWeatherContribution,
                SiteComponent::TileDeviceWeatherContribution,
                SiteComponent::Weather,
                SiteComponent::Modifier,
                SiteComponent::LocalWeatherRuntime),
            site_component_mask_of(
                SiteComponent::TileWeather,
                SiteComponent::LocalWeatherRuntime)};
    }
};

template <>
struct system_state_tags<LocalWeatherResolveSystem>
{
    using type = type_list<>;
};
}  // namespace gs1
