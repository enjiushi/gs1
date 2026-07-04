#pragma once

#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"
#include "gs1/status.h"

namespace gs1
{
class WeatherEventSystem final : public IRuntimeSystem
{
public:
    using subscribed_messages = type_list<
        SiteRunStartedMessage,
        SiteAttemptEndedMessage>;
    static constexpr std::optional<Gs1RuntimeProfileSystemId> profile_id =
        GS1_RUNTIME_PROFILE_SYSTEM_WEATHER_EVENT;
    static constexpr std::optional<std::uint32_t> fixed_step_order_value = 4U;

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteRunStartedMessage& message);
    [[nodiscard]] Gs1Status handle(
        RuntimeInvocation& invocation,
        const SiteAttemptEndedMessage& message);
    void run(RuntimeInvocation& invocation) override;

    [[nodiscard]] static constexpr SiteSystemAccess access() noexcept
    {
        return SiteSystemAccess {
            "WeatherEventSystem",
            site_component_mask_of(
                SiteComponent::RunMeta,
                SiteComponent::Objective,
                SiteComponent::Weather,
                SiteComponent::Event),
            site_component_mask_of(
                SiteComponent::Weather,
                SiteComponent::Event)};
    }

};

template <>
struct system_state_tags<WeatherEventSystem>
{
    using type = type_list<RuntimeFixedStepSecondsTag>;
};
}  // namespace gs1
