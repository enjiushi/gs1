#include "app/site_presentation_ops.h"

#include "app/game_presentation_coordinator.h"
#include "runtime/game_runtime.h"
#include "site/systems/phone_panel_system.h"

#include <algorithm>

namespace gs1
{
void site_presentation_handle_message(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context,
    const GameMessage& message)
{
    owner.active_context_ = &context;

    switch (message.type)
    {
    case GameMessageType::OpenRegionalMapTechTree:
        owner.queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        owner.queue_site_protection_overlay_state_message();
        break;

    case GameMessageType::PhonePanelSectionRequested:
        owner.queue_site_protection_overlay_state_message();
        break;

    case GameMessageType::InventoryStorageViewRequest:
        if (message.payload_as<InventoryStorageViewRequestMessage>().event_kind ==
            GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT)
        {
            owner.queue_site_protection_overlay_state_message();
        }
        break;

    case GameMessageType::TaskRewardClaimResolved:
    {
        const auto& payload = message.payload_as<TaskRewardClaimResolvedMessage>();
        site_presentation_flush_if_dirty(owner, context);
        owner.queue_task_reward_claimed_cue_message(
            payload.task_instance_id,
            payload.task_template_id,
            payload.reward_candidate_count);
        break;
    }

    case GameMessageType::InventoryCraftCompleted:
    {
        const auto& payload = message.payload_as<InventoryCraftCompletedMessage>();
        if (owner.active_site_run().has_value() && payload.output_storage_id != 0U)
        {
            GameMessage open_storage {};
            open_storage.type = GameMessageType::InventoryStorageViewRequest;
            open_storage.set_payload(InventoryStorageViewRequestMessage {
                payload.output_storage_id,
                GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT,
                {0U, 0U, 0U}});
            owner.message_queue().push_back(open_storage);
            site_presentation_flush_if_dirty(owner, context);
            owner.queue_craft_output_stored_cue_message(
                payload.output_storage_id,
                payload.output_item_id,
                payload.output_quantity == 0U ? 1U : payload.output_quantity);
        }
        break;
    }

    case GameMessageType::SiteActionStarted:
    case GameMessageType::SiteActionCompleted:
    case GameMessageType::SiteActionFailed:
        owner.queue_site_action_update_message();
        break;

    case GameMessageType::PlacementModeCommitRejected:
        owner.queue_site_placement_failure_message(
            message.payload_as<PlacementModeCommitRejectedMessage>());
        break;

    case GameMessageType::OpenSiteProtectionSelector:
        owner.queue_site_protection_selector_ui_messages();
        break;

    case GameMessageType::CloseSiteProtectionUi:
    case GameMessageType::SetSiteProtectionOverlayMode:
        owner.queue_close_ui_setup_if_open(GS1_UI_SETUP_SITE_PROTECTION_SELECTOR);
        owner.queue_site_protection_overlay_state_message();
        break;

    default:
        break;
    }
}

void site_presentation_mark_projection_dirty(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context,
    std::uint64_t dirty_flags) noexcept
{
    owner.active_context_ = &context;
    if (!owner.active_site_run().has_value())
    {
        return;
    }

    if ((dirty_flags & SITE_PROJECTION_UPDATE_TILES) != 0U)
    {
        owner.active_site_run()->pending_full_tile_projection_update = true;
    }
    if ((dirty_flags & SITE_PROJECTION_UPDATE_INVENTORY) != 0U)
    {
        owner.active_site_run()->pending_full_inventory_projection_update = true;
        owner.active_site_run()->pending_inventory_storage_descriptor_projection_update = true;
        if (owner.active_site_run()->inventory.opened_device_storage_id != 0U)
        {
            owner.active_site_run()->pending_opened_inventory_storage_full_projection_update = true;
        }
    }

    owner.active_site_run()->pending_projection_update_flags |= dirty_flags;
}

void site_presentation_mark_tile_dirty(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context,
    TileCoord coord) noexcept
{
    owner.active_context_ = &context;
    if (!owner.active_site_run().has_value())
    {
        return;
    }

    auto& site_run = owner.active_site_run().value();
    if (!site_world_access::tile_coord_in_bounds(site_run, coord))
    {
        return;
    }

    const auto tile_count = site_world_access::tile_count(site_run);
    if (site_run.pending_tile_projection_update_mask.size() != tile_count)
    {
        site_run.pending_tile_projection_update_mask.assign(tile_count, 0U);
        site_run.pending_tile_projection_updates.clear();
    }

    const auto index = site_world_access::tile_index(site_run, coord);
    if (site_run.pending_tile_projection_update_mask[index] == 0U)
    {
        site_run.pending_tile_projection_update_mask[index] = 1U;
        site_run.pending_tile_projection_updates.push_back(coord);
    }

    site_run.pending_projection_update_flags |= SITE_PROJECTION_UPDATE_TILES;
}

void site_presentation_flush_if_dirty(
    GamePresentationCoordinator& owner,
    GamePresentationRuntimeContext& context)
{
    owner.active_context_ = &context;
    if (!owner.active_site_run().has_value())
    {
        return;
    }

    const auto dirty_flags = owner.active_site_run()->pending_projection_update_flags;
    if (dirty_flags == 0U)
    {
        return;
    }

    if ((dirty_flags & (SITE_PROJECTION_UPDATE_PHONE |
            SITE_PROJECTION_UPDATE_TASKS |
            SITE_PROJECTION_UPDATE_INVENTORY)) != 0U)
    {
        RuntimeInvocation invocation {
            owner.app_state(),
            owner.campaign(),
            owner.active_site_run(),
            owner.engine_messages(),
            owner.message_queue(),
            owner.fixed_step_seconds()};
        PhonePanelSystem system {};
        system.run(invocation);
    }

    const auto projected_dirty_flags = owner.active_site_run()->pending_projection_update_flags;
    if (projected_dirty_flags == 0U)
    {
        return;
    }

    owner.queue_site_delta_messages(projected_dirty_flags);

    if ((projected_dirty_flags & (SITE_PROJECTION_UPDATE_HUD |
            SITE_PROJECTION_UPDATE_WORKER |
            SITE_PROJECTION_UPDATE_WEATHER)) != 0U)
    {
        owner.queue_hud_state_message();
    }

    owner.active_site_run()->pending_projection_update_flags = 0U;
    owner.clear_pending_site_tile_projection_updates();
    owner.clear_pending_site_inventory_projection_updates();
}
}  // namespace gs1
