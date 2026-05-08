#include "game_process_registry.h"

#include "visual_smoke_silent_onboarding.h"

#include <array>
#include <string_view>

namespace
{
int run_site1_onboarding_scenario(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    SmokeEngineHost::LogMode log_mode,
    const GameProcessTestRunOptions& options)
{
    VisualSmokeSilentOnboardingOptions onboarding_options {};
    onboarding_options.max_frames = options.max_frames;
    onboarding_options.frame_delta_seconds = options.frame_delta_seconds;
    return run_visual_smoke_silent_onboarding(api, runtime, log_mode, onboarding_options);
}

constexpr std::array<GameProcessScenarioDescriptor, 1> k_registered_scenarios {{
    {
        "site1_onboarding",
        "Drive the full Site 1 onboarding chain through semantic player actions without opening the browser viewer.",
        &run_site1_onboarding_scenario,
    },
}};
}  // namespace

const GameProcessScenarioDescriptor* find_game_process_scenario(std::string_view name) noexcept
{
    for (const auto& scenario : k_registered_scenarios)
    {
        if (std::string_view {scenario.name} == name)
        {
            return &scenario;
        }
    }

    return nullptr;
}

std::vector<GameProcessScenarioDescriptor> list_game_process_scenarios()
{
    return {k_registered_scenarios.begin(), k_registered_scenarios.end()};
}
