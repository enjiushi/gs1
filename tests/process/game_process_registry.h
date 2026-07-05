#pragma once

#include "gs1/host/runtime_dll_loader.h"

#include <cstdint>
#include <string_view>
#include <vector>

struct GameProcessTestRunOptions final
{
    std::uint32_t max_frames {18000U};
    double frame_delta_seconds {0.25};
};

struct GameProcessScenarioDescriptor final
{
    const char* name {nullptr};
    const char* description {nullptr};
    int (*run)(
        const gs1::host::RuntimeApi& api,
        gs1::host::RuntimeHostHandle* runtime,
        bool verbose_logging,
        const GameProcessTestRunOptions& options) {nullptr};
};

[[nodiscard]] const GameProcessScenarioDescriptor* find_game_process_scenario(std::string_view name) noexcept;
[[nodiscard]] std::vector<GameProcessScenarioDescriptor> list_game_process_scenarios();
