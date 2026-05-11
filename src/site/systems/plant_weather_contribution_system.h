#pragma once

#include "gs1/status.h"
#include "runtime/system_interface.h"
#include "site/systems/site_system_context.h"

namespace gs1
{
class PlantWeatherContributionSystem final : public IRuntimeSystem
{
public:
    [[nodiscard]] const char* name() const noexcept override;
    void register_game_message_subscriptions(RuntimeSystemRegistration& registration) override;
    void register_host_message_subscriptions(RuntimeSystemRegistration& registration) override;
    void register_feedback_event_subscriptions(RuntimeSystemRegistration& registration) override;
    [[nodiscard]] std::optional<Gs1RuntimeProfileSystemId> profile_system_id() const noexcept override;
    [[nodiscard]] std::optional<std::uint32_t> fixed_step_order() const noexcept override;
    [[nodiscard]] Gs1Status process_game_message(
        GameRuntimeTempBridge& bridge,
        const GameMessage& message) override;
    [[nodiscard]] Gs1Status process_host_message(
        GameRuntimeTempBridge& bridge,
        const Gs1HostMessage& message) override;
    void run(GameRuntimeTempBridge& bridge) override;

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

    [[nodiscard]] static bool subscribes_to(GameMessageType type) noexcept;
    [[nodiscard]] static Gs1Status process_message(
        SiteSystemContext<PlantWeatherContributionSystem>& context,
        const GameMessage& message);
    static void run(SiteSystemContext<PlantWeatherContributionSystem>& context);
};
}  // namespace gs1
