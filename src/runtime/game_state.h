#pragma once

#include "campaign/campaign_state.h"
#include "messages/game_message.h"
#include "runtime/presentation_runtime_state.h"
#include "site/site_run_state.h"
#include "runtime/runtime_clock.h"
#include "runtime/site_protection_presentation_state.h"
#include "runtime/ui_presentation_state.h"
#include "gs1/types.h"

#include <deque>
#include <optional>

namespace gs1
{
struct GameState final
{
    double fixed_step_seconds {k_default_fixed_step_seconds};
    Gs1AppState app_state {GS1_APP_STATE_BOOT};
    std::optional<CampaignState> campaign {};
    std::optional<SiteRunState> active_site_run {};
    SiteProtectionPresentationState site_protection_presentation {};
    UiPresentationState ui_presentation {};
    PresentationRuntimeState presentation_runtime {};
    GameMessageQueue message_queue {};
    std::deque<Gs1RuntimeMessage> runtime_messages {};
};
}  // namespace gs1
