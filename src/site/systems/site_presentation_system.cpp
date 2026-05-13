#include "site/systems/site_presentation_system.h"

#include "site/site_projection_update_flags.h"
#include "site/site_world_access.h"
#include "runtime/game_runtime.h"
#include "support/currency.h"

#include <algorithm>

namespace gs1
{
template <>
struct system_state_tags<SitePresentationSystem>
{
    using type = type_list<
        RuntimeActiveSiteRunTag>;
};

namespace
{
[[nodiscard]] Gs1RuntimeMessage make_engine_message(Gs1EngineMessageType type)
{
    Gs1RuntimeMessage message {};
    message.type = type;
    return message;
}

void queue_runtime_message(RuntimeInvocation& invocation, const Gs1RuntimeMessage& message)
{
    invocation.push_runtime_message(message);
}

enum : std::uint16_t
{
    k_hud_warning_none = 0U,
    k_hud_warning_wind_watch = 1U,
    k_hud_warning_wind_exposure = 2U,
    k_hud_warning_severe_gale = 3U,
    k_hud_warning_sandblast = 4U
};

std::uint16_t resolve_hud_warning_code(const SiteRunState& site_run) noexcept;

void queue_storage_open_request(RuntimeInvocation& invocation, std::uint32_t storage_id)
{
    if (storage_id == 0U)
    {
        return;
    }

    GameMessage open_storage {};
    open_storage.type = GameMessageType::InventoryStorageViewRequest;
    open_storage.set_payload(InventoryStorageViewRequestMessage {
        storage_id,
        GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
        {0U, 0U, 0U}});
    invocation.push_game_message(open_storage);
}

void queue_task_reward_claimed_cue_message(
    RuntimeInvocation& invocation,
    std::uint32_t task_instance_id,
    std::uint32_t task_template_id,
    std::uint32_t reward_candidate_count)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE);
    auto& payload = message.emplace_payload<Gs1EngineMessageOneShotCueData>();
    payload.subject_id = task_instance_id;
    payload.world_x = 0.0f;
    payload.world_y = 0.0f;
    payload.arg0 = task_template_id;
    payload.arg1 = reward_candidate_count;
    payload.cue_kind = GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED;
    queue_runtime_message(invocation, message);
}

void queue_craft_output_stored_cue_message(
    RuntimeInvocation& invocation,
    std::uint32_t storage_id,
    std::uint32_t item_id,
    std::uint32_t quantity)
{
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE);
    auto& payload = message.emplace_payload<Gs1EngineMessageOneShotCueData>();
    payload.subject_id = storage_id;
    payload.world_x = 0.0f;
    payload.world_y = 0.0f;
    payload.arg0 = item_id;
    payload.arg1 = quantity;
    payload.cue_kind = GS1_ONE_SHOT_CUE_CRAFT_OUTPUT_STORED;
    queue_runtime_message(invocation, message);
}

[[nodiscard]] bool action_has_started(const ActionState& action_state) noexcept
{
    return action_state.current_action_id.has_value() &&
        action_state.started_at_world_minute.has_value() &&
        action_state.total_action_minutes > 0.0;
}

[[nodiscard]] float action_progress_normalized(const ActionState& action_state) noexcept
{
    if (!action_has_started(action_state))
    {
        return 0.0f;
    }

    const double elapsed_minutes =
        std::max(0.0, action_state.total_action_minutes - action_state.remaining_action_minutes);
    return static_cast<float>(
        std::clamp(elapsed_minutes / action_state.total_action_minutes, 0.0, 1.0));
}

void queue_site_action_update_message(RuntimeInvocation& invocation, const SiteRunState& site_run)
{
    const auto& action_state = site_run.site_action;
    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE);
    auto& payload = message.emplace_payload<Gs1EngineMessageSiteActionData>();

    if (action_has_started(action_state))
    {
        const TileCoord target_tile = action_state.target_tile.value_or(TileCoord {});
        payload.action_id = action_state.current_action_id->value;
        payload.target_tile_x = target_tile.x;
        payload.target_tile_y = target_tile.y;
        payload.action_kind = static_cast<Gs1SiteActionKind>(action_state.action_kind);
        payload.flags = GS1_SITE_ACTION_PRESENTATION_FLAG_ACTIVE;
        payload.progress_normalized = action_progress_normalized(action_state);
        payload.duration_minutes = static_cast<float>(action_state.total_action_minutes);
    }
    else
    {
        payload.action_id = 0U;
        payload.target_tile_x = 0;
        payload.target_tile_y = 0;
        payload.action_kind = GS1_SITE_ACTION_NONE;
        payload.flags = GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR;
        payload.progress_normalized = 0.0f;
        payload.duration_minutes = 0.0f;
    }

    queue_runtime_message(invocation, message);
}

void queue_site_placement_failure_message(
    RuntimeInvocation& invocation,
    const PlacementModeCommitRejectedMessage& payload)
{
    static std::uint32_t next_sequence_id = 1U;

    auto message = make_engine_message(GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE);
    auto& engine_payload = message.emplace_payload<Gs1EngineMessagePlacementFailureData>();
    engine_payload.tile_x = payload.tile_x;
    engine_payload.tile_y = payload.tile_y;
    engine_payload.blocked_mask = payload.blocked_mask;
    engine_payload.sequence_id = next_sequence_id++;
    engine_payload.action_kind = payload.action_kind;
    engine_payload.flags = 1U;
    engine_payload.reserved0 = 0U;
    queue_runtime_message(invocation, message);
}

void queue_hud_state_message(RuntimeInvocation& invocation, const SiteRunState& site_run)
{
    const auto worker_conditions = site_world_access::worker_conditions(site_run);

    auto hud_message = make_engine_message(GS1_ENGINE_MESSAGE_HUD_STATE);
    auto& payload = hud_message.emplace_payload<Gs1EngineMessageHudStateData>();
    payload.player_health = worker_conditions.health;
    payload.player_hydration = worker_conditions.hydration;
    payload.player_nourishment = worker_conditions.nourishment;
    payload.player_energy = worker_conditions.energy;
    payload.player_morale = worker_conditions.morale;
    payload.current_money = cash_value_from_cash_points(site_run.economy.current_cash);
    payload.active_task_count =
        static_cast<std::uint16_t>(site_run.task_board.accepted_task_ids.size());
    payload.current_action_kind = static_cast<Gs1SiteActionKind>(site_run.site_action.action_kind);
    payload.site_completion_normalized = site_run.counters.objective_progress_normalized;
    payload.warning_code = resolve_hud_warning_code(site_run);
    queue_runtime_message(invocation, hud_message);
}

std::uint16_t resolve_hud_warning_code(const SiteRunState& site_run) noexcept
{
    if (site_run.site_world == nullptr || !site_run.site_world->initialized())
    {
        return k_hud_warning_none;
    }

    const auto worker = site_run.site_world->worker();
    if (!site_world_access::tile_coord_in_bounds(site_run, worker.position.tile_coord))
    {
        return k_hud_warning_none;
    }

    const auto local_weather =
        site_world_access::tile_local_weather(site_run, worker.position.tile_coord);
    const float wind = std::max(local_weather.wind, 0.0f);
    const float dust = std::max(local_weather.dust, 0.0f);

    if (wind >= 65.0f || (wind >= 50.0f && dust >= 18.0f))
    {
        return k_hud_warning_sandblast;
    }

    if (!worker.conditions.is_sheltered && wind >= 45.0f)
    {
        return k_hud_warning_severe_gale;
    }

    if (!worker.conditions.is_sheltered && wind >= 28.0f)
    {
        return k_hud_warning_wind_exposure;
    }

    if (wind >= 18.0f ||
        site_run.weather.weather_wind >= 14.0f ||
        site_run.event.event_wind_pressure >= 4.0f)
    {
        return k_hud_warning_wind_watch;
    }

    return k_hud_warning_none;
}
}  // namespace

const char* SitePresentationSystem::name() const noexcept
{
    return "SitePresentationSystem";
}

void SitePresentationSystem::flush_projection_if_dirty(RuntimeInvocation& invocation)
{
    if (invocation.runtime() != nullptr)
    {
        invocation.runtime()->flush_site_presentation_if_dirty();
        return;
    }

    auto access = make_game_state_access<SitePresentationSystem>(invocation);
    auto& active_site_run = access.template read<RuntimeActiveSiteRunTag>();
    if (!active_site_run.has_value())
    {
        return;
    }

    const auto dirty_flags = active_site_run->pending_projection_update_flags;
    if ((dirty_flags & (SITE_PROJECTION_UPDATE_HUD |
            SITE_PROJECTION_UPDATE_WORKER |
            SITE_PROJECTION_UPDATE_WEATHER)) != 0U)
    {
        queue_hud_state_message(invocation, *active_site_run);
    }
}

GameMessageSubscriptionSpan SitePresentationSystem::subscribed_game_messages() const noexcept
{
    static constexpr GameMessageType subscriptions[] = {
        GameMessageType::TaskRewardClaimResolved,
        GameMessageType::InventoryCraftCompleted,
        GameMessageType::SiteActionStarted,
        GameMessageType::SiteActionCompleted,
        GameMessageType::SiteActionFailed,
        GameMessageType::PlacementModeCommitRejected,
    };
    return subscriptions;
}

HostMessageSubscriptionSpan SitePresentationSystem::subscribed_host_messages() const noexcept
{
    return {};
}

std::optional<Gs1RuntimeProfileSystemId> SitePresentationSystem::profile_system_id() const noexcept
{
    return std::nullopt;
}

std::optional<std::uint32_t> SitePresentationSystem::fixed_step_order() const noexcept
{
    return std::nullopt;
}

Gs1Status SitePresentationSystem::process_host_message(
    RuntimeInvocation& invocation,
    const Gs1HostMessage& message)
{
    (void)invocation;
    (void)message;
    return GS1_STATUS_OK;
}

Gs1Status SitePresentationSystem::process_game_message(
    RuntimeInvocation& invocation,
    const GameMessage& message)
{
    auto access = make_game_state_access<SitePresentationSystem>(invocation);
    auto& active_site_run = access.template read<RuntimeActiveSiteRunTag>();

    switch (message.type)
    {
    case GameMessageType::TaskRewardClaimResolved:
    {
        const auto& payload = message.payload_as<TaskRewardClaimResolvedMessage>();
        flush_projection_if_dirty(invocation);
        queue_task_reward_claimed_cue_message(
            invocation,
            payload.task_instance_id,
            payload.task_template_id,
            payload.reward_candidate_count);
        return GS1_STATUS_OK;
    }

    case GameMessageType::InventoryCraftCompleted:
    {
        const auto& payload = message.payload_as<InventoryCraftCompletedMessage>();
        if (active_site_run.has_value() && payload.output_storage_id != 0U)
        {
            flush_projection_if_dirty(invocation);
            queue_storage_open_request(invocation, payload.output_storage_id);
            queue_craft_output_stored_cue_message(
                invocation,
                payload.output_storage_id,
                payload.output_item_id,
                payload.output_quantity == 0U ? 1U : payload.output_quantity);
        }
        return GS1_STATUS_OK;
    }

    case GameMessageType::SiteActionStarted:
    case GameMessageType::SiteActionCompleted:
    case GameMessageType::SiteActionFailed:
        if (active_site_run.has_value())
        {
            queue_site_action_update_message(invocation, *active_site_run);
        }
        return GS1_STATUS_OK;

    case GameMessageType::PlacementModeCommitRejected:
        queue_site_placement_failure_message(
            invocation,
            message.payload_as<PlacementModeCommitRejectedMessage>());
        return GS1_STATUS_OK;

    default:
        return GS1_STATUS_OK;
    }
}

void SitePresentationSystem::run(RuntimeInvocation& invocation)
{
    flush_projection_if_dirty(invocation);
}
}  // namespace gs1
