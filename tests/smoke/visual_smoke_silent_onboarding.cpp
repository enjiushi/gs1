#include "visual_smoke_silent_onboarding.h"

#include "content/defs/task_defs.h"
#include "content/defs/item_defs.h"
#include "content/defs/plant_defs.h"
#include "content/defs/structure_defs.h"
#include "smoke_log.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
using LiveStateSnapshot = SmokeEngineHost::LiveStateSnapshot;
using SiteSnapshotProjection = SmokeEngineHost::SiteSnapshotProjection;
using SitePlacementPreviewProjection = SmokeEngineHost::SitePlacementPreviewProjection;
using SiteInventorySlotProjection = SmokeEngineHost::SiteInventorySlotProjection;
using SiteTaskProjection = SmokeEngineHost::SiteTaskProjection;
using SiteTileProjection = SmokeEngineHost::SiteTileProjection;

constexpr std::uint32_t k_onboarding_task_template_min = 101U;
constexpr std::uint32_t k_onboarding_task_template_max = 111U;
constexpr std::uint32_t k_preview_flag_active = 1U << 0U;
constexpr std::uint32_t k_preview_flag_valid = 1U << 1U;

bool is_onboarding_task_template(std::uint32_t task_template_id) noexcept
{
    return task_template_id >= k_onboarding_task_template_min &&
        task_template_id <= k_onboarding_task_template_max;
}

bool is_active_site_action(const LiveStateSnapshot& snapshot) noexcept
{
    return snapshot.site_action.has_value() &&
        snapshot.site_action->action_kind != GS1_SITE_ACTION_NONE &&
        (snapshot.site_action->flags & GS1_SITE_ACTION_PRESENTATION_FLAG_ACTIVE) != 0U;
}

std::optional<SiteTaskProjection> find_onboarding_task_with_list_kind(
    const LiveStateSnapshot& snapshot,
    Gs1TaskPresentationListKind list_kind)
{
    if (!snapshot.active_site_snapshot.has_value())
    {
        return std::nullopt;
    }

    for (const auto& task : snapshot.active_site_snapshot->tasks)
    {
        if (is_onboarding_task_template(task.task_template_id) &&
            task.list_kind == list_kind)
        {
            return task;
        }
    }

    return std::nullopt;
}

std::optional<SiteTaskProjection> find_active_onboarding_task(const LiveStateSnapshot& snapshot)
{
    if (!snapshot.active_site_snapshot.has_value())
    {
        return std::nullopt;
    }

    for (const auto& preferred_kind : {
             GS1_TASK_PRESENTATION_LIST_ACCEPTED,
             GS1_TASK_PRESENTATION_LIST_VISIBLE,
             GS1_TASK_PRESENTATION_LIST_PENDING_CLAIM})
    {
        const auto task = find_onboarding_task_with_list_kind(snapshot, preferred_kind);
        if (task.has_value())
        {
            return task;
        }
    }

    return std::nullopt;
}

bool snapshot_has_nonclaimed_onboarding_task(const LiveStateSnapshot& snapshot) noexcept
{
    if (!snapshot.active_site_snapshot.has_value())
    {
        return false;
    }

    for (const auto& task : snapshot.active_site_snapshot->tasks)
    {
        if (is_onboarding_task_template(task.task_template_id) &&
            task.list_kind != GS1_TASK_PRESENTATION_LIST_CLAIMED)
        {
            return true;
        }
    }

    return false;
}

const char* app_state_label(Gs1AppState app_state) noexcept
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
        return "UNKNOWN";
    }
}

const char* onboarding_task_label(std::uint32_t task_template_id) noexcept
{
    switch (task_template_id)
    {
    case gs1::k_task_template_site1_onboarding_plant_checkerboard:
        return "Lay One Checkerboard";
    case gs1::k_task_template_site1_onboarding_craft_hammer:
        return "Craft a Hammer";
    case gs1::k_task_template_site1_onboarding_craft_storage_crate:
        return "Craft a Storage Crate";
    case gs1::k_task_template_site1_onboarding_buy_water:
        return "Buy Water";
    case gs1::k_task_template_site1_onboarding_buy_food:
        return "Buy Food";
    case gs1::k_task_template_site1_onboarding_craft_shovel:
        return "Craft a Shovel";
    case gs1::k_task_template_site1_onboarding_keep_starter_ephedra_stable:
        return "Hold the Starter Patch";
    case gs1::k_task_template_site1_onboarding_craft_camp_stove:
        return "Craft a Cook Plot";
    case gs1::k_task_template_site1_onboarding_build_camp_stove:
        return "Place the Cook Plot";
    case gs1::k_task_template_site1_onboarding_harvest_starter_ephedra:
        return "Harvest Desert Ephedra";
    case gs1::k_task_template_site1_onboarding_cook_ephedra_stew:
        return "Cook Ephedra Stew";
    default:
        return "Onboarding Task";
    }
}

std::uint64_t encode_inventory_transfer_owners_arg(
    std::uint32_t source_owner_id,
    std::uint32_t destination_owner_id) noexcept
{
    return static_cast<std::uint64_t>(source_owner_id) |
        (static_cast<std::uint64_t>(destination_owner_id) << 32U);
}

std::uint64_t encode_inventory_transfer_arg(
    Gs1InventoryContainerKind source_container_kind,
    std::uint16_t source_slot_index,
    Gs1InventoryContainerKind destination_container_kind,
    std::uint16_t quantity) noexcept
{
    return static_cast<std::uint64_t>(source_container_kind) |
        (static_cast<std::uint64_t>(source_slot_index & 0xffU) << 8U) |
        (static_cast<std::uint64_t>(destination_container_kind & 0xffU) << 16U) |
        (static_cast<std::uint64_t>(quantity) << 24U);
}

std::optional<std::pair<std::int32_t, std::int32_t>> find_structure_tile(
    const SiteSnapshotProjection& site_snapshot,
    std::uint32_t structure_id)
{
    for (const auto& tile : site_snapshot.tiles)
    {
        if (tile.structure_type_id == structure_id)
        {
            return std::pair {static_cast<std::int32_t>(tile.x), static_cast<std::int32_t>(tile.y)};
        }
    }

    return std::nullopt;
}

std::optional<std::pair<std::int32_t, std::int32_t>> find_harvestable_ephedra_tile(
    const SiteSnapshotProjection& site_snapshot)
{
    std::optional<std::pair<std::int32_t, std::int32_t>> best_tile {};
    float best_density = -1.0f;

    for (const auto& tile : site_snapshot.tiles)
    {
        if (tile.plant_type_id != gs1::k_plant_desert_ephedra)
        {
            continue;
        }

        if (tile.plant_density > best_density)
        {
            best_density = tile.plant_density;
            best_tile = std::pair {static_cast<std::int32_t>(tile.x), static_cast<std::int32_t>(tile.y)};
        }
    }

    return best_tile;
}

std::optional<std::uint32_t> find_delivery_box_storage_id(const SiteSnapshotProjection& site_snapshot)
{
    for (const auto& storage : site_snapshot.inventory_storages)
    {
        if ((storage.flags & GS1_INVENTORY_STORAGE_FLAG_DELIVERY_BOX) != 0U)
        {
            return storage.storage_id;
        }
    }

    return std::nullopt;
}

std::optional<std::uint32_t> find_storage_id_at_tile(
    const SiteSnapshotProjection& site_snapshot,
    std::int32_t tile_x,
    std::int32_t tile_y)
{
    for (const auto& storage : site_snapshot.inventory_storages)
    {
        if (storage.tile_x == tile_x && storage.tile_y == tile_y)
        {
            return storage.storage_id;
        }
    }

    return std::nullopt;
}

std::optional<std::uint32_t> find_structure_storage_id(
    const SiteSnapshotProjection& site_snapshot,
    std::uint32_t structure_id)
{
    const auto structure_tile = find_structure_tile(site_snapshot, structure_id);
    if (!structure_tile.has_value())
    {
        return std::nullopt;
    }

    return find_storage_id_at_tile(site_snapshot, structure_tile->first, structure_tile->second);
}

std::optional<SiteInventorySlotProjection> find_opened_storage_slot(
    const LiveStateSnapshot& snapshot,
    std::uint32_t storage_id,
    std::uint32_t item_id)
{
    if (!snapshot.active_site_snapshot.has_value() ||
        !snapshot.active_site_snapshot->opened_storage.has_value() ||
        snapshot.active_site_snapshot->opened_storage->storage_id != storage_id)
    {
        return std::nullopt;
    }

    for (const auto& slot : snapshot.active_site_snapshot->opened_storage->slots)
    {
        if (slot.item_id == item_id && slot.quantity > 0U)
        {
            return slot;
        }
    }

    return std::nullopt;
}

std::uint32_t worker_pack_item_quantity(
    const LiveStateSnapshot& snapshot,
    std::uint32_t item_id) noexcept
{
    if (!snapshot.active_site_snapshot.has_value())
    {
        return 0U;
    }

    std::uint32_t quantity = 0U;
    for (const auto& slot : snapshot.active_site_snapshot->worker_pack_slots)
    {
        if (slot.item_id == item_id)
        {
            quantity += slot.quantity;
        }
    }

    return quantity;
}

std::uint32_t resolve_worker_pack_storage_id(const LiveStateSnapshot& snapshot) noexcept
{
    if (!snapshot.active_site_snapshot.has_value())
    {
        return 1U;
    }

    for (const auto& slot : snapshot.active_site_snapshot->worker_pack_slots)
    {
        if (slot.storage_id != 0U)
        {
            return slot.storage_id;
        }
    }

    // The worker pack container is initialized before device storage containers,
    // so the bootstrap worker-pack storage id is deterministically the first id.
    return 1U;
}

std::optional<SitePlacementPreviewProjection> current_preview(const LiveStateSnapshot& snapshot)
{
    if (!snapshot.active_site_snapshot.has_value() ||
        !snapshot.active_site_snapshot->placement_preview.has_value())
    {
        return std::nullopt;
    }

    return snapshot.active_site_snapshot->placement_preview;
}

std::optional<std::uint32_t> find_buy_listing_id(
    const LiveStateSnapshot& snapshot,
    std::uint32_t item_id)
{
    if (!snapshot.active_site_snapshot.has_value())
    {
        return std::nullopt;
    }

    for (const auto& listing : snapshot.active_site_snapshot->phone_listings)
    {
        if (listing.listing_kind == GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM &&
            listing.item_or_unlockable_id == item_id &&
            listing.quantity > 0U)
        {
            return listing.listing_id;
        }
    }

    return std::nullopt;
}

bool craft_context_contains_output(
    const LiveStateSnapshot& snapshot,
    std::int32_t tile_x,
    std::int32_t tile_y,
    std::uint32_t output_item_id) noexcept
{
    if (!snapshot.active_site_snapshot.has_value() ||
        !snapshot.active_site_snapshot->craft_context.has_value())
    {
        return false;
    }

    const auto& craft_context = snapshot.active_site_snapshot->craft_context.value();
    if (craft_context.tile_x != tile_x || craft_context.tile_y != tile_y)
    {
        return false;
    }

    for (const auto& option : craft_context.options)
    {
        if (option.output_item_id == output_item_id)
        {
            return true;
        }
    }

    return false;
}

struct TileCandidate final
{
    std::int32_t x {0};
    std::int32_t y {0};
    std::int32_t sort_distance {0};
};

std::vector<TileCandidate> build_empty_tile_candidates(
    const SiteSnapshotProjection& site_snapshot,
    std::uint32_t footprint_width,
    std::uint32_t footprint_height)
{
    std::vector<TileCandidate> candidates {};

    const std::int32_t origin_x = site_snapshot.camp.has_value()
        ? site_snapshot.camp->tile_x
        : 0;
    const std::int32_t origin_y = site_snapshot.camp.has_value()
        ? site_snapshot.camp->tile_y
        : 0;

    for (std::int32_t y = 0; y < static_cast<std::int32_t>(site_snapshot.height); ++y)
    {
        for (std::int32_t x = 0; x < static_cast<std::int32_t>(site_snapshot.width); ++x)
        {
            if ((x + static_cast<std::int32_t>(footprint_width)) > static_cast<std::int32_t>(site_snapshot.width) ||
                (y + static_cast<std::int32_t>(footprint_height)) > static_cast<std::int32_t>(site_snapshot.height))
            {
                continue;
            }

            bool looks_empty = true;
            for (const auto& tile : site_snapshot.tiles)
            {
                const auto tile_x = static_cast<std::int32_t>(tile.x);
                const auto tile_y = static_cast<std::int32_t>(tile.y);
                if (tile_x < x || tile_x >= (x + static_cast<std::int32_t>(footprint_width)) ||
                    tile_y < y || tile_y >= (y + static_cast<std::int32_t>(footprint_height)))
                {
                    continue;
                }

                if (tile.plant_type_id != 0U || tile.structure_type_id != 0U)
                {
                    looks_empty = false;
                    break;
                }
            }

            if (!looks_empty)
            {
                continue;
            }

            candidates.push_back(TileCandidate {
                x,
                y,
                std::abs(x - origin_x) + std::abs(y - origin_y)});
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const TileCandidate& lhs, const TileCandidate& rhs) {
            if (lhs.sort_distance != rhs.sort_distance)
            {
                return lhs.sort_distance < rhs.sort_distance;
            }

            if (lhs.y != rhs.y)
            {
                return lhs.y < rhs.y;
            }

            return lhs.x < rhs.x;
        });
    return candidates;
}

struct PlacementAttemptState final
{
    Gs1SiteActionKind action_kind {GS1_SITE_ACTION_NONE};
    std::uint32_t item_id {0U};
    std::vector<TileCandidate> candidates {};
    std::size_t candidate_index {0U};
};

class SilentOnboardingController final
{
public:
    enum class TickResult : std::uint32_t
    {
        Running = 0,
        Succeeded = 1,
        Failed = 2
    };

    TickResult tick(SmokeEngineHost& host, std::string& out_error)
    {
        const auto snapshot = host.capture_live_state_snapshot();
        const std::uint64_t frame_number = host.frame_number();

        if (snapshot.current_app_state.has_value())
        {
            if (!last_logged_app_state_.has_value() ||
                last_logged_app_state_.value() != snapshot.current_app_state.value())
            {
                last_logged_app_state_ = snapshot.current_app_state;
                smoke_log::infof(
                    "[HEADLESS] frame=%llu state=%s\n",
                    static_cast<unsigned long long>(frame_number),
                    app_state_label(snapshot.current_app_state.value()));
            }
        }

        if (snapshot.current_app_state == std::optional {GS1_APP_STATE_SITE_RESULT})
        {
            out_error = "Site attempt ended before onboarding finished.";
            return TickResult::Failed;
        }

        if (!snapshot.current_app_state.has_value())
        {
            return TickResult::Running;
        }

        switch (snapshot.current_app_state.value())
        {
        case GS1_APP_STATE_MAIN_MENU:
            clear_task_state();
            maybe_queue_start_new_campaign(host);
            return TickResult::Running;

        case GS1_APP_STATE_REGIONAL_MAP:
            clear_task_state();
            maybe_queue_site_selection_or_start(host, snapshot);
            return TickResult::Running;

        case GS1_APP_STATE_SITE_LOADING:
        case GS1_APP_STATE_CAMPAIGN_SETUP:
        case GS1_APP_STATE_BOOT:
        case GS1_APP_STATE_SITE_PAUSED:
        case GS1_APP_STATE_CAMPAIGN_END:
            return TickResult::Running;

        case GS1_APP_STATE_SITE_ACTIVE:
            return tick_site_active(host, snapshot, out_error);

        default:
            return TickResult::Running;
        }
    }

private:
    void clear_task_state() noexcept
    {
        pending_placement_.reset();
        saw_onboarding_task_ = false;
        last_logged_task_template_id_ = 0U;
    }

    void maybe_queue_start_new_campaign(SmokeEngineHost& host)
    {
        if (!can_issue_command(host.frame_number()))
        {
            return;
        }

        Gs1UiAction action {};
        action.type = GS1_UI_ACTION_START_NEW_CAMPAIGN;
        host.queue_ui_action(action);
        note_command(host.frame_number(), "start_new_campaign");
    }

    void maybe_queue_site_selection_or_start(
        SmokeEngineHost& host,
        const LiveStateSnapshot& snapshot)
    {
        if (!can_issue_command(host.frame_number()))
        {
            return;
        }

        if (snapshot.selected_site_id.value_or(0U) != 1U)
        {
            Gs1UiAction action {};
            action.type = GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE;
            action.target_id = 1U;
            host.queue_ui_action(action);
            note_command(host.frame_number(), "select_site_1");
            return;
        }

        Gs1UiAction action {};
        action.type = GS1_UI_ACTION_START_SITE_ATTEMPT;
        action.target_id = 1U;
        host.queue_ui_action(action);
        note_command(host.frame_number(), "start_site_1");
    }

    TickResult tick_site_active(
        SmokeEngineHost& host,
        const LiveStateSnapshot& snapshot,
        std::string& out_error)
    {
        if (!snapshot.active_site_snapshot.has_value())
        {
            return TickResult::Running;
        }

        const auto& site_snapshot = snapshot.active_site_snapshot.value();
        if (snapshot_has_nonclaimed_onboarding_task(snapshot))
        {
            saw_onboarding_task_ = true;
        }

        const auto active_task = find_active_onboarding_task(snapshot);
        if (active_task.has_value() &&
            active_task->task_template_id != last_logged_task_template_id_)
        {
            last_logged_task_template_id_ = active_task->task_template_id;
            smoke_log::infof(
                "[HEADLESS] frame=%llu task=\"%s\" progress=%u/%u\n",
                static_cast<unsigned long long>(host.frame_number()),
                onboarding_task_label(active_task->task_template_id),
                active_task->current_progress,
                active_task->target_progress);
        }

        if (saw_onboarding_task_ &&
            !snapshot_has_nonclaimed_onboarding_task(snapshot) &&
            !is_active_site_action(snapshot))
        {
            smoke_log::infof(
                "[HEADLESS] frame=%llu onboarding complete\n",
                static_cast<unsigned long long>(host.frame_number()));
            return TickResult::Succeeded;
        }

        const auto pending_claim_task =
            find_onboarding_task_with_list_kind(snapshot, GS1_TASK_PRESENTATION_LIST_PENDING_CLAIM);
        if (pending_claim_task.has_value())
        {
            pending_placement_.reset();
            maybe_queue_claim_task_reward(host, pending_claim_task.value());
            return TickResult::Running;
        }

        const auto visible_task =
            find_onboarding_task_with_list_kind(snapshot, GS1_TASK_PRESENTATION_LIST_VISIBLE);
        if (visible_task.has_value())
        {
            maybe_queue_accept_task(host, visible_task.value());
            return TickResult::Running;
        }

        const auto accepted_task =
            find_onboarding_task_with_list_kind(snapshot, GS1_TASK_PRESENTATION_LIST_ACCEPTED);
        if (!accepted_task.has_value())
        {
            return TickResult::Running;
        }

        const auto preview = current_preview(snapshot);
        if (preview.has_value() &&
            accepted_task->task_template_id != gs1::k_task_template_site1_onboarding_plant_checkerboard &&
            accepted_task->task_template_id != gs1::k_task_template_site1_onboarding_build_camp_stove)
        {
            maybe_cancel_placement_mode(host);
            return TickResult::Running;
        }

        if (is_active_site_action(snapshot))
        {
            return TickResult::Running;
        }

        switch (accepted_task->task_template_id)
        {
        case gs1::k_task_template_site1_onboarding_plant_checkerboard:
            return handle_checkerboard_task(host, snapshot, site_snapshot, out_error);

        case gs1::k_task_template_site1_onboarding_craft_hammer:
            handle_craft_task(host, snapshot, site_snapshot, gs1::k_structure_workbench, gs1::k_item_hammer);
            return TickResult::Running;

        case gs1::k_task_template_site1_onboarding_craft_storage_crate:
            handle_craft_task(
                host,
                snapshot,
                site_snapshot,
                gs1::k_structure_workbench,
                gs1::k_item_storage_crate_kit);
            return TickResult::Running;

        case gs1::k_task_template_site1_onboarding_buy_water:
            handle_buy_task(host, snapshot, gs1::k_item_water_container);
            return TickResult::Running;

        case gs1::k_task_template_site1_onboarding_buy_food:
            handle_buy_task(host, snapshot, gs1::k_item_food_pack);
            return TickResult::Running;

        case gs1::k_task_template_site1_onboarding_craft_shovel:
            handle_craft_task(host, snapshot, site_snapshot, gs1::k_structure_workbench, gs1::k_item_shovel);
            return TickResult::Running;

        case gs1::k_task_template_site1_onboarding_keep_starter_ephedra_stable:
            return TickResult::Running;

        case gs1::k_task_template_site1_onboarding_craft_camp_stove:
            handle_craft_task(
                host,
                snapshot,
                site_snapshot,
                gs1::k_structure_workbench,
                gs1::k_item_camp_stove_kit);
            return TickResult::Running;

        case gs1::k_task_template_site1_onboarding_build_camp_stove:
            return handle_build_camp_stove_task(host, snapshot, site_snapshot, out_error);

        case gs1::k_task_template_site1_onboarding_harvest_starter_ephedra:
            return handle_harvest_task(host, site_snapshot, out_error);

        case gs1::k_task_template_site1_onboarding_cook_ephedra_stew:
            handle_craft_task(host, snapshot, site_snapshot, gs1::k_structure_camp_stove, gs1::k_item_ephedra_stew);
            return TickResult::Running;

        default:
            out_error = "Unknown onboarding task template id.";
            return TickResult::Failed;
        }
    }

    void maybe_queue_claim_task_reward(SmokeEngineHost& host, const SiteTaskProjection& task)
    {
        if (!can_issue_command(host.frame_number()))
        {
            return;
        }

        Gs1UiAction action {};
        action.type = GS1_UI_ACTION_CLAIM_TASK_REWARD;
        action.target_id = task.task_instance_id;
        action.arg0 = 0U;
        host.queue_ui_action(action);
        note_command(host.frame_number(), "claim_task_reward");
    }

    void maybe_queue_accept_task(SmokeEngineHost& host, const SiteTaskProjection& task)
    {
        if (!can_issue_command(host.frame_number()))
        {
            return;
        }

        Gs1UiAction action {};
        action.type = GS1_UI_ACTION_ACCEPT_TASK;
        action.target_id = task.task_instance_id;
        host.queue_ui_action(action);
        note_command(host.frame_number(), "accept_task");
    }

    void maybe_cancel_placement_mode(SmokeEngineHost& host)
    {
        if (!can_issue_command(host.frame_number()))
        {
            return;
        }

        Gs1HostEventSiteActionCancelData cancel {};
        cancel.flags = GS1_SITE_ACTION_CANCEL_FLAG_PLACEMENT_MODE;
        host.queue_site_action_cancel(cancel);
        pending_placement_.reset();
        note_command(host.frame_number(), "cancel_placement_mode");
    }

    TickResult handle_checkerboard_task(
        SmokeEngineHost& host,
        const LiveStateSnapshot& snapshot,
        const SiteSnapshotProjection& site_snapshot,
        std::string& out_error)
    {
        const auto worker_pack_quantity =
            worker_pack_item_quantity(snapshot, gs1::k_item_basic_straw_checkerboard);
        if (worker_pack_quantity == 0U)
        {
            ensure_item_in_worker_pack(
                host,
                snapshot,
                find_delivery_box_storage_id(site_snapshot),
                gs1::k_item_basic_straw_checkerboard,
                "open_delivery_box_snapshot");
            return TickResult::Running;
        }

        return handle_placement_task(
            host,
            snapshot,
            site_snapshot,
            GS1_SITE_ACTION_PLANT,
            gs1::k_item_basic_straw_checkerboard,
            2U,
            2U,
            out_error);
    }

    TickResult handle_build_camp_stove_task(
        SmokeEngineHost& host,
        const LiveStateSnapshot& snapshot,
        const SiteSnapshotProjection& site_snapshot,
        std::string& out_error)
    {
        if (worker_pack_item_quantity(snapshot, gs1::k_item_camp_stove_kit) == 0U)
        {
            ensure_item_in_worker_pack(
                host,
                snapshot,
                find_structure_storage_id(site_snapshot, gs1::k_structure_workbench),
                gs1::k_item_camp_stove_kit,
                "open_workbench_storage_snapshot");
            return TickResult::Running;
        }

        return handle_placement_task(
            host,
            snapshot,
            site_snapshot,
            GS1_SITE_ACTION_BUILD,
            gs1::k_item_camp_stove_kit,
            1U,
            1U,
            out_error);
    }

    TickResult handle_harvest_task(
        SmokeEngineHost& host,
        const SiteSnapshotProjection& site_snapshot,
        std::string& out_error)
    {
        const auto tile = find_harvestable_ephedra_tile(site_snapshot);
        if (!tile.has_value())
        {
            out_error = "Could not find a Desert Ephedra tile to harvest.";
            return TickResult::Failed;
        }

        if (!can_issue_command(host.frame_number()))
        {
            return TickResult::Running;
        }

        Gs1HostEventSiteActionRequestData action {};
        action.action_kind = GS1_SITE_ACTION_HARVEST;
        action.quantity = 1U;
        action.target_tile_x = tile->first;
        action.target_tile_y = tile->second;
        host.queue_site_action_request(action);
        note_command(host.frame_number(), "harvest_ephedra");
        return TickResult::Running;
    }

    void handle_buy_task(
        SmokeEngineHost& host,
        const LiveStateSnapshot& snapshot,
        std::uint32_t item_id)
    {
        if (!can_issue_command(host.frame_number()))
        {
            return;
        }

        const auto listing_id = find_buy_listing_id(snapshot, item_id);
        if (listing_id.has_value())
        {
            Gs1UiAction action {};
            action.type = GS1_UI_ACTION_BUY_PHONE_LISTING;
            action.target_id = listing_id.value();
            action.arg0 = 1U;
            host.queue_ui_action(action);
            note_command(host.frame_number(), "buy_phone_listing");
            return;
        }

        Gs1UiAction section_action {};
        section_action.type = GS1_UI_ACTION_SET_PHONE_PANEL_SECTION;
        section_action.arg0 = GS1_PHONE_PANEL_SECTION_BUY;
        host.queue_ui_action(section_action);
        note_command(host.frame_number(), "open_phone_buy_section");
    }

    void handle_craft_task(
        SmokeEngineHost& host,
        const LiveStateSnapshot& snapshot,
        const SiteSnapshotProjection& site_snapshot,
        std::uint32_t structure_id,
        std::uint32_t output_item_id)
    {
        const auto station_tile = find_structure_tile(site_snapshot, structure_id);
        if (!station_tile.has_value())
        {
            return;
        }

        if (!can_issue_command(host.frame_number()))
        {
            return;
        }

        if (!craft_context_contains_output(
                snapshot,
                station_tile->first,
                station_tile->second,
                output_item_id))
        {
            Gs1HostEventSiteContextRequestData request {};
            request.tile_x = station_tile->first;
            request.tile_y = station_tile->second;
            host.queue_site_context_request(request);
            note_command(host.frame_number(), "request_craft_context");
            return;
        }

        Gs1HostEventSiteActionRequestData action {};
        action.action_kind = GS1_SITE_ACTION_CRAFT;
        action.flags = GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM;
        action.quantity = 1U;
        action.target_tile_x = station_tile->first;
        action.target_tile_y = station_tile->second;
        action.item_id = output_item_id;
        host.queue_site_action_request(action);
        note_command(host.frame_number(), "craft_item");
    }

    void ensure_item_in_worker_pack(
        SmokeEngineHost& host,
        const LiveStateSnapshot& snapshot,
        std::optional<std::uint32_t> storage_id,
        std::uint32_t item_id,
        const char* open_command_label)
    {
        if (!can_issue_command(host.frame_number()))
        {
            return;
        }

        if (!storage_id.has_value())
        {
            return;
        }

        const auto storage_slot =
            find_opened_storage_slot(snapshot, storage_id.value(), item_id);
        if (!storage_slot.has_value())
        {
            Gs1HostEventSiteStorageViewData request {};
            request.storage_id = storage_id.value();
            request.event_kind = GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT;
            host.queue_site_storage_view(request);
            note_command(host.frame_number(), open_command_label);
            return;
        }

        Gs1UiAction action {};
        action.type = GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM;
        action.target_id = storage_slot->item_instance_id != 0U
            ? storage_slot->item_instance_id
            : storage_slot->item_id;
        action.arg0 = encode_inventory_transfer_arg(
            storage_slot->container_kind,
            static_cast<std::uint16_t>(storage_slot->slot_index),
            GS1_INVENTORY_CONTAINER_WORKER_PACK,
            0U);
        action.arg1 = encode_inventory_transfer_owners_arg(
            storage_slot->storage_id,
            resolve_worker_pack_storage_id(snapshot));
        host.queue_ui_action(action);
        note_command(host.frame_number(), "transfer_item_to_worker_pack");
    }

    TickResult handle_placement_task(
        SmokeEngineHost& host,
        const LiveStateSnapshot& snapshot,
        const SiteSnapshotProjection& site_snapshot,
        Gs1SiteActionKind action_kind,
        std::uint32_t item_id,
        std::uint32_t footprint_width,
        std::uint32_t footprint_height,
        std::string& out_error)
    {
        auto preview = current_preview(snapshot);
        if (!pending_placement_.has_value() ||
            pending_placement_->action_kind != action_kind ||
            pending_placement_->item_id != item_id)
        {
            pending_placement_ = PlacementAttemptState {
                action_kind,
                item_id,
                build_empty_tile_candidates(site_snapshot, footprint_width, footprint_height),
                0U};
        }

        if (pending_placement_->candidates.empty())
        {
            out_error = "No empty tile candidates were available for placement.";
            return TickResult::Failed;
        }

        const auto& candidate = pending_placement_->candidates[pending_placement_->candidate_index];
        if (!preview.has_value() ||
            preview->action_kind != action_kind ||
            preview->item_id != item_id ||
            (preview->flags & k_preview_flag_active) == 0U)
        {
            if (!can_issue_command(host.frame_number()))
            {
                return TickResult::Running;
            }

            Gs1HostEventSiteActionRequestData action {};
            action.action_kind = action_kind;
            action.flags = static_cast<std::uint8_t>(
                GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM |
                GS1_SITE_ACTION_REQUEST_FLAG_DEFERRED_TARGET_SELECTION);
            action.quantity = 1U;
            action.target_tile_x = candidate.x;
            action.target_tile_y = candidate.y;
            action.item_id = item_id;
            host.queue_site_action_request(action);
            note_command(host.frame_number(), "begin_placement_mode");
            return TickResult::Running;
        }

        if ((preview->flags & k_preview_flag_valid) != 0U)
        {
            if (!can_issue_command(host.frame_number()))
            {
                return TickResult::Running;
            }

            Gs1HostEventSiteActionRequestData action {};
            action.action_kind = action_kind;
            action.flags = GS1_SITE_ACTION_REQUEST_FLAG_HAS_ITEM;
            action.quantity = 1U;
            action.target_tile_x = preview->tile_x;
            action.target_tile_y = preview->tile_y;
            action.item_id = item_id;
            host.queue_site_action_request(action);
            note_command(host.frame_number(), "confirm_placement");
            return TickResult::Running;
        }

        if (preview->tile_x != candidate.x || preview->tile_y != candidate.y)
        {
            if (!can_issue_command(host.frame_number()))
            {
                return TickResult::Running;
            }

            Gs1HostEventSiteContextRequestData request {};
            request.tile_x = candidate.x;
            request.tile_y = candidate.y;
            host.queue_site_context_request(request);
            note_command(host.frame_number(), "move_placement_cursor");
            return TickResult::Running;
        }

        if ((pending_placement_->candidate_index + 1U) >= pending_placement_->candidates.size())
        {
            out_error = "Placement preview rejected every candidate tile.";
            return TickResult::Failed;
        }

        pending_placement_->candidate_index += 1U;
        return TickResult::Running;
    }

    bool can_issue_command(std::uint64_t frame_number) const noexcept
    {
        return frame_number >= next_command_frame_;
    }

    void note_command(std::uint64_t frame_number, const char* label)
    {
        const char* message = label;
        if (std::string_view {label} == "start_new_campaign")
        {
            message = "player starts a new campaign";
        }
        else if (std::string_view {label} == "select_site_1")
        {
            message = "player selects Site 1";
        }
        else if (std::string_view {label} == "start_site_1")
        {
            message = "player enters Site 1";
        }
        else if (std::string_view {label} == "open_delivery_box_snapshot")
        {
            message = "player opens the delivery crate";
        }
        else if (std::string_view {label} == "open_workbench_storage_snapshot")
        {
            message = "player opens the workbench storage";
        }
        else if (std::string_view {label} == "transfer_item_to_worker_pack")
        {
            message = "player moves an item into the worker pack";
        }
        else if (std::string_view {label} == "begin_placement_mode")
        {
            message = "player starts placing an item";
        }
        else if (std::string_view {label} == "move_placement_cursor")
        {
            message = "player adjusts the placement target";
        }
        else if (std::string_view {label} == "confirm_placement")
        {
            message = "player confirms the placement";
        }
        else if (std::string_view {label} == "claim_task_reward")
        {
            message = "player claims a task reward";
        }
        else if (std::string_view {label} == "accept_task")
        {
            message = "player accepts a task";
        }
        else if (std::string_view {label} == "cancel_placement_mode")
        {
            message = "player closes placement mode";
        }
        else if (std::string_view {label} == "request_craft_context")
        {
            message = "player checks a crafting station";
        }
        else if (std::string_view {label} == "craft_item")
        {
            message = "player starts crafting";
        }
        else if (std::string_view {label} == "buy_phone_listing")
        {
            message = "player buys a phone-market item";
        }
        else if (std::string_view {label} == "open_phone_buy_section")
        {
            message = "player opens the phone market";
        }
        else if (std::string_view {label} == "harvest_ephedra")
        {
            message = "player harvests Desert Ephedra";
        }

        smoke_log::infof(
            "[HEADLESS] frame=%llu action=%s\n",
            static_cast<unsigned long long>(frame_number),
            message);
        next_command_frame_ = frame_number + 20U;
    }

private:
    std::uint64_t next_command_frame_ {0U};
    bool saw_onboarding_task_ {false};
    std::optional<Gs1AppState> last_logged_app_state_ {};
    std::uint32_t last_logged_task_template_id_ {0U};
    std::optional<PlacementAttemptState> pending_placement_ {};
};
}  // namespace

int run_visual_smoke_silent_onboarding(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    SmokeEngineHost::LogMode log_mode,
    const VisualSmokeSilentOnboardingOptions& options)
{
    SmokeEngineHost host {api, runtime, log_mode};
    SilentOnboardingController controller {};
    std::string error {};

    for (std::uint32_t frame = 0U; frame < options.max_frames; ++frame)
    {
        host.update(options.frame_delta_seconds);

        const auto result = controller.tick(host, error);
        if (result == SilentOnboardingController::TickResult::Succeeded)
        {
            return 0;
        }

        if (result == SilentOnboardingController::TickResult::Failed)
        {
            smoke_log::errorf("[HEADLESS] %s\n", error.c_str());
            return 1;
        }
    }

    smoke_log::errorf(
        "[HEADLESS] Timed out after %u frames while waiting for Site 1 onboarding to complete.\n",
        options.max_frames);
    return 1;
}
