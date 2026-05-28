#include "runtime/game_runtime.h"

#include <string_view>
#include <vector>

namespace
{
using gs1::GameRuntime;
using gs1::RuntimeSemanticGameMessageScope;

struct GameRuntimeSemanticStackTestAccess
{
#ifndef NDEBUG
    [[nodiscard]] static std::vector<std::string_view> last_semantic_game_message_stack(
        const GameRuntime& runtime)
    {
        const auto stack = runtime.debug_last_semantic_game_message_stack();
        return {stack.begin(), stack.end()};
    }

    [[nodiscard]] static std::vector<std::string_view> current_semantic_game_message_stack(
        const GameRuntime& runtime)
    {
        const auto stack = runtime.debug_semantic_game_message_stack();
        return {stack.begin(), stack.end()};
    }
#endif
};
}  // namespace

namespace
{
int run_debug_semantic_stack_regression()
{
#ifdef NDEBUG
    return 0;
#else
    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = gs1::k_api_version;
    create_desc.fixed_step_seconds = 1.0 / 60.0;
    create_desc.adapter_config_json_utf8 = nullptr;

    GameRuntime runtime {create_desc};

    {
        RuntimeSemanticGameMessageScope outer {runtime, "SelectDeploymentSiteMessage"};
        const auto outer_stack = GameRuntimeSemanticStackTestAccess::current_semantic_game_message_stack(
            runtime);
        if (outer_stack.size() != 1U || outer_stack[0] != "SelectDeploymentSiteMessage")
        {
            return 1;
        }

        RuntimeSemanticGameMessageScope inner {runtime, "DeploymentSiteSelectionChangedMessage"};
        const auto inner_stack = GameRuntimeSemanticStackTestAccess::current_semantic_game_message_stack(
            runtime);
        if (inner_stack.size() != 2U)
        {
            return 2;
        }
        if (inner_stack[0] != "SelectDeploymentSiteMessage" ||
            inner_stack[1] != "DeploymentSiteSelectionChangedMessage")
        {
            return 3;
        }
    }

    if (!GameRuntimeSemanticStackTestAccess::current_semantic_game_message_stack(runtime).empty())
    {
        return 4;
    }

    const auto stack = GameRuntimeSemanticStackTestAccess::last_semantic_game_message_stack(runtime);
    if (stack.size() != 2U)
    {
        return 5;
    }
    if (stack[0] != "SelectDeploymentSiteMessage")
    {
        return 6;
    }
    if (stack[1] != "DeploymentSiteSelectionChangedMessage")
    {
        return 7;
    }

    return 0;
#endif
}
}  // namespace

int main()
{
    return run_debug_semantic_stack_regression();
}
