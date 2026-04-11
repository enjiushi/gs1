#pragma once

#include "support/id_types.h"

#include <cstdint>
#include <deque>
#include <variant>

namespace gs1
{
enum class GameCommandType : std::uint32_t
{
    OpenMainMenu,
    StartNewCampaign,
    SelectDeploymentSite,
    ClearDeploymentSiteSelection,
    StartSiteAttempt,
    ReturnToRegionalMap,
    MarkSiteCompleted,
    MarkSiteFailed,
    PresentLog
};

struct StartNewCampaignCommand final
{
    std::uint64_t campaign_seed {0};
    std::uint32_t campaign_days {0};
};

struct OpenMainMenuCommand final
{
};

struct SelectDeploymentSiteCommand final
{
    SiteId site_id {};
};

struct ClearDeploymentSiteSelectionCommand final
{
};

struct StartSiteAttemptCommand final
{
    SiteId site_id {};
};

struct ReturnToRegionalMapCommand final
{
};

struct MarkSiteCompletedCommand final
{
    SiteId site_id {};
};

struct MarkSiteFailedCommand final
{
    SiteId site_id {};
};

struct PresentLogCommand final
{
    char text[128] {};
};

using GameCommandPayload = std::variant<
    OpenMainMenuCommand,
    StartNewCampaignCommand,
    SelectDeploymentSiteCommand,
    ClearDeploymentSiteSelectionCommand,
    StartSiteAttemptCommand,
    ReturnToRegionalMapCommand,
    MarkSiteCompletedCommand,
    MarkSiteFailedCommand,
    PresentLogCommand>;

struct GameCommand final
{
    GameCommandType type {};
    GameCommandPayload payload {};
};

using GameCommandQueue = std::deque<GameCommand>;
}  // namespace gs1
