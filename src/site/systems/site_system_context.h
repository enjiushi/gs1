#pragma once

#include "commands/game_command.h"
#include "site/site_world.h"

namespace gs1
{
struct CampaignState;

struct SiteMoveDirectionInput final
{
    float world_move_x {0.0f};
    float world_move_y {0.0f};
    float world_move_z {0.0f};
    bool present {false};
};

template <typename SystemTag>
struct SiteSystemContext final
{
    const CampaignState& campaign;
    SiteWorldAccess<SystemTag> world;
    GameCommandQueue& command_queue;
    double fixed_step_seconds {0.0};
    SiteMoveDirectionInput move_direction {};
};

template <typename SystemTag>
[[nodiscard]] inline SiteSystemContext<SystemTag> make_site_system_context(
    const CampaignState& campaign,
    SiteRunState& site_run,
    GameCommandQueue& command_queue,
    double fixed_step_seconds,
    SiteMoveDirectionInput move_direction)
{
    return SiteSystemContext<SystemTag> {
        campaign,
        SiteWorldAccess<SystemTag> {site_run},
        command_queue,
        fixed_step_seconds,
        move_direction};
}
}  // namespace gs1
