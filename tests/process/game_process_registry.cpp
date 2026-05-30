#include "game_process_registry.h"
#include <string_view>

const GameProcessScenarioDescriptor* find_game_process_scenario(std::string_view name) noexcept
{
    (void)name;
    return nullptr;
}

std::vector<GameProcessScenarioDescriptor> list_game_process_scenarios()
{
    return {};
}
