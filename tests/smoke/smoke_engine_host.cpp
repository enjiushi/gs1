#include "smoke_engine_host.h"
#include "smoke_log.h"

#include "content/defs/plant_defs.h"
#include "site/weather_contribution_logic.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <utility>

namespace
{
double elapsed_seconds(
    const std::chrono::steady_clock::time_point start,
    const std::chrono::steady_clock::time_point end) noexcept
{
    return std::chrono::duration<double>(end - start).count();
}

float resolve_final_overlay_protection_value(
    std::uint32_t plant_type_id,
    float plant_density,
    float projected_protection,
    float gs1::PlantDef::*self_channel) noexcept
{
    const auto* plant_def = gs1::find_plant_def(gs1::PlantId {plant_type_id});
    if (plant_def == nullptr)
    {
        return std::clamp(projected_protection, 0.0f, 100.0f);
    }

    const float self_resistance =
        gs1::resolve_density_scaled_resistance(plant_def->*self_channel, plant_density);
    return std::clamp(projected_protection + self_resistance, 0.0f, 100.0f);
}

const char* message_type_name(Gs1EngineMessageType type)
{
    switch (type)
    {
    case GS1_ENGINE_MESSAGE_LOG_TEXT:
        return "LOG_TEXT";
    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        return "SET_APP_STATE";
    case GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY:
        return "PRESENTATION_DIRTY";
    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
        return "BEGIN_REGIONAL_MAP_SNAPSHOT";
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
        return "REGIONAL_MAP_SITE_UPSERT";
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
        return "REGIONAL_MAP_SITE_REMOVE";
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
        return "REGIONAL_MAP_LINK_UPSERT";
    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
        return "REGIONAL_MAP_LINK_REMOVE";
    case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
        return "END_REGIONAL_MAP_SNAPSHOT";
    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
        return "BEGIN_UI_SETUP";
    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
        return "UI_ELEMENT_UPSERT";
    case GS1_ENGINE_MESSAGE_END_UI_SETUP:
        return "END_UI_SETUP";
    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
        return "CLOSE_UI_SETUP";
    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
        return "BEGIN_SITE_SNAPSHOT";
    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
        return "SITE_TILE_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
        return "SITE_WORKER_UPDATE";
    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
        return "SITE_CAMP_UPDATE";
    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
        return "SITE_WEATHER_UPDATE";
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
        return "SITE_INVENTORY_STORAGE_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
        return "SITE_INVENTORY_SLOT_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
        return "SITE_INVENTORY_VIEW_STATE";
    case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
        return "SITE_TASK_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_TASK_REMOVE:
        return "SITE_TASK_REMOVE";
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
        return "SITE_PHONE_LISTING_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
        return "SITE_PHONE_LISTING_REMOVE";
    case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
        return "END_SITE_SNAPSHOT";
    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
        return "SITE_ACTION_UPDATE";
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
        return "SITE_CRAFT_CONTEXT_BEGIN";
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
        return "SITE_CRAFT_CONTEXT_OPTION_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
        return "SITE_CRAFT_CONTEXT_END";
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
        return "SITE_PLACEMENT_PREVIEW";
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT:
        return "SITE_PLACEMENT_PREVIEW_TILE_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
        return "SITE_PLACEMENT_FAILURE";
    case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
        return "SITE_PHONE_PANEL_STATE";
    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
        return "SITE_PROTECTION_OVERLAY_STATE";
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
        return "SITE_MODIFIER_LIST_BEGIN";
    case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
        return "SITE_MODIFIER_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT:
        return "SITE_PLANT_VISUAL_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_REMOVE:
        return "SITE_PLANT_VISUAL_REMOVE";
    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT:
        return "SITE_DEVICE_VISUAL_UPSERT";
    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_REMOVE:
        return "SITE_DEVICE_VISUAL_REMOVE";
    case GS1_ENGINE_MESSAGE_HUD_STATE:
        return "HUD_STATE";
    case GS1_ENGINE_MESSAGE_NOTIFICATION_PUSH:
        return "NOTIFICATION_PUSH";
    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
        return "SITE_RESULT_READY";
    case GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE:
        return "PLAY_ONE_SHOT_CUE";
    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
        return "CAMPAIGN_RESOURCES";
    default:
        return "NONE";
    }
}

const char* app_state_name(Gs1AppState app_state)
{
    switch (app_state)
    {
    case GS1_APP_STATE_BOOT:
        return "BOOT";
    case GS1_APP_STATE_MAIN_MENU:
        return "MAIN_MENU";
    case GS1_APP_STATE_CAMPAIGN_SETUP:
        return "CAMPAIGN_SETUP";
    case GS1_APP_STATE_REGIONAL_MAP:
        return "REGIONAL_MAP";
    case GS1_APP_STATE_SITE_LOADING:
        return "SITE_LOADING";
    case GS1_APP_STATE_SITE_ACTIVE:
        return "SITE_ACTIVE";
    case GS1_APP_STATE_SITE_PAUSED:
        return "SITE_PAUSED";
    case GS1_APP_STATE_SITE_RESULT:
        return "SITE_RESULT";
    case GS1_APP_STATE_CAMPAIGN_END:
        return "CAMPAIGN_END";
    default:
        return "NONE";
    }
}

const char* ui_setup_name(Gs1UiSetupId setup_id)
{
    switch (setup_id)
    {
    case GS1_UI_SETUP_MAIN_MENU:
        return "MAIN_MENU";
    case GS1_UI_SETUP_REGIONAL_MAP_SELECTION:
        return "REGIONAL_MAP_SELECTION";
    case GS1_UI_SETUP_SITE_RESULT:
        return "SITE_RESULT";
    case GS1_UI_SETUP_REGIONAL_MAP_MENU:
        return "REGIONAL_MAP_MENU";
    case GS1_UI_SETUP_REGIONAL_MAP_TECH_TREE:
        return "REGIONAL_MAP_TECH_TREE";
    case GS1_UI_SETUP_SITE_PROTECTION_SELECTOR:
        return "SITE_PROTECTION_SELECTOR";
    default:
        return "NONE";
    }
}

const char* ui_setup_presentation_type_name(Gs1UiSetupPresentationType presentation_type)
{
    switch (presentation_type)
    {
    case GS1_UI_SETUP_PRESENTATION_NORMAL:
        return "NORMAL";
    case GS1_UI_SETUP_PRESENTATION_OVERLAY:
        return "OVERLAY";
    case GS1_UI_SETUP_PRESENTATION_NONE:
    default:
        return "NONE";
    }
}

const char* ui_action_name(Gs1UiActionType action_type)
{
    switch (action_type)
    {
    case GS1_UI_ACTION_START_NEW_CAMPAIGN:
        return "START_NEW_CAMPAIGN";
    case GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE:
        return "SELECT_DEPLOYMENT_SITE";
    case GS1_UI_ACTION_START_SITE_ATTEMPT:
        return "START_SITE_ATTEMPT";
    case GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP:
        return "RETURN_TO_REGIONAL_MAP";
    case GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION:
        return "CLEAR_DEPLOYMENT_SITE_SELECTION";
    case GS1_UI_ACTION_ACCEPT_TASK:
        return "ACCEPT_TASK";
    case GS1_UI_ACTION_CLAIM_TASK_REWARD:
        return "CLAIM_TASK_REWARD";
    case GS1_UI_ACTION_BUY_PHONE_LISTING:
        return "BUY_PHONE_LISTING";
    case GS1_UI_ACTION_SELL_PHONE_LISTING:
        return "SELL_PHONE_LISTING";
    case GS1_UI_ACTION_ADD_PHONE_LISTING_TO_CART:
        return "ADD_PHONE_LISTING_TO_CART";
    case GS1_UI_ACTION_REMOVE_PHONE_LISTING_FROM_CART:
        return "REMOVE_PHONE_LISTING_FROM_CART";
    case GS1_UI_ACTION_CHECKOUT_PHONE_CART:
        return "CHECKOUT_PHONE_CART";
    case GS1_UI_ACTION_SET_PHONE_PANEL_SECTION:
        return "SET_PHONE_PANEL_SECTION";
    case GS1_UI_ACTION_CLOSE_PHONE_PANEL:
        return "CLOSE_PHONE_PANEL";
    case GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR:
        return "OPEN_SITE_PROTECTION_SELECTOR";
    case GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI:
        return "CLOSE_SITE_PROTECTION_UI";
    case GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE:
        return "SET_SITE_PROTECTION_OVERLAY_MODE";
    case GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE:
        return "OPEN_REGIONAL_MAP_TECH_TREE";
    case GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE:
        return "CLOSE_REGIONAL_MAP_TECH_TREE";
    case GS1_UI_ACTION_CLAIM_TECHNOLOGY_NODE:
        return "CLAIM_TECHNOLOGY_NODE";
    case GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB:
        return "SELECT_TECH_TREE_FACTION_TAB";
    case GS1_UI_ACTION_HIRE_CONTRACTOR:
        return "HIRE_CONTRACTOR";
    case GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE:
        return "PURCHASE_SITE_UNLOCKABLE";
    case GS1_UI_ACTION_END_SITE_MODIFIER:
        return "END_SITE_MODIFIER";
    default:
        return "NONE";
    }
}

const char* site_protection_overlay_mode_name(Gs1SiteProtectionOverlayMode mode)
{
    switch (mode)
    {
    case GS1_SITE_PROTECTION_OVERLAY_WIND:
        return "WIND";
    case GS1_SITE_PROTECTION_OVERLAY_HEAT:
        return "HEAT";
    case GS1_SITE_PROTECTION_OVERLAY_DUST:
        return "DUST";
    case GS1_SITE_PROTECTION_OVERLAY_OCCUPANT_CONDITION:
        return "OCCUPANT_CONDITION";
    case GS1_SITE_PROTECTION_OVERLAY_NONE:
    default:
        return "NONE";
    }
}

const char* projection_mode_name(Gs1ProjectionMode mode)
{
    switch (mode)
    {
    case GS1_PROJECTION_MODE_SNAPSHOT:
        return "SNAPSHOT";
    case GS1_PROJECTION_MODE_DELTA:
        return "DELTA";
    default:
        return "UNKNOWN";
    }
}

const char* site_state_name(Gs1SiteState site_state)
{
    switch (site_state)
    {
    case GS1_SITE_STATE_LOCKED:
        return "LOCKED";
    case GS1_SITE_STATE_AVAILABLE:
        return "AVAILABLE";
    case GS1_SITE_STATE_COMPLETED:
        return "COMPLETED";
    default:
        return "UNKNOWN";
    }
}

const char* site_attempt_result_name(Gs1SiteAttemptResult result)
{
    switch (result)
    {
    case GS1_SITE_ATTEMPT_RESULT_COMPLETED:
        return "COMPLETED";
    case GS1_SITE_ATTEMPT_RESULT_FAILED:
        return "FAILED";
    case GS1_SITE_ATTEMPT_RESULT_NONE:
    default:
        return "NONE";
    }
}

const char* one_shot_cue_name(Gs1OneShotCueKind cue_kind)
{
    switch (cue_kind)
    {
    case GS1_ONE_SHOT_CUE_ACTION_COMPLETED:
        return "ACTION_COMPLETED";
    case GS1_ONE_SHOT_CUE_ACTION_FAILED:
        return "ACTION_FAILED";
    case GS1_ONE_SHOT_CUE_HAZARD_PEAK:
        return "HAZARD_PEAK";
    case GS1_ONE_SHOT_CUE_SITE_COMPLETED:
        return "SITE_COMPLETED";
    case GS1_ONE_SHOT_CUE_SITE_FAILED:
        return "SITE_FAILED";
    case GS1_ONE_SHOT_CUE_TASK_REWARD_CLAIMED:
        return "TASK_REWARD_CLAIMED";
    case GS1_ONE_SHOT_CUE_CAMPAIGN_UNLOCKED:
        return "CAMPAIGN_UNLOCKED";
    case GS1_ONE_SHOT_CUE_CRAFT_OUTPUT_STORED:
        return "CRAFT_OUTPUT_STORED";
    case GS1_ONE_SHOT_CUE_NONE:
    default:
        return "NONE";
    }
}

bool ui_action_matches(const Gs1UiAction& requested, const Gs1UiAction& available)
{
    if (requested.type != available.type)
    {
        return false;
    }

    if (requested.target_id != 0U && requested.target_id != available.target_id)
    {
        return false;
    }

    if (requested.arg0 != 0ULL && requested.arg0 != available.arg0)
    {
        return false;
    }

    if (requested.arg1 != 0ULL && requested.arg1 != available.arg1)
    {
        return false;
    }

    return true;
}

bool should_log_engine_message_to_sink(
    SmokeEngineHost::LogMode log_mode,
    const Gs1RuntimeMessage& message) noexcept
{
    if (log_mode == SmokeEngineHost::LogMode::Verbose)
    {
        return true;
    }

    if (message.type != GS1_ENGINE_MESSAGE_LOG_TEXT)
    {
        return false;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageLogTextData>();
    return payload.level != GS1_LOG_LEVEL_DEBUG;
}
}  // namespace

SmokeEngineHost::SmokeEngineHost(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    LogMode log_mode) noexcept
    : api_(&api)
    , runtime_(runtime)
    , log_mode_(log_mode)
{
    publish_live_state_snapshot();
    smoke_log::infof("[ENGINE][BOOT] smoke host ready\n");
}

void SmokeEngineHost::queue_feedback_event(const Gs1FeedbackEvent& event)
{
    std::scoped_lock lock {incoming_commands_mutex_};
    incoming_feedback_events_.push_back(event);
}

void SmokeEngineHost::drain_incoming_commands()
{
    std::scoped_lock lock {incoming_commands_mutex_};

    if (!incoming_pre_phase1_host_events_.empty())
    {
        frame_pre_phase1_host_events_.insert(
            frame_pre_phase1_host_events_.end(),
            incoming_pre_phase1_host_events_.begin(),
            incoming_pre_phase1_host_events_.end());
        incoming_pre_phase1_host_events_.clear();
    }

    if (!incoming_feedback_events_.empty())
    {
        frame_feedback_events_.insert(
            frame_feedback_events_.end(),
            incoming_feedback_events_.begin(),
            incoming_feedback_events_.end());
        incoming_feedback_events_.clear();
    }
}

void SmokeEngineHost::update(double delta_seconds)
{
    const auto update_start = std::chrono::steady_clock::now();

    frame_number_ += 1U;
    current_frame_gameplay_dll_seconds_ = 0.0;
    current_frame_message_entries_.clear();
    frame_live_state_patch_mask_ = LiveStatePatchField_None;

    drain_incoming_commands();

    apply_inflight_script_directive();
    submit_host_messages(frame_pre_phase1_host_events_, "pre_phase1");

    Gs1Phase1Request phase1_request {};
    phase1_request.struct_size = sizeof(Gs1Phase1Request);
    phase1_request.real_delta_seconds = delta_seconds;

    Gs1Phase1Result phase1_result {};
    const auto phase1_start = std::chrono::steady_clock::now();
    const auto phase1_status = api_->run_phase1(runtime_, &phase1_request, &phase1_result);
    current_frame_gameplay_dll_seconds_ += elapsed_seconds(phase1_start, std::chrono::steady_clock::now());
    assert(phase1_status == GS1_STATUS_OK);
    const bool log_phase1_summary = log_mode_ == LogMode::Verbose;
    if (log_phase1_summary)
    {
        smoke_log::infof("[ENGINE][FRAME %llu] phase1 status=%u host_events=%u fixed_steps=%u queued=%u\n",
            static_cast<unsigned long long>(frame_number_),
            static_cast<unsigned>(phase1_status),
            static_cast<unsigned>(phase1_result.processed_host_message_count),
            static_cast<unsigned>(phase1_result.fixed_steps_executed),
            static_cast<unsigned>(phase1_result.runtime_messages_queued));
    }
    phase1_fixed_steps_executed_ = phase1_result.fixed_steps_executed;
    phase1_processed_host_message_count_ = phase1_result.processed_host_message_count;

    flush_engine_messages("phase1");

    queue_between_phase_site_scene_ready_if_needed();
    queue_between_phase_ui_action_if_ready();
    submit_host_messages(pending_between_phase_host_events_, "between_phases");

    if (!frame_feedback_events_.empty())
    {
        const auto feedback_submit_start = std::chrono::steady_clock::now();
        const auto status = api_->submit_feedback_events(
            runtime_,
            frame_feedback_events_.data(),
            static_cast<std::uint32_t>(frame_feedback_events_.size()));
        current_frame_gameplay_dll_seconds_ +=
            elapsed_seconds(feedback_submit_start, std::chrono::steady_clock::now());
        assert(status == GS1_STATUS_OK);
        if (log_mode_ == LogMode::Verbose)
        {
            smoke_log::infof("[ENGINE][FRAME %llu] submit_feedback_events status=%u count=%u\n",
                static_cast<unsigned long long>(frame_number_),
                static_cast<unsigned>(status),
                static_cast<unsigned>(frame_feedback_events_.size()));
        }
        frame_feedback_events_.clear();
    }

    Gs1Phase2Request phase2_request {};
    phase2_request.struct_size = sizeof(Gs1Phase2Request);

    Gs1Phase2Result phase2_result {};
    const auto phase2_start = std::chrono::steady_clock::now();
    const auto phase2_status = api_->run_phase2(runtime_, &phase2_request, &phase2_result);
    current_frame_gameplay_dll_seconds_ += elapsed_seconds(phase2_start, std::chrono::steady_clock::now());
    assert(phase2_status == GS1_STATUS_OK);
    const bool log_phase2_summary = log_mode_ == LogMode::Verbose;
    if (log_phase2_summary)
    {
        smoke_log::infof("[ENGINE][FRAME %llu] phase2 status=%u host_events=%u feedback_events=%u queued=%u\n",
            static_cast<unsigned long long>(frame_number_),
            static_cast<unsigned>(phase2_status),
            static_cast<unsigned>(phase2_result.processed_host_message_count),
            static_cast<unsigned>(phase2_result.processed_feedback_event_count),
            static_cast<unsigned>(phase2_result.runtime_messages_queued));
    }
    phase2_processed_host_message_count_ = phase2_result.processed_host_message_count;

    flush_engine_messages("phase2");
    resolve_inflight_script_directive();
    publish_live_state_snapshot();

    const auto update_end = std::chrono::steady_clock::now();
    last_frame_timing_.total_update_seconds = elapsed_seconds(update_start, update_end);
    last_frame_timing_.gameplay_dll_seconds = current_frame_gameplay_dll_seconds_;
    last_frame_timing_.host_update_seconds = std::max(
        0.0,
        last_frame_timing_.total_update_seconds - last_frame_timing_.gameplay_dll_seconds);
}

void SmokeEngineHost::queue_ui_action(const Gs1UiAction& action)
{
    std::scoped_lock lock {incoming_commands_mutex_};
    incoming_pre_phase1_host_events_.push_back(make_ui_action_event(action));
}

void SmokeEngineHost::queue_site_action_request(const Gs1HostEventSiteActionRequestData& action)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_REQUEST;
    event.payload.site_action_request = action;
    std::scoped_lock lock {incoming_commands_mutex_};
    incoming_pre_phase1_host_events_.push_back(event);
}

void SmokeEngineHost::queue_site_action_cancel(const Gs1HostEventSiteActionCancelData& action)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_ACTION_CANCEL;
    event.payload.site_action_cancel = action;
    std::scoped_lock lock {incoming_commands_mutex_};
    incoming_pre_phase1_host_events_.push_back(event);
}

void SmokeEngineHost::queue_site_storage_view(const Gs1HostEventSiteStorageViewData& request)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_STORAGE_VIEW;
    event.payload.site_storage_view = request;
    std::scoped_lock lock {incoming_commands_mutex_};
    incoming_pre_phase1_host_events_.push_back(event);
}

void SmokeEngineHost::queue_site_inventory_slot_tap(const Gs1HostEventSiteInventorySlotTapData& request)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_INVENTORY_SLOT_TAP;
    event.payload.site_inventory_slot_tap = request;
    std::scoped_lock lock {incoming_commands_mutex_};
    incoming_pre_phase1_host_events_.push_back(event);
}

void SmokeEngineHost::queue_site_context_request(const Gs1HostEventSiteContextRequestData& request)
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_CONTEXT_REQUEST;
    event.payload.site_context_request = request;
    std::scoped_lock lock {incoming_commands_mutex_};
    incoming_pre_phase1_host_events_.push_back(event);
}

void SmokeEngineHost::queue_site_move_direction(float world_move_x, float world_move_y, float world_move_z)
{
    std::scoped_lock lock {incoming_commands_mutex_};
    const auto duplicate = std::find_if(
        incoming_pre_phase1_host_events_.begin(),
        incoming_pre_phase1_host_events_.end(),
        [](const Gs1HostMessage& event) {
            return event.type == GS1_HOST_EVENT_SITE_MOVE_DIRECTION;
        });
    if (duplicate != incoming_pre_phase1_host_events_.end())
    {
        *duplicate = make_site_move_direction_event(world_move_x, world_move_y, world_move_z);
        return;
    }

    incoming_pre_phase1_host_events_.push_back(make_site_move_direction_event(
        world_move_x,
        world_move_y,
        world_move_z));
}

std::vector<std::string> SmokeEngineHost::consume_pending_live_state_patches()
{
    std::scoped_lock lock {published_state_mutex_};
    auto patches = std::move(published_live_state_patches_);
    published_live_state_patches_.clear();
    return patches;
}

SmokeEngineHost::LiveStateSnapshot SmokeEngineHost::capture_live_state_snapshot() const
{
    std::scoped_lock lock {published_state_mutex_};
    return published_live_state_snapshot_;
}

SmokeEngineHost::LiveStateSnapshot SmokeEngineHost::capture_frame_live_state_snapshot() const
{
    LiveStateSnapshot snapshot {};
    snapshot.frame_number = frame_number_;
    snapshot.current_app_state = current_app_state_;
    snapshot.selected_site_id = selected_site_id_;
    snapshot.script_failed = script_failed_;
    snapshot.current_frame_message_entries = current_frame_message_entries_;

    const std::size_t log_start = message_logs_.size() > 40U ? (message_logs_.size() - 40U) : 0U;
    snapshot.message_log_tail.reserve(message_logs_.size() - log_start);
    for (std::size_t index = log_start; index < message_logs_.size(); ++index)
    {
        snapshot.message_log_tail.push_back(message_logs_[index]);
    }

    snapshot.active_ui_setups = snapshot_active_ui_setups();
    snapshot.active_progression_views = snapshot_active_progression_views();
    snapshot.regional_map_sites = snapshot_regional_map_sites();
    snapshot.regional_map_links = snapshot_regional_map_links();
    snapshot.active_site_snapshot = active_site_snapshot_;
    snapshot.campaign_resources = campaign_resources_;
    snapshot.hud_state = hud_state_;
    snapshot.site_action = site_action_;
    snapshot.site_result = site_result_;
    snapshot.recent_one_shot_cues = recent_one_shot_cues_;
    return snapshot;
}

void SmokeEngineHost::publish_live_state_snapshot()
{
    LiveStateSnapshot snapshot = capture_frame_live_state_snapshot();
    std::scoped_lock lock {published_state_mutex_};
    published_live_state_snapshot_ = std::move(snapshot);
    if (frame_live_state_patch_mask_ != LiveStatePatchField_None)
    {
        const auto patch = build_live_state_patch_json(
            published_live_state_snapshot_,
            frame_live_state_patch_mask_);
        if (!patch.empty())
        {
            published_live_state_patches_.push_back(patch);
        }
        frame_live_state_patch_mask_ = LiveStatePatchField_None;
    }
}

bool SmokeEngineHost::set_inflight_script_directive(SmokeScriptDirective directive)
{
    if (inflight_script_directive_.has_value())
    {
        return false;
    }

    inflight_script_directive_ = std::move(directive);
    inflight_script_directive_started_ = false;

    smoke_log::infof("[SCRIPT][FRAME %llu] line %zu: %s\n",
        static_cast<unsigned long long>(frame_number_ + 1U),
        inflight_script_directive_->line_number,
        inflight_script_directive_->source_text.c_str());
    return true;
}

bool SmokeEngineHost::saw_message(Gs1EngineMessageType type) const noexcept
{
    for (const auto seen_type : seen_messages_)
    {
        if (seen_type == type)
        {
            return true;
        }
    }

    return false;
}

void SmokeEngineHost::queue_pre_phase1_ui_action_if_ready()
{
    (void)try_queue_ui_action_from_directive(frame_pre_phase1_host_events_);
}

void SmokeEngineHost::queue_between_phase_ui_action_if_ready()
{
    (void)try_queue_ui_action_from_directive(pending_between_phase_host_events_);
}

void SmokeEngineHost::queue_between_phase_site_scene_ready_if_needed()
{
    if (!pending_site_scene_ready_ack_)
    {
        return;
    }

    pending_between_phase_host_events_.push_back(make_site_scene_ready_event());
    pending_site_scene_ready_ack_ = false;
}

void SmokeEngineHost::submit_host_messages(
    std::vector<Gs1HostMessage>& events,
    const char* stage_label)
{
    if (events.empty())
    {
        return;
    }

    const auto submit_start = std::chrono::steady_clock::now();
    const auto status = api_->submit_host_messages(
        runtime_,
        events.data(),
        static_cast<std::uint32_t>(events.size()));
    current_frame_gameplay_dll_seconds_ += elapsed_seconds(submit_start, std::chrono::steady_clock::now());
    assert(status == GS1_STATUS_OK);
    if (log_mode_ == LogMode::Verbose)
    {
        smoke_log::infof("[ENGINE][FRAME %llu] submit_host_messages stage=%s status=%u count=%u\n",
            static_cast<unsigned long long>(frame_number_),
            stage_label,
            static_cast<unsigned>(status),
            static_cast<unsigned>(events.size()));
    }
    events.clear();
}

void SmokeEngineHost::apply_inflight_script_directive()
{
    queue_pre_phase1_ui_action_if_ready();
}

void SmokeEngineHost::resolve_inflight_script_directive()
{
    if (!inflight_script_directive_.has_value())
    {
        return;
    }

    auto& directive = inflight_script_directive_.value();

    switch (directive.opcode)
    {
    case SmokeScriptOpcode::WaitFrames:
        if (directive.remaining_frames <= 1U)
        {
            clear_inflight_script_directive();
        }
        else
        {
            directive.remaining_frames -= 1U;
        }
        return;

    case SmokeScriptOpcode::ClickUiAction:
        if (inflight_script_directive_started_)
        {
            clear_inflight_script_directive();
        }
        return;

    case SmokeScriptOpcode::WaitAppState:
        if (current_app_state_ == directive.app_state)
        {
            clear_inflight_script_directive();
            return;
        }
        if (directive.remaining_frames <= 1U)
        {
            fail_inflight_script_directive("timed out waiting for app state");
            return;
        }
        directive.remaining_frames -= 1U;
        return;

    case SmokeScriptOpcode::WaitSelectedSite:
        if (selected_site_id_ == directive.site_id)
        {
            clear_inflight_script_directive();
            return;
        }
        if (directive.remaining_frames <= 1U)
        {
            fail_inflight_script_directive("timed out waiting for selected site");
            return;
        }
        directive.remaining_frames -= 1U;
        return;

    case SmokeScriptOpcode::AssertAppState:
        if (current_app_state_ != directive.app_state)
        {
            fail_inflight_script_directive("app state assertion failed");
            return;
        }
        clear_inflight_script_directive();
        return;

    case SmokeScriptOpcode::AssertSelectedSite:
        if (selected_site_id_ != directive.site_id)
        {
            fail_inflight_script_directive("selected site assertion failed");
            return;
        }
        clear_inflight_script_directive();
        return;

    case SmokeScriptOpcode::AssertSawMessage:
        if (!saw_message(directive.engine_message_type))
        {
            fail_inflight_script_directive("engine message was not observed");
            return;
        }
        clear_inflight_script_directive();
        return;

    case SmokeScriptOpcode::AssertLogContains:
        for (const auto& entry : message_logs_)
        {
            if (entry.find(directive.text) != std::string::npos)
            {
                clear_inflight_script_directive();
                return;
            }
        }
        fail_inflight_script_directive("log text was not found: " + directive.text);
        return;
    }
}

void SmokeEngineHost::clear_inflight_script_directive()
{
    inflight_script_directive_.reset();
    inflight_script_directive_started_ = false;
}

void SmokeEngineHost::fail_inflight_script_directive(const std::string& message)
{
    if (!inflight_script_directive_.has_value())
    {
        return;
    }

    smoke_log::errorf(
        "%s:%zu: %s\n",
        inflight_script_directive_->script_path.c_str(),
        inflight_script_directive_->line_number,
        message.c_str());

    script_failed_ = true;
    clear_inflight_script_directive();
}

void SmokeEngineHost::flush_engine_messages(const char* stage_label)
{
    Gs1RuntimeMessage message {};

    while (true)
    {
        const auto pop_start = std::chrono::steady_clock::now();
        const auto pop_status = api_->pop_runtime_message(runtime_, &message);
        current_frame_gameplay_dll_seconds_ += elapsed_seconds(pop_start, std::chrono::steady_clock::now());
        if (pop_status != GS1_STATUS_OK)
        {
            break;
        }

        seen_messages_.push_back(message.type);
        std::uint32_t live_state_patch_mask = LiveStatePatchField_None;

        switch (message.type)
        {
        case GS1_ENGINE_MESSAGE_SET_APP_STATE:
        {
            const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
            current_app_state_ = payload.app_state;
            if (current_app_state_ == GS1_APP_STATE_MAIN_MENU ||
                current_app_state_ == GS1_APP_STATE_REGIONAL_MAP ||
                current_app_state_ == GS1_APP_STATE_CAMPAIGN_SETUP ||
                current_app_state_ == GS1_APP_STATE_CAMPAIGN_END)
            {
                pending_site_scene_ready_ack_ = false;
                active_site_snapshot_.reset();
                hud_state_.reset();
                site_action_.reset();
                site_result_.reset();
            }
            else if (current_app_state_ == GS1_APP_STATE_SITE_ACTIVE ||
                current_app_state_ == GS1_APP_STATE_SITE_LOADING ||
                current_app_state_ == GS1_APP_STATE_SITE_PAUSED)
            {
                pending_site_scene_ready_ack_ = current_app_state_ == GS1_APP_STATE_SITE_LOADING;
                site_result_.reset();
            }
            live_state_patch_mask =
                LiveStatePatchField_AppState |
                LiveStatePatchField_SiteBootstrap |
                LiveStatePatchField_SiteState |
                LiveStatePatchField_Hud |
                LiveStatePatchField_SiteAction |
                LiveStatePatchField_SiteResult;
            break;
        }

        case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
            apply_regional_map_snapshot_begin(message);
            break;
        case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
            apply_regional_map_site_upsert(message);
            if (pending_regional_map_patch_mask_ != LiveStatePatchField_None)
            {
                pending_regional_map_patch_mask_ |=
                    LiveStatePatchField_RegionalMap |
                    LiveStatePatchField_SelectedSiteId;
            }
            else
            {
                live_state_patch_mask =
                    LiveStatePatchField_RegionalMap |
                    LiveStatePatchField_SelectedSiteId;
            }
            break;
        case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
            apply_regional_map_site_remove(message);
            if (pending_regional_map_patch_mask_ != LiveStatePatchField_None)
            {
                pending_regional_map_patch_mask_ |=
                    LiveStatePatchField_RegionalMap |
                    LiveStatePatchField_SelectedSiteId;
            }
            else
            {
                live_state_patch_mask =
                    LiveStatePatchField_RegionalMap |
                    LiveStatePatchField_SelectedSiteId;
            }
            break;
        case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
            apply_regional_map_link_upsert(message);
            if (pending_regional_map_patch_mask_ != LiveStatePatchField_None)
            {
                pending_regional_map_patch_mask_ |= LiveStatePatchField_RegionalMap;
            }
            else
            {
                live_state_patch_mask = LiveStatePatchField_RegionalMap;
            }
            break;
        case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
            apply_regional_map_link_remove(message);
            if (pending_regional_map_patch_mask_ != LiveStatePatchField_None)
            {
                pending_regional_map_patch_mask_ |= LiveStatePatchField_RegionalMap;
            }
            else
            {
                live_state_patch_mask = LiveStatePatchField_RegionalMap;
            }
            break;
        case GS1_ENGINE_MESSAGE_END_REGIONAL_MAP_SNAPSHOT:
            live_state_patch_mask = pending_regional_map_patch_mask_;
            pending_regional_map_patch_mask_ = LiveStatePatchField_None;
            break;

        case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
            apply_ui_setup_begin(message);
            break;
        case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
            apply_ui_setup_close(message);
            live_state_patch_mask = LiveStatePatchField_UiSetups;
            break;
        case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
            apply_ui_element_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_END_UI_SETUP:
            apply_ui_setup_end();
            live_state_patch_mask = LiveStatePatchField_UiSetups;
            break;
        case GS1_ENGINE_MESSAGE_BEGIN_PROGRESSION_VIEW:
            apply_progression_view_begin(message);
            break;
        case GS1_ENGINE_MESSAGE_PROGRESSION_ENTRY_UPSERT:
            apply_progression_entry_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_END_PROGRESSION_VIEW:
            apply_progression_view_end();
            live_state_patch_mask = LiveStatePatchField_ProgressionViews;
            break;
        case GS1_ENGINE_MESSAGE_CLOSE_PROGRESSION_VIEW:
            apply_progression_view_close(message);
            live_state_patch_mask = LiveStatePatchField_ProgressionViews;
            break;

        case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
            apply_site_snapshot_begin(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
            apply_site_tile_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
            apply_site_worker_update(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
            apply_site_camp_update(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
            apply_site_weather_update(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_INVENTORY_STORAGE_UPSERT:
            apply_site_inventory_storage_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_INVENTORY_SLOT_UPSERT:
            apply_site_inventory_slot_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_INVENTORY_VIEW_STATE:
            apply_site_inventory_view_state(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_BEGIN:
            apply_site_craft_context_begin(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_OPTION_UPSERT:
            apply_site_craft_context_option_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_CRAFT_CONTEXT_END:
            apply_site_craft_context_end();
            break;
        case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW:
            apply_site_placement_preview(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_PREVIEW_TILE_UPSERT:
            apply_site_placement_preview_tile_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_PLACEMENT_FAILURE:
            apply_site_placement_failure(message);
            live_state_patch_mask = LiveStatePatchField_SitePlacementFailure;
            break;
        case GS1_ENGINE_MESSAGE_SITE_TASK_UPSERT:
            apply_site_task_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_MODIFIER_LIST_BEGIN:
            apply_site_modifier_list_begin(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_MODIFIER_UPSERT:
            apply_site_modifier_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_PHONE_PANEL_STATE:
            apply_site_phone_panel_state(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
            apply_site_protection_overlay_state(message);
            live_state_patch_mask = LiveStatePatchField_SiteStateProtectionOverlay;
            break;
        case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_REMOVE:
            apply_site_phone_listing_remove(message);
            break;
        case GS1_ENGINE_MESSAGE_SITE_PHONE_LISTING_UPSERT:
            apply_site_phone_listing_upsert(message);
            break;
        case GS1_ENGINE_MESSAGE_END_SITE_SNAPSHOT:
            apply_site_snapshot_end();
            live_state_patch_mask = pending_site_snapshot_patch_mask_;
            pending_site_snapshot_patch_mask_ = LiveStatePatchField_None;
            break;

        case GS1_ENGINE_MESSAGE_HUD_STATE:
            apply_hud_state(message);
            live_state_patch_mask = LiveStatePatchField_Hud;
            break;
        case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
            apply_campaign_resources(message);
            live_state_patch_mask = LiveStatePatchField_CampaignResources;
            break;
        case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
            apply_site_action_update(message);
            live_state_patch_mask = LiveStatePatchField_SiteAction;
            break;
        case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
            apply_site_result_ready(message);
            live_state_patch_mask = LiveStatePatchField_SiteResult;
            break;
        case GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE:
            apply_one_shot_cue(message);
            live_state_patch_mask = LiveStatePatchField_AudioCues;
            break;

        default:
            break;
        }

        const auto description = describe_message(message);
        std::string log_entry {};

        if (message.type == GS1_ENGINE_MESSAGE_LOG_TEXT)
        {
            log_entry = std::string("[GAMEPLAY] ") + description;
        }
        else
        {
            log_entry = std::string("[ENGINE][CMD] ") + stage_label + ": " + description;
        }

        if (should_log_engine_message_to_sink(log_mode_, message))
        {
            if (message.type == GS1_ENGINE_MESSAGE_LOG_TEXT)
            {
                smoke_log::infof("[GAMEPLAY][FRAME %llu] %s\n",
                    static_cast<unsigned long long>(frame_number_),
                    description.c_str());
            }
            else
            {
                smoke_log::infof("[ENGINE][CMD][FRAME %llu] %s: %s\n",
                    static_cast<unsigned long long>(frame_number_),
                    stage_label,
                    description.c_str());
            }
        }

        message_logs_.push_back(log_entry);
        current_frame_message_entries_.push_back(std::move(log_entry));

        if (live_state_patch_mask != LiveStatePatchField_None)
        {
            queue_live_state_patch(live_state_patch_mask);
        }
    }
}

bool SmokeEngineHost::try_queue_ui_action_from_directive(std::vector<Gs1HostMessage>& destination)
{
    if (!inflight_script_directive_.has_value() || inflight_script_directive_started_)
    {
        return false;
    }

    if (inflight_script_directive_->opcode != SmokeScriptOpcode::ClickUiAction)
    {
        return false;
    }

    Gs1UiAction resolved_action {};
    if (!resolve_available_ui_action(inflight_script_directive_->ui_action, resolved_action))
    {
        return false;
    }

    destination.push_back(make_ui_action_event(resolved_action));
    inflight_script_directive_started_ = true;
    smoke_log::infof("[ENGINE][FRAME %llu] queued ui_action=%s target=%u\n",
        static_cast<unsigned long long>(frame_number_),
        ui_action_name(resolved_action.type),
        static_cast<unsigned>(resolved_action.target_id));
    return true;
}

bool SmokeEngineHost::resolve_available_ui_action(
    const Gs1UiAction& requested_action,
    Gs1UiAction& out_action) const
{
    if (requested_action.type == GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE)
    {
        const auto site_it = regional_map_sites_.find(requested_action.target_id);
        if (site_it == regional_map_sites_.end())
        {
            return false;
        }

        if (site_it->second.site_state != GS1_SITE_STATE_AVAILABLE)
        {
            return false;
        }

        out_action = requested_action;
        return true;
    }

    for (const auto& [setup_id, setup] : active_ui_setups_)
    {
        (void)setup_id;
        for (const auto& element : setup.elements)
        {
            if ((element.flags & GS1_UI_ELEMENT_FLAG_DISABLED) != 0U)
            {
                continue;
            }

            if (ui_action_matches(requested_action, element.action))
            {
                out_action = element.action;
                return true;
            }
        }
    }

    for (const auto& [view_id, view] : active_progression_views_)
    {
        (void)view_id;
        for (const auto& entry : view.entries)
        {
            if ((entry.flags & GS1_PROGRESSION_ENTRY_FLAG_LOCKED) != 0U ||
                entry.action.type == GS1_UI_ACTION_NONE)
            {
                continue;
            }

            if (ui_action_matches(requested_action, entry.action))
            {
                out_action = entry.action;
                return true;
            }
        }
    }

    return false;
}

void SmokeEngineHost::apply_ui_setup_begin(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageUiSetupData>();
    pending_ui_setup_ = PendingUiSetup {};
    pending_ui_setup_->setup_id = payload.setup_id;
    pending_ui_setup_->presentation_type = payload.presentation_type;
    pending_ui_setup_->context_id = payload.context_id;
}

void SmokeEngineHost::apply_ui_setup_close(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageCloseUiSetupData>();
    const auto setup_id = payload.setup_id;
    active_ui_setups_.erase(setup_id);

    if (pending_ui_setup_.has_value() && pending_ui_setup_->setup_id == setup_id)
    {
        pending_ui_setup_.reset();
    }
}

void SmokeEngineHost::apply_ui_element_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_ui_setup_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageUiElementData>();
    ActiveUiElement element {};
    element.element_id = payload.element_id;
    element.element_type = payload.element_type;
    element.flags = payload.flags;
    element.action = payload.action;
    element.content_kind = payload.content_kind;
    element.primary_id = payload.primary_id;
    element.secondary_id = payload.secondary_id;
    element.quantity = payload.quantity;
    pending_ui_setup_->elements.push_back(std::move(element));
}

void SmokeEngineHost::apply_ui_setup_end()
{
    if (!pending_ui_setup_.has_value())
    {
        return;
    }

    if (pending_ui_setup_->elements.empty())
    {
        active_ui_setups_.erase(pending_ui_setup_->setup_id);
        pending_ui_setup_.reset();
        return;
    }

    ActiveUiSetup setup {};
    setup.setup_id = pending_ui_setup_->setup_id;
    setup.presentation_type = pending_ui_setup_->presentation_type;
    setup.context_id = pending_ui_setup_->context_id;
    setup.elements = std::move(pending_ui_setup_->elements);

    if (setup.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
    {
        for (auto it = active_ui_setups_.begin(); it != active_ui_setups_.end();)
        {
            if (it->second.presentation_type == GS1_UI_SETUP_PRESENTATION_NORMAL)
            {
                it = active_ui_setups_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    active_ui_setups_[setup.setup_id] = std::move(setup);
    pending_ui_setup_.reset();
}

void SmokeEngineHost::apply_progression_view_begin(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageProgressionViewData>();
    pending_progression_view_ = PendingProgressionView {};
    pending_progression_view_->view_id = payload.view_id;
    pending_progression_view_->context_id = payload.context_id;
    pending_progression_view_->entries.clear();
    pending_progression_view_->entries.reserve(payload.entry_count);
}

void SmokeEngineHost::apply_progression_view_close(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageCloseProgressionViewData>();
    active_progression_views_.erase(payload.view_id);

    if (pending_progression_view_.has_value() && pending_progression_view_->view_id == payload.view_id)
    {
        pending_progression_view_.reset();
    }
}

void SmokeEngineHost::apply_progression_entry_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_progression_view_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageProgressionEntryData>();
    ProgressionEntryProjection projection {};
    projection.entry_id = payload.entry_id;
    projection.reputation_requirement = payload.reputation_requirement;
    projection.content_id = payload.content_id;
    projection.tech_node_id = payload.tech_node_id;
    projection.faction_id = payload.faction_id;
    projection.entry_kind = payload.entry_kind;
    projection.flags = payload.flags;
    projection.content_kind = payload.content_kind;
    projection.tier_index = payload.tier_index;
    projection.action = payload.action;
    pending_progression_view_->entries.push_back(projection);
}

void SmokeEngineHost::apply_progression_view_end()
{
    if (!pending_progression_view_.has_value())
    {
        return;
    }

    ActiveProgressionView view {};
    view.view_id = pending_progression_view_->view_id;
    view.context_id = pending_progression_view_->context_id;
    view.entries = std::move(pending_progression_view_->entries);
    active_progression_views_[view.view_id] = std::move(view);
    pending_progression_view_.reset();
}

void SmokeEngineHost::apply_regional_map_snapshot_begin(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSnapshotData>();
    pending_regional_map_patch_mask_ =
        LiveStatePatchField_RegionalMap |
        LiveStatePatchField_SelectedSiteId;
    if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
    {
        regional_map_sites_.clear();
        regional_map_links_.clear();
    }

    if (payload.selected_site_id != 0U)
    {
        selected_site_id_ = payload.selected_site_id;
    }
    else
    {
        selected_site_id_.reset();
    }
}

void SmokeEngineHost::apply_regional_map_site_upsert(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
    RegionalMapSiteProjection projection {};
    projection.site_id = payload.site_id;
    projection.site_state = payload.site_state;
    projection.site_archetype_id = payload.site_archetype_id;
    projection.flags = payload.flags;
    projection.map_x = payload.map_x;
    projection.map_y = payload.map_y;
    projection.support_package_id = payload.support_package_id;
    projection.support_preview_mask = payload.support_preview_mask;
    regional_map_sites_[projection.site_id] = projection;

    if ((projection.flags & 1U) != 0U)
    {
        selected_site_id_ = projection.site_id;
    }
    else if (selected_site_id_.has_value() && selected_site_id_.value() == projection.site_id)
    {
        selected_site_id_.reset();
    }
}

void SmokeEngineHost::apply_regional_map_site_remove(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
    const auto site_id = payload.site_id;
    regional_map_sites_.erase(site_id);
    if (selected_site_id_.has_value() && selected_site_id_.value() == site_id)
    {
        selected_site_id_.reset();
    }
}

void SmokeEngineHost::apply_regional_map_link_upsert(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
    RegionalMapLinkProjection projection {};
    projection.from_site_id = payload.from_site_id;
    projection.to_site_id = payload.to_site_id;
    projection.flags = payload.flags;
    regional_map_links_[make_regional_map_link_key(projection.from_site_id, projection.to_site_id)] = projection;
}

void SmokeEngineHost::apply_regional_map_link_remove(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
    regional_map_links_.erase(make_regional_map_link_key(
        payload.from_site_id,
        payload.to_site_id));
}

void SmokeEngineHost::apply_site_snapshot_begin(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
    if (payload.mode == GS1_PROJECTION_MODE_DELTA && active_site_snapshot_.has_value())
    {
        pending_site_snapshot_patch_mask_ = LiveStatePatchField_None;
        pending_site_snapshot_ = active_site_snapshot_.value();
    }
    else
    {
        pending_site_snapshot_patch_mask_ = LiveStatePatchField_SiteState;
        pending_site_snapshot_ = SiteSnapshotProjection {};
    }

    pending_site_snapshot_->site_id = payload.site_id;
    pending_site_snapshot_->site_archetype_id = payload.site_archetype_id;
    pending_site_snapshot_->width = payload.width;
    pending_site_snapshot_->height = payload.height;

    if (payload.mode == GS1_PROJECTION_MODE_SNAPSHOT)
    {
        pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteBootstrap;
        pending_site_snapshot_->tiles.clear();
        pending_site_snapshot_->inventory_storages.clear();
        pending_site_snapshot_->worker_pack_slots.clear();
        pending_site_snapshot_->tasks.clear();
        pending_site_snapshot_->phone_listings.clear();
        pending_site_snapshot_->phone_panel = SitePhonePanelProjection {};
        pending_site_snapshot_->protection_overlay = SiteProtectionOverlayProjection {};
        pending_site_snapshot_->opened_storage.reset();
        pending_site_snapshot_->craft_context.reset();
        pending_site_snapshot_->placement_preview.reset();
        pending_site_snapshot_->placement_failure.reset();
        pending_site_snapshot_->worker.reset();
        pending_site_snapshot_->camp.reset();
        pending_site_snapshot_->weather.reset();
    }
}

void SmokeEngineHost::apply_site_tile_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageSiteTileData>();
    SiteTileProjection projection {};
    projection.x = payload.x;
    projection.y = payload.y;
    projection.terrain_type_id = payload.terrain_type_id;
    projection.plant_type_id = payload.plant_type_id;
    projection.structure_type_id = payload.structure_type_id;
    projection.ground_cover_type_id = payload.ground_cover_type_id;
    projection.plant_density = payload.plant_density;
    projection.sand_burial = payload.sand_burial;
    projection.local_wind = payload.local_wind;
    projection.wind_protection =
        resolve_final_overlay_protection_value(
            projection.plant_type_id,
            projection.plant_density,
            payload.wind_protection,
            &gs1::PlantDef::wind_resistance);
    projection.heat_protection =
        resolve_final_overlay_protection_value(
            projection.plant_type_id,
            projection.plant_density,
            payload.heat_protection,
            &gs1::PlantDef::heat_tolerance);
    projection.dust_protection =
        resolve_final_overlay_protection_value(
            projection.plant_type_id,
            projection.plant_density,
            payload.dust_protection,
            &gs1::PlantDef::dust_tolerance);
    projection.moisture = payload.moisture;
    projection.soil_fertility = payload.soil_fertility;
    projection.soil_salinity = payload.soil_salinity;
    projection.device_integrity =
        static_cast<float>(payload.device_integrity_quantized) * (100.0f / 128.0f);
    projection.excavation_depth = payload.excavation_depth;
    projection.visible_excavation_depth = payload.visible_excavation_depth;

    auto& tiles = pending_site_snapshot_->tiles;
    const auto existing = std::find_if(tiles.begin(), tiles.end(), [&](const SiteTileProjection& tile) {
        return tile.x == projection.x && tile.y == projection.y;
    });
    if (existing != tiles.end())
    {
        *existing = projection;
    }
    else
    {
        tiles.push_back(projection);
    }

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteBootstrap;
}

void SmokeEngineHost::apply_site_worker_update(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageWorkerData>();
    SiteWorkerProjection projection {};
    projection.entity_id = payload.entity_id;
    projection.tile_x = payload.tile_x;
    projection.tile_y = payload.tile_y;
    projection.facing_degrees = payload.facing_degrees;
    projection.health_normalized = payload.health_normalized;
    projection.hydration_normalized = payload.hydration_normalized;
    projection.energy_normalized = payload.energy_normalized;
    projection.flags = payload.flags;
    projection.current_action_kind = payload.current_action_kind;
    pending_site_snapshot_->worker = projection;
    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateWorker;
}

void SmokeEngineHost::apply_site_camp_update(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageCampData>();
    SiteCampProjection projection {};
    projection.tile_x = payload.tile_x;
    projection.tile_y = payload.tile_y;
    projection.durability_normalized = payload.durability_normalized;
    projection.flags = payload.flags;
    pending_site_snapshot_->camp = projection;
    pending_site_snapshot_patch_mask_ |=
        LiveStatePatchField_SiteBootstrap |
        LiveStatePatchField_SiteStateCamp;
}

void SmokeEngineHost::apply_site_weather_update(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageWeatherData>();
    SiteWeatherProjection projection {};
    projection.heat = payload.heat;
    projection.wind = payload.wind;
    projection.dust = payload.dust;
    projection.wind_direction_degrees = payload.wind_direction_degrees;
    projection.event_template_id = payload.event_template_id;
    projection.event_start_time_minutes = payload.event_start_time_minutes;
    projection.event_peak_time_minutes = payload.event_peak_time_minutes;
    projection.event_peak_duration_minutes = payload.event_peak_duration_minutes;
    projection.event_end_time_minutes = payload.event_end_time_minutes;
    pending_site_snapshot_->weather = projection;
    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateWeather;
}

void SmokeEngineHost::apply_site_inventory_storage_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageInventoryStorageData>();
    SiteInventoryStorageProjection projection {};
    projection.storage_id = payload.storage_id;
    projection.owner_entity_id = payload.owner_entity_id;
    projection.slot_count = payload.slot_count;
    projection.tile_x = payload.tile_x;
    projection.tile_y = payload.tile_y;
    projection.container_kind = payload.container_kind;
    projection.flags = payload.flags;

    auto& storages = pending_site_snapshot_->inventory_storages;
    const auto existing = std::find_if(storages.begin(), storages.end(), [&](const SiteInventoryStorageProjection& storage) {
        return storage.storage_id == projection.storage_id;
    });
    if (existing != storages.end())
    {
        *existing = projection;
    }
    else
    {
        storages.push_back(projection);
    }

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateInventory;
}

void SmokeEngineHost::apply_site_inventory_slot_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageInventorySlotData>();
    SiteInventorySlotProjection projection {};
    projection.item_id = payload.item_id;
    projection.item_instance_id = payload.item_instance_id;
    projection.storage_id = payload.storage_id;
    projection.condition = payload.condition;
    projection.freshness = payload.freshness;
    projection.container_owner_id = payload.container_owner_id;
    projection.quantity = payload.quantity;
    projection.slot_index = payload.slot_index;
    projection.container_tile_x = payload.container_tile_x;
    projection.container_tile_y = payload.container_tile_y;
    projection.container_kind = payload.container_kind;
    projection.flags = payload.flags;

    std::vector<SiteInventorySlotProjection>* slots = nullptr;
    if (projection.container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK)
    {
        slots = &pending_site_snapshot_->worker_pack_slots;
    }
    else if (pending_site_snapshot_->opened_storage.has_value() &&
        pending_site_snapshot_->opened_storage->storage_id == projection.storage_id)
    {
        slots = &pending_site_snapshot_->opened_storage->slots;
    }

    if (slots == nullptr)
    {
        return;
    }

    const auto existing = std::find_if(slots->begin(), slots->end(), [&](const SiteInventorySlotProjection& slot) {
        return slot.storage_id == projection.storage_id &&
            slot.slot_index == projection.slot_index;
    });
    if (existing != slots->end())
    {
        *existing = projection;
    }
    else
    {
        slots->push_back(projection);
    }

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateInventory;
}

void SmokeEngineHost::apply_site_inventory_view_state(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageInventoryViewData>();
    const auto is_worker_pack_storage = [&]() {
        const auto storage = std::find_if(
            pending_site_snapshot_->inventory_storages.begin(),
            pending_site_snapshot_->inventory_storages.end(),
            [&](const SiteInventoryStorageProjection& projection) {
                return projection.storage_id == payload.storage_id;
            });
        return storage != pending_site_snapshot_->inventory_storages.end() &&
            storage->container_kind == GS1_INVENTORY_CONTAINER_WORKER_PACK;
    }();

    if (is_worker_pack_storage)
    {
        pending_site_snapshot_->worker_pack_open =
            payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT;
        pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateInventory;
        return;
    }

    if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_CLOSE)
    {
        if (pending_site_snapshot_->opened_storage.has_value() &&
            pending_site_snapshot_->opened_storage->storage_id == payload.storage_id)
        {
            pending_site_snapshot_->opened_storage.reset();
        }
    }
    else if (payload.event_kind == GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
    {
        pending_site_snapshot_->opened_storage = SiteInventoryViewProjection {};
        pending_site_snapshot_->opened_storage->storage_id = payload.storage_id;
        pending_site_snapshot_->opened_storage->slot_count = payload.slot_count;
        pending_site_snapshot_->opened_storage->slots.clear();
    }

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateInventory;
}

void SmokeEngineHost::apply_site_craft_context_begin(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageCraftContextData>();
    pending_site_snapshot_->craft_context = SiteCraftContextProjection {};
    pending_site_snapshot_->craft_context->tile_x = payload.tile_x;
    pending_site_snapshot_->craft_context->tile_y = payload.tile_y;
    pending_site_snapshot_->craft_context->flags = payload.flags;
    pending_site_snapshot_->craft_context->options.clear();
    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateCraftContext;
}

void SmokeEngineHost::apply_site_craft_context_option_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value() || !pending_site_snapshot_->craft_context.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageCraftContextOptionData>();
    pending_site_snapshot_->craft_context->options.push_back(SiteCraftContextOptionProjection {
        payload.recipe_id,
        payload.output_item_id,
        payload.flags});
    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateCraftContext;
}

void SmokeEngineHost::apply_site_craft_context_end()
{
    if (!pending_site_snapshot_.has_value() || !pending_site_snapshot_->craft_context.has_value())
    {
        return;
    }

    auto& options = pending_site_snapshot_->craft_context->options;
    std::sort(options.begin(), options.end(), [](const SiteCraftContextOptionProjection& lhs, const SiteCraftContextOptionProjection& rhs) {
        if (lhs.output_item_id != rhs.output_item_id)
        {
            return lhs.output_item_id < rhs.output_item_id;
        }
        return lhs.recipe_id < rhs.recipe_id;
    });
    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateCraftContext;
}

void SmokeEngineHost::apply_site_placement_preview(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessagePlacementPreviewData>();
    if ((payload.flags & 1U) == 0U)
    {
        pending_site_snapshot_->placement_preview.reset();
        pending_site_snapshot_->placement_preview_tiles.clear();
    }
    else
    {
        pending_site_snapshot_->placement_preview = SitePlacementPreviewProjection {
            payload.tile_x,
            payload.tile_y,
            payload.blocked_mask,
            payload.item_id,
            payload.action_kind,
            payload.flags,
            payload.footprint_width,
            payload.footprint_height};
        pending_site_snapshot_->placement_preview_tiles.clear();
    }

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStatePlacementPreview;
}

void SmokeEngineHost::apply_site_placement_preview_tile_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessagePlacementPreviewTileData>();
    SitePlacementPreviewTileProjection projection {};
    projection.x = payload.x;
    projection.y = payload.y;
    projection.flags = payload.flags;
    projection.wind_protection = payload.wind_protection;
    projection.heat_protection = payload.heat_protection;
    projection.dust_protection = payload.dust_protection;
    projection.final_wind_protection = payload.final_wind_protection;
    projection.final_heat_protection = payload.final_heat_protection;
    projection.final_dust_protection = payload.final_dust_protection;
    projection.occupant_condition = payload.occupant_condition;

    auto& preview_tiles = pending_site_snapshot_->placement_preview_tiles;
    const auto existing = std::find_if(
        preview_tiles.begin(),
        preview_tiles.end(),
        [&](const SitePlacementPreviewTileProjection& tile) {
            return tile.x == projection.x && tile.y == projection.y;
        });
    if (existing != preview_tiles.end())
    {
        *existing = projection;
    }
    else
    {
        preview_tiles.push_back(projection);
    }

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStatePlacementPreview;
}

void SmokeEngineHost::apply_site_placement_failure(const Gs1RuntimeMessage& message)
{
    auto* snapshot = pending_site_snapshot_.has_value()
        ? &pending_site_snapshot_.value()
        : (active_site_snapshot_.has_value() ? &active_site_snapshot_.value() : nullptr);
    if (snapshot == nullptr)
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessagePlacementFailureData>();
    snapshot->placement_failure = SitePlacementFailureProjection {
        payload.tile_x,
        payload.tile_y,
        payload.blocked_mask,
        payload.action_kind,
        payload.sequence_id,
        payload.flags};
}

void SmokeEngineHost::apply_site_task_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageTaskData>();
    SiteTaskProjection projection {};
    projection.task_instance_id = payload.task_instance_id;
    projection.task_template_id = payload.task_template_id;
    projection.publisher_faction_id = payload.publisher_faction_id;
    projection.current_progress = payload.current_progress;
    projection.target_progress = payload.target_progress;
    projection.list_kind = payload.list_kind;
    projection.flags = payload.flags;

    auto& tasks = pending_site_snapshot_->tasks;
    const auto existing = std::find_if(tasks.begin(), tasks.end(), [&](const SiteTaskProjection& task) {
        return task.task_instance_id == projection.task_instance_id;
    });
    if (existing != tasks.end())
    {
        *existing = projection;
    }
    else
    {
        tasks.push_back(projection);
    }

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateTasks;
}

void SmokeEngineHost::apply_site_phone_panel_state(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessagePhonePanelData>();
    pending_site_snapshot_->phone_panel = SitePhonePanelProjection {
        payload.active_section,
        payload.visible_task_count,
        payload.accepted_task_count,
        payload.completed_task_count,
        payload.claimed_task_count,
        payload.buy_listing_count,
        payload.sell_listing_count,
        payload.service_listing_count,
        payload.cart_item_count,
        payload.flags};
    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStatePhone;
}

void SmokeEngineHost::apply_site_modifier_list_begin(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageSiteModifierListData>();
    pending_site_snapshot_->active_modifiers.clear();
    pending_site_snapshot_->active_modifiers.reserve(payload.modifier_count);
    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateModifiers;
}

void SmokeEngineHost::apply_site_modifier_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageSiteModifierData>();
    pending_site_snapshot_->active_modifiers.push_back(
        SiteModifierProjection {
            payload.modifier_id,
            payload.remaining_game_hours,
            payload.flags});
    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStateModifiers;
}

void SmokeEngineHost::apply_site_protection_overlay_state(const Gs1RuntimeMessage& message)
{
    auto* snapshot = pending_site_snapshot_.has_value()
        ? &pending_site_snapshot_.value()
        : (active_site_snapshot_.has_value() ? &active_site_snapshot_.value() : nullptr);
    if (snapshot == nullptr)
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
    snapshot->protection_overlay = SiteProtectionOverlayProjection {payload.mode};
}

void SmokeEngineHost::apply_site_phone_listing_upsert(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessagePhoneListingData>();
    SitePhoneListingProjection projection {};
    projection.listing_id = payload.listing_id;
    projection.item_or_unlockable_id = payload.item_or_unlockable_id;
    projection.price = payload.price;
    projection.related_site_id = payload.related_site_id;
    projection.quantity = payload.quantity;
    projection.cart_quantity = payload.cart_quantity;
    projection.listing_kind = payload.listing_kind;
    projection.flags = payload.flags;

    auto& listings = pending_site_snapshot_->phone_listings;
    const auto existing = std::find_if(listings.begin(), listings.end(), [&](const SitePhoneListingProjection& listing) {
        return listing.listing_id == projection.listing_id;
    });
    if (existing != listings.end())
    {
        *existing = projection;
    }
    else
    {
        listings.push_back(projection);
    }

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStatePhone;
}

void SmokeEngineHost::apply_site_phone_listing_remove(const Gs1RuntimeMessage& message)
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    const auto& payload = message.payload_as<Gs1EngineMessagePhoneListingData>();
    auto& listings = pending_site_snapshot_->phone_listings;
    listings.erase(
        std::remove_if(listings.begin(), listings.end(), [&](const SitePhoneListingProjection& listing) {
            return listing.listing_id == payload.listing_id;
        }),
        listings.end());

    pending_site_snapshot_patch_mask_ |= LiveStatePatchField_SiteStatePhone;
}

void SmokeEngineHost::apply_site_snapshot_end()
{
    if (!pending_site_snapshot_.has_value())
    {
        return;
    }

    auto& tiles = pending_site_snapshot_->tiles;
    std::sort(tiles.begin(), tiles.end(), [](const SiteTileProjection& lhs, const SiteTileProjection& rhs) {
        if (lhs.y != rhs.y)
        {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    });
    auto& inventory_storages = pending_site_snapshot_->inventory_storages;
    std::sort(inventory_storages.begin(), inventory_storages.end(), [](const SiteInventoryStorageProjection& lhs, const SiteInventoryStorageProjection& rhs) {
        if (lhs.container_kind != rhs.container_kind)
        {
            return lhs.container_kind < rhs.container_kind;
        }
        return lhs.storage_id < rhs.storage_id;
    });
    auto& worker_pack_slots = pending_site_snapshot_->worker_pack_slots;
    std::sort(worker_pack_slots.begin(), worker_pack_slots.end(), [](const SiteInventorySlotProjection& lhs, const SiteInventorySlotProjection& rhs) {
        if (lhs.container_kind != rhs.container_kind)
        {
            return lhs.container_kind < rhs.container_kind;
        }
        return lhs.slot_index < rhs.slot_index;
    });
    if (pending_site_snapshot_->opened_storage.has_value())
    {
        auto& opened_slots = pending_site_snapshot_->opened_storage->slots;
        std::sort(opened_slots.begin(), opened_slots.end(), [](const SiteInventorySlotProjection& lhs, const SiteInventorySlotProjection& rhs) {
            return lhs.slot_index < rhs.slot_index;
        });
    }
    auto& tasks = pending_site_snapshot_->tasks;
    std::sort(tasks.begin(), tasks.end(), [](const SiteTaskProjection& lhs, const SiteTaskProjection& rhs) {
        return lhs.task_instance_id < rhs.task_instance_id;
    });
    auto& active_modifiers = pending_site_snapshot_->active_modifiers;
    std::sort(
        active_modifiers.begin(),
        active_modifiers.end(),
        [](const SiteModifierProjection& lhs, const SiteModifierProjection& rhs) {
            if ((lhs.flags & GS1_SITE_MODIFIER_FLAG_TIMED) != (rhs.flags & GS1_SITE_MODIFIER_FLAG_TIMED))
            {
                return (lhs.flags & GS1_SITE_MODIFIER_FLAG_TIMED) != 0U;
            }
            return lhs.modifier_id < rhs.modifier_id;
        });
    auto& phone_listings = pending_site_snapshot_->phone_listings;
    std::sort(phone_listings.begin(), phone_listings.end(), [](const SitePhoneListingProjection& lhs, const SitePhoneListingProjection& rhs) {
        return lhs.listing_id < rhs.listing_id;
    });

    active_site_snapshot_ = std::move(pending_site_snapshot_);
    pending_site_snapshot_.reset();
}

void SmokeEngineHost::apply_hud_state(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageHudStateData>();
    HudProjection projection {};
    projection.player_health = payload.player_health;
    projection.player_hydration = payload.player_hydration;
    projection.player_nourishment = payload.player_nourishment;
    projection.player_energy = payload.player_energy;
    projection.player_morale = payload.player_morale;
    projection.current_money = payload.current_money;
    projection.active_task_count = payload.active_task_count;
    projection.warning_code = payload.warning_code;
    projection.current_action_kind = payload.current_action_kind;
    projection.site_completion_normalized = payload.site_completion_normalized;
    hud_state_ = projection;
}

void SmokeEngineHost::apply_campaign_resources(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageCampaignResourcesData>();
    CampaignResourcesProjection projection {};
    projection.current_money = payload.current_money;
    projection.total_reputation = payload.total_reputation;
    projection.village_reputation = payload.village_reputation;
    projection.forestry_reputation = payload.forestry_reputation;
    projection.university_reputation = payload.university_reputation;
    campaign_resources_ = projection;
}

void SmokeEngineHost::apply_site_action_update(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageSiteActionData>();
    SiteActionProjection projection {};
    projection.action_id = payload.action_id;
    projection.target_tile_x = payload.target_tile_x;
    projection.target_tile_y = payload.target_tile_y;
    projection.action_kind = payload.action_kind;
    projection.flags = payload.flags;
    projection.progress_normalized = payload.progress_normalized;
    projection.duration_minutes = payload.duration_minutes;

    if ((payload.flags & GS1_SITE_ACTION_PRESENTATION_FLAG_CLEAR) != 0U)
    {
        site_action_.reset();
        return;
    }

    site_action_ = projection;
}

void SmokeEngineHost::apply_site_result_ready(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageSiteResultData>();
    SiteResultProjection projection {};
    projection.site_id = payload.site_id;
    projection.result = payload.result;
    projection.newly_revealed_site_count = payload.newly_revealed_site_count;
    site_result_ = projection;
}

void SmokeEngineHost::apply_one_shot_cue(const Gs1RuntimeMessage& message)
{
    const auto& payload = message.payload_as<Gs1EngineMessageOneShotCueData>();
    OneShotCueProjection projection {};
    projection.sequence_id = next_one_shot_cue_sequence_id_;
    next_one_shot_cue_sequence_id_ += 1U;
    projection.frame_number = frame_number_;
    projection.subject_id = payload.subject_id;
    projection.arg0 = payload.arg0;
    projection.arg1 = payload.arg1;
    projection.world_x = payload.world_x;
    projection.world_y = payload.world_y;
    projection.cue_kind = payload.cue_kind;
    recent_one_shot_cues_.push_back(projection);

    constexpr std::size_t k_max_recent_one_shot_cues = 16U;
    if (recent_one_shot_cues_.size() > k_max_recent_one_shot_cues)
    {
        const auto overflow_count = static_cast<std::vector<OneShotCueProjection>::difference_type>(
            recent_one_shot_cues_.size() - k_max_recent_one_shot_cues);
        recent_one_shot_cues_.erase(
            recent_one_shot_cues_.begin(),
            recent_one_shot_cues_.begin() + overflow_count);
    }
}

void SmokeEngineHost::queue_live_state_patch(std::uint32_t field_mask)
{
    if (field_mask == LiveStatePatchField_None)
    {
        return;
    }
    frame_live_state_patch_mask_ |= field_mask;
}

std::vector<SmokeEngineHost::ActiveUiSetup> SmokeEngineHost::snapshot_active_ui_setups() const
{
    std::vector<ActiveUiSetup> setups {};
    setups.reserve(active_ui_setups_.size());

    for (const auto& [setup_id, setup] : active_ui_setups_)
    {
        (void)setup_id;
        setups.push_back(setup);
    }

    return setups;
}

std::vector<SmokeEngineHost::ActiveProgressionView> SmokeEngineHost::snapshot_active_progression_views() const
{
    std::vector<ActiveProgressionView> views {};
    views.reserve(active_progression_views_.size());

    for (const auto& [view_id, view] : active_progression_views_)
    {
        (void)view_id;
        views.push_back(view);
    }

    return views;
}

std::vector<SmokeEngineHost::RegionalMapSiteProjection> SmokeEngineHost::snapshot_regional_map_sites() const
{
    std::vector<RegionalMapSiteProjection> sites {};
    sites.reserve(regional_map_sites_.size());

    for (const auto& [site_id, site] : regional_map_sites_)
    {
        (void)site_id;
        sites.push_back(site);
    }

    return sites;
}

std::vector<SmokeEngineHost::RegionalMapLinkProjection> SmokeEngineHost::snapshot_regional_map_links() const
{
    std::vector<RegionalMapLinkProjection> links {};
    links.reserve(regional_map_links_.size());

    for (const auto& [link_key, link] : regional_map_links_)
    {
        (void)link_key;
        links.push_back(link);
    }

    return links;
}

std::uint64_t SmokeEngineHost::make_regional_map_link_key(
    std::uint32_t from_site_id,
    std::uint32_t to_site_id) noexcept
{
    const auto low = (from_site_id < to_site_id) ? from_site_id : to_site_id;
    const auto high = (from_site_id < to_site_id) ? to_site_id : from_site_id;
    return (static_cast<std::uint64_t>(low) << 32U) | static_cast<std::uint64_t>(high);
}

std::string SmokeEngineHost::describe_message(const Gs1RuntimeMessage& message)
{
    std::string description = message_type_name(message.type);

    switch (message.type)
    {
    case GS1_ENGINE_MESSAGE_LOG_TEXT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageLogTextData>();
        description += " text=";
        description += payload.text;
        break;
    }

    case GS1_ENGINE_MESSAGE_SET_APP_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSetAppStateData>();
        description += " state=";
        description += app_state_name(payload.app_state);
        break;
    }

    case GS1_ENGINE_MESSAGE_PRESENTATION_DIRTY:
    {
        const auto& payload = message.payload_as<Gs1EngineMessagePresentationDirtyData>();
        description += " flags=" + std::to_string(payload.dirty_flags);
        description += " revision=" + std::to_string(payload.revision);
        break;
    }

    case GS1_ENGINE_MESSAGE_BEGIN_REGIONAL_MAP_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSnapshotData>();
        description += " mode=";
        description += projection_mode_name(payload.mode);
        description += " sites=" + std::to_string(payload.site_count);
        description += " links=" + std::to_string(payload.link_count);
        description += " selected=" + std::to_string(payload.selected_site_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_UPSERT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
        description += " site=" + std::to_string(payload.site_id);
        description += " state=";
        description += site_state_name(payload.site_state);
        description += " pos=(" + std::to_string(payload.map_x);
        description += "," + std::to_string(payload.map_y) + ")";
        break;
    }

    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_SITE_REMOVE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapSiteData>();
        description += " site=" + std::to_string(payload.site_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_UPSERT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
        description += " from=" + std::to_string(payload.from_site_id);
        description += " to=" + std::to_string(payload.to_site_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_REGIONAL_MAP_LINK_REMOVE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageRegionalMapLinkData>();
        description += " from=" + std::to_string(payload.from_site_id);
        description += " to=" + std::to_string(payload.to_site_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_BEGIN_UI_SETUP:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageUiSetupData>();
        description += " setup=";
        description += ui_setup_name(payload.setup_id);
        description += " type=";
        description += ui_setup_presentation_type_name(payload.presentation_type);
        description += " elements=" + std::to_string(payload.element_count);
        description += " context=" + std::to_string(payload.context_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_CLOSE_UI_SETUP:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCloseUiSetupData>();
        description += " setup=";
        description += ui_setup_name(payload.setup_id);
        description += " type=";
        description += ui_setup_presentation_type_name(payload.presentation_type);
        break;
    }

    case GS1_ENGINE_MESSAGE_UI_ELEMENT_UPSERT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageUiElementData>();
        description += " contentKind=" + std::to_string(payload.content_kind);
        description += " primary=" + std::to_string(payload.primary_id);
        description += " secondary=" + std::to_string(payload.secondary_id);
        description += " quantity=" + std::to_string(payload.quantity);
        description += " action=";
        description += ui_action_name(payload.action.type);
        description += " target=" + std::to_string(payload.action.target_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_BEGIN_SITE_SNAPSHOT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteSnapshotData>();
        description += " mode=";
        description += projection_mode_name(payload.mode);
        description += " site=" + std::to_string(payload.site_id);
        description += " size=" + std::to_string(payload.width);
        description += "x" + std::to_string(payload.height);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_TILE_UPSERT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteTileData>();
        description += " x=" + std::to_string(payload.x);
        description += " y=" + std::to_string(payload.y);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_WORKER_UPDATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageWorkerData>();
        description += " tile=(" + std::to_string(payload.tile_x);
        description += "," + std::to_string(payload.tile_y) + ")";
        description += " health=" + std::to_string(payload.health_normalized);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_CAMP_UPDATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCampData>();
        description += " tile=(" + std::to_string(payload.tile_x);
        description += "," + std::to_string(payload.tile_y) + ")";
        description += " durability=" + std::to_string(payload.durability_normalized);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_WEATHER_UPDATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageWeatherData>();
        description += " heat=" + std::to_string(payload.heat);
        description += " wind=" + std::to_string(payload.wind);
        description += " dust=" + std::to_string(payload.dust);
        description += " eventTemplate=" + std::to_string(payload.event_template_id);
        description += " start=" + std::to_string(payload.event_start_time_minutes);
        description += " peak=" + std::to_string(payload.event_peak_time_minutes);
        description += " peakDuration=" + std::to_string(payload.event_peak_duration_minutes);
        description += " end=" + std::to_string(payload.event_end_time_minutes);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_ACTION_UPDATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteActionData>();
        description += " action=" + std::to_string(payload.action_id);
        description += " kind=" + std::to_string(payload.action_kind);
        description += " target=(" + std::to_string(payload.target_tile_x);
        description += "," + std::to_string(payload.target_tile_y) + ")";
        description += " duration=" + std::to_string(payload.duration_minutes);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_PROTECTION_OVERLAY_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteProtectionOverlayData>();
        description += " mode=";
        description += site_protection_overlay_mode_name(payload.mode);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_UPSERT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSitePlantVisualData>();
        description += " plant=" + std::to_string(payload.plant_type_id);
        description += " anchor=(" + std::to_string(payload.anchor_tile_x);
        description += "," + std::to_string(payload.anchor_tile_y) + ")";
        description += " visual=" + std::to_string(payload.visual_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_PLANT_VISUAL_REMOVE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteVisualRemoveData>();
        description += " visual=" + std::to_string(payload.visual_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_UPSERT:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteDeviceVisualData>();
        description += " structure=" + std::to_string(payload.structure_type_id);
        description += " anchor=(" + std::to_string(payload.anchor_tile_x);
        description += "," + std::to_string(payload.anchor_tile_y) + ")";
        description += " visual=" + std::to_string(payload.visual_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_DEVICE_VISUAL_REMOVE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteVisualRemoveData>();
        description += " visual=" + std::to_string(payload.visual_id);
        break;
    }

    case GS1_ENGINE_MESSAGE_HUD_STATE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageHudStateData>();
        description += " health=" + std::to_string(payload.player_health);
        description += " hydration=" + std::to_string(payload.player_hydration);
        description += " nourishment=" + std::to_string(payload.player_nourishment);
        description += " energy=" + std::to_string(payload.player_energy);
        description += " morale=" + std::to_string(payload.player_morale);
        description += " completion=" + std::to_string(payload.site_completion_normalized);
        break;
    }

    case GS1_ENGINE_MESSAGE_CAMPAIGN_RESOURCES:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageCampaignResourcesData>();
        description += " cash=" + std::to_string(payload.current_money);
        description += " reputation=" + std::to_string(payload.total_reputation);
        description += " village=" + std::to_string(payload.village_reputation);
        description += " forestry=" + std::to_string(payload.forestry_reputation);
        description += " university=" + std::to_string(payload.university_reputation);
        break;
    }

    case GS1_ENGINE_MESSAGE_SITE_RESULT_READY:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageSiteResultData>();
        description += " site=" + std::to_string(payload.site_id);
        description += " result=";
        description += site_attempt_result_name(payload.result);
        description += " revealed=" + std::to_string(payload.newly_revealed_site_count);
        break;
    }

    case GS1_ENGINE_MESSAGE_PLAY_ONE_SHOT_CUE:
    {
        const auto& payload = message.payload_as<Gs1EngineMessageOneShotCueData>();
        description += " cue=";
        description += one_shot_cue_name(payload.cue_kind);
        description += " subject=" + std::to_string(payload.subject_id);
        description += " pos=(" + std::to_string(payload.world_x);
        description += "," + std::to_string(payload.world_y) + ")";
        description += " arg0=" + std::to_string(payload.arg0);
        description += " arg1=" + std::to_string(payload.arg1);
        break;
    }

    default:
        break;
    }

    return description;
}

Gs1HostMessage SmokeEngineHost::make_ui_action_event(const Gs1UiAction& action) noexcept
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_UI_ACTION;
    event.payload.ui_action.action = action;
    return event;
}

Gs1HostMessage SmokeEngineHost::make_site_scene_ready_event() noexcept
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_SCENE_READY;
    return event;
}

Gs1HostMessage SmokeEngineHost::make_site_move_direction_event(
    float world_move_x,
    float world_move_y,
    float world_move_z) noexcept
{
    Gs1HostMessage event {};
    event.type = GS1_HOST_EVENT_SITE_MOVE_DIRECTION;
    event.payload.site_move_direction.world_move_x = world_move_x;
    event.payload.site_move_direction.world_move_y = world_move_y;
    event.payload.site_move_direction.world_move_z = world_move_z;
    return event;
}


