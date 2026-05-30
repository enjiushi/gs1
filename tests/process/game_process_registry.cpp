#include "game_process_registry.h"

#include "content/defs/task_defs.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
constexpr std::string_view k_site1_onboarding_name = "site1_onboarding";
constexpr const char* k_site1_onboarding_description =
    "Starts a new campaign, enters Site 1, and verifies the active-site onboarding task set loads.";

bool run_phase1(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    double delta_seconds) noexcept
{
    Gs1Phase1Request request {};
    request.struct_size = sizeof(Gs1Phase1Request);
    request.real_delta_seconds = delta_seconds;

    Gs1Phase1Result result {};
    return api.run_phase1(runtime, &request, &result) == GS1_STATUS_OK;
}

bool run_phase2(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime) noexcept
{
    Gs1Phase2Request request {};
    request.struct_size = sizeof(Gs1Phase2Request);

    Gs1Phase2Result result {};
    return api.run_phase2(runtime, &request, &result) == GS1_STATUS_OK;
}

bool get_game_state_view(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    Gs1GameStateView& out_view) noexcept
{
    out_view = {};
    out_view.struct_size = sizeof(Gs1GameStateView);
    return api.get_game_state_view(runtime, &out_view) == GS1_STATUS_OK;
}

bool submit_gameplay_action(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    Gs1GameplayActionType action_type,
    std::uint32_t target_id = 0U,
    std::uint64_t arg0 = 0U,
    std::uint64_t arg1 = 0U) noexcept
{
    Gs1GameplayAction action {};
    action.type = action_type;
    action.target_id = target_id;
    action.arg0 = arg0;
    action.arg1 = arg1;
    return api.submit_gameplay_action(runtime, &action) == GS1_STATUS_OK;
}

bool submit_site_scene_ready(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime) noexcept
{
    return api.submit_site_scene_ready(runtime) == GS1_STATUS_OK;
}

bool site_has_onboarding_tasks(const Gs1SiteStateView& site) noexcept
{
    for (std::uint32_t index = 0; index < site.task_count; ++index)
    {
        const std::uint32_t task_template_id = site.tasks[index].task_template_id;
        if (task_template_id >= gs1::k_task_template_site1_onboarding_plant_checkerboard &&
            task_template_id <= gs1::k_task_template_site1_onboarding_deploy_storage_crate + 0U)
        {
            return true;
        }
        if (task_template_id >= gs1::k_task_template_site1_onboarding_buy_water &&
            task_template_id <= gs1::k_task_template_site1_onboarding_cook_ephedra_stew)
        {
            return true;
        }
    }

    return false;
}

void log_verbose(bool enabled, const char* format, ...) noexcept
{
    if (!enabled)
    {
        return;
    }

    va_list args {};
    va_start(args, format);
    std::vfprintf(stdout, format, args);
    std::fflush(stdout);
    va_end(args);
}

int run_site1_onboarding(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    bool verbose_logging,
    const GameProcessTestRunOptions& options)
{
    std::optional<Gs1AppState> last_app_state {};

    for (std::uint32_t frame = 0U; frame < options.max_frames; ++frame)
    {
        if (!run_phase1(api, runtime, options.frame_delta_seconds))
        {
            std::fprintf(stderr, "[PROCESS] phase1 failed at frame %u\n", frame);
            return 1;
        }

        Gs1GameStateView view {};
        if (!get_game_state_view(api, runtime, view))
        {
            std::fprintf(stderr, "[PROCESS] get_game_state_view failed at frame %u\n", frame);
            return 2;
        }

        if (!last_app_state.has_value() || *last_app_state != view.app_state)
        {
            log_verbose(
                verbose_logging,
                "[PROCESS] frame=%u app_state=%u\n",
                frame,
                static_cast<unsigned>(view.app_state));
            last_app_state = view.app_state;
        }

        switch (view.app_state)
        {
        case GS1_APP_STATE_MAIN_MENU:
            if (!submit_gameplay_action(api, runtime, GS1_GAMEPLAY_ACTION_START_NEW_CAMPAIGN, 0U, 1U, 0U))
            {
                std::fprintf(stderr, "[PROCESS] failed to submit start-new-campaign at frame %u\n", frame);
                return 3;
            }
            break;

        case GS1_APP_STATE_REGIONAL_MAP:
            if (view.campaign == nullptr)
            {
                std::fprintf(stderr, "[PROCESS] missing campaign view in regional map at frame %u\n", frame);
                return 4;
            }

            if (view.campaign->selected_site_id != 1)
            {
                if (!submit_gameplay_action(api, runtime, GS1_GAMEPLAY_ACTION_SELECT_DEPLOYMENT_SITE, 1U))
                {
                    std::fprintf(stderr, "[PROCESS] failed to select Site 1 at frame %u\n", frame);
                    return 5;
                }
            }
            else
            {
                if (!submit_gameplay_action(api, runtime, GS1_GAMEPLAY_ACTION_START_SITE_ATTEMPT, 1U))
                {
                    std::fprintf(stderr, "[PROCESS] failed to start Site 1 attempt at frame %u\n", frame);
                    return 6;
                }
            }
            break;

        case GS1_APP_STATE_SITE_LOADING:
            if (!submit_site_scene_ready(api, runtime))
            {
                std::fprintf(stderr, "[PROCESS] failed to acknowledge site-scene ready at frame %u\n", frame);
                return 7;
            }
            break;

        case GS1_APP_STATE_SITE_ACTIVE:
            if (view.has_active_site == 0U || view.active_site == nullptr)
            {
                std::fprintf(stderr, "[PROCESS] app reported SITE_ACTIVE without an active site at frame %u\n", frame);
                return 8;
            }

            if (view.active_site->site_id != 1U)
            {
                std::fprintf(stderr, "[PROCESS] expected Site 1 but saw site_id=%u at frame %u\n", view.active_site->site_id, frame);
                return 9;
            }

            if (!site_has_onboarding_tasks(*view.active_site))
            {
                std::fprintf(stderr, "[PROCESS] Site 1 became active without onboarding tasks at frame %u\n", frame);
                return 10;
            }

            log_verbose(verbose_logging, "[PROCESS] frame=%u Site 1 onboarding tasks are present\n", frame);
            return 0;

        case GS1_APP_STATE_SITE_RESULT:
        case GS1_APP_STATE_CAMPAIGN_END:
            std::fprintf(
                stderr,
                "[PROCESS] unexpected terminal app_state=%u before onboarding loaded\n",
                static_cast<unsigned>(view.app_state));
            return 11;

        default:
            break;
        }

        if (!run_phase2(api, runtime))
        {
            std::fprintf(stderr, "[PROCESS] phase2 failed at frame %u\n", frame);
            return 12;
        }
    }

    std::fprintf(
        stderr,
        "[PROCESS] timed out after %u frames waiting for Site 1 onboarding bootstrap\n",
        options.max_frames);
    return 13;
}

constexpr std::array<GameProcessScenarioDescriptor, 1U> k_registered_scenarios {{
    GameProcessScenarioDescriptor {
        k_site1_onboarding_name.data(),
        k_site1_onboarding_description,
        &run_site1_onboarding}
}};
}  // namespace

const GameProcessScenarioDescriptor* find_game_process_scenario(std::string_view name) noexcept
{
    const auto found = std::find_if(
        k_registered_scenarios.begin(),
        k_registered_scenarios.end(),
        [name](const GameProcessScenarioDescriptor& descriptor) {
            return descriptor.name != nullptr && name == descriptor.name;
        });
    return found != k_registered_scenarios.end() ? &(*found) : nullptr;
}

std::vector<GameProcessScenarioDescriptor> list_game_process_scenarios()
{
    return {k_registered_scenarios.begin(), k_registered_scenarios.end()};
}
