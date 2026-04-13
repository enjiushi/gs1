#pragma once

#include "commands/game_command.h"

namespace gs1
{
struct CampaignState;
struct SiteRunState;

struct SiteMoveDirectionInput final
{
    float world_move_x {0.0f};
    float world_move_y {0.0f};
    float world_move_z {0.0f};
    bool present {false};
};

struct SiteSystemContext final
{
    const CampaignState& campaign;
    SiteRunState& site_run;
    GameCommandQueue& command_queue;
    double fixed_step_seconds {0.0};
    SiteMoveDirectionInput move_direction {};
};
}  // namespace gs1
