#include "smoke_engine_host.h"
#include "content/defs/item_defs.h"

#include <string>

namespace
{
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

const char* ui_element_type_name(Gs1UiElementType element_type)
{
    switch (element_type)
    {
    case GS1_UI_ELEMENT_BUTTON:
        return "BUTTON";
    case GS1_UI_ELEMENT_LABEL:
        return "LABEL";
    case GS1_UI_ELEMENT_NONE:
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
    case GS1_UI_ACTION_USE_INVENTORY_ITEM:
        return "USE_INVENTORY_ITEM";
    case GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM:
        return "TRANSFER_INVENTORY_ITEM";
    case GS1_UI_ACTION_HIRE_CONTRACTOR:
        return "HIRE_CONTRACTOR";
    case GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE:
        return "PURCHASE_SITE_UNLOCKABLE";
    default:
        return "NONE";
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

const char* weather_phase_name(Gs1WeatherEventPhase phase)
{
    switch (phase)
    {
    case GS1_WEATHER_EVENT_PHASE_NONE:
        return "NONE";
    case GS1_WEATHER_EVENT_PHASE_WARNING:
        return "WARNING";
    case GS1_WEATHER_EVENT_PHASE_BUILD:
        return "BUILD";
    case GS1_WEATHER_EVENT_PHASE_PEAK:
        return "PEAK";
    case GS1_WEATHER_EVENT_PHASE_DECAY:
        return "DECAY";
    case GS1_WEATHER_EVENT_PHASE_AFTERMATH:
        return "AFTERMATH";
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

const char* inventory_container_name(Gs1InventoryContainerKind kind)
{
    switch (kind)
    {
    case GS1_INVENTORY_CONTAINER_CAMP_STORAGE:
        return "CAMP_STORAGE";
    case GS1_INVENTORY_CONTAINER_WORKER_PACK:
    default:
        return "WORKER_PACK";
    }
}

const char* task_list_kind_name(Gs1TaskPresentationListKind kind)
{
    switch (kind)
    {
    case GS1_TASK_PRESENTATION_LIST_ACCEPTED:
        return "ACCEPTED";
    case GS1_TASK_PRESENTATION_LIST_COMPLETED:
        return "COMPLETED";
    case GS1_TASK_PRESENTATION_LIST_VISIBLE:
    default:
        return "VISIBLE";
    }
}

const char* phone_listing_kind_name(Gs1PhoneListingPresentationKind kind)
{
    switch (kind)
    {
    case GS1_PHONE_LISTING_PRESENTATION_SELL_ITEM:
        return "SELL_ITEM";
    case GS1_PHONE_LISTING_PRESENTATION_HIRE_CONTRACTOR:
        return "HIRE_CONTRACTOR";
    case GS1_PHONE_LISTING_PRESENTATION_PURCHASE_UNLOCKABLE:
        return "PURCHASE_UNLOCKABLE";
    case GS1_PHONE_LISTING_PRESENTATION_BUY_ITEM:
    default:
        return "BUY_ITEM";
    }
}

void append_json_string(std::string& json, std::string_view value)
{
    json.push_back('"');

    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
            json += "\\\\";
            break;
        case '"':
            json += "\\\"";
            break;
        case '\n':
            json += "\\n";
            break;
        case '\r':
            json += "\\r";
            break;
        case '\t':
            json += "\\t";
            break;
        case '<':
            json += "\\u003c";
            break;
        case '>':
            json += "\\u003e";
            break;
        case '&':
            json += "\\u0026";
            break;
        default:
            json.push_back(ch);
            break;
        }
    }

    json.push_back('"');
}

void append_optional_app_state_json(std::string& json, const std::optional<Gs1AppState>& value)
{
    if (!value.has_value())
    {
        json += "null";
        return;
    }

    append_json_string(json, app_state_name(value.value()));
}

void append_optional_u32_json(std::string& json, const std::optional<std::uint32_t>& value)
{
    if (!value.has_value())
    {
        json += "null";
        return;
    }

    json += std::to_string(value.value());
}

std::string_view item_display_name(std::uint32_t item_id)
{
    const auto* item_def = gs1::find_item_def(gs1::ItemId {item_id});
    if (item_def == nullptr)
    {
        return {};
    }

    return item_def->display_name;
}

void append_bool_json(std::string& json, bool value)
{
    json += value ? "true" : "false";
}

void append_ui_action_json(std::string& json, const Gs1UiAction& action)
{
    json += "{\"type\":";
    append_json_string(json, ui_action_name(action.type));
    json += ",\"targetId\":";
    json += std::to_string(action.target_id);
    json += ",\"arg0\":";
    json += std::to_string(action.arg0);
    json += ",\"arg1\":";
    json += std::to_string(action.arg1);
    json += '}';
}

void append_command_entries_json(std::string& json, const std::vector<std::string>& entries)
{
    json += '[';
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        if (index > 0U)
        {
            json.push_back(',');
        }
        append_json_string(json, entries[index]);
    }
    json += ']';
}

void append_ui_setups_json(std::string& json, const std::vector<SmokeEngineHost::ActiveUiSetup>& setups)
{
    json += '[';
    for (std::size_t setup_index = 0; setup_index < setups.size(); ++setup_index)
    {
        const auto& setup = setups[setup_index];
        if (setup_index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"setupId\":";
        append_json_string(json, ui_setup_name(setup.setup_id));
        json += ",\"presentationType\":";
        append_json_string(json, ui_setup_presentation_type_name(setup.presentation_type));
        json += ",\"contextId\":";
        json += std::to_string(setup.context_id);
        json += ",\"elements\":[";

        for (std::size_t element_index = 0; element_index < setup.elements.size(); ++element_index)
        {
            const auto& element = setup.elements[element_index];
            if (element_index > 0U)
            {
                json.push_back(',');
            }

            json += "{\"elementId\":";
            json += std::to_string(element.element_id);
            json += ",\"elementType\":";
            append_json_string(json, ui_element_type_name(element.element_type));
            json += ",\"flags\":";
            json += std::to_string(element.flags);
            json += ",\"text\":";
            append_json_string(json, element.text);
            json += ",\"action\":";
            append_ui_action_json(json, element.action);
            json += '}';
        }

        json += "]}";
    }
    json += ']';
}

void append_regional_map_json(
    std::string& json,
    const std::vector<SmokeEngineHost::RegionalMapSiteProjection>& sites,
    const std::vector<SmokeEngineHost::RegionalMapLinkProjection>& links)
{
    json += "{\"sites\":[";
    for (std::size_t site_index = 0; site_index < sites.size(); ++site_index)
    {
        const auto& site = sites[site_index];
        if (site_index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"siteId\":";
        json += std::to_string(site.site_id);
        json += ",\"siteState\":";
        append_json_string(json, site_state_name(site.site_state));
        json += ",\"siteArchetypeId\":";
        json += std::to_string(site.site_archetype_id);
        json += ",\"flags\":";
        json += std::to_string(site.flags);
        json += ",\"mapX\":";
        json += std::to_string(site.map_x);
        json += ",\"mapY\":";
        json += std::to_string(site.map_y);
        json += ",\"supportPackageId\":";
        json += std::to_string(site.support_package_id);
        json += ",\"supportPreviewMask\":";
        json += std::to_string(site.support_preview_mask);
        json += '}';
    }
    json += "],\"links\":[";
    for (std::size_t link_index = 0; link_index < links.size(); ++link_index)
    {
        const auto& link = links[link_index];
        if (link_index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"fromSiteId\":";
        json += std::to_string(link.from_site_id);
        json += ",\"toSiteId\":";
        json += std::to_string(link.to_site_id);
        json += ",\"flags\":";
        json += std::to_string(link.flags);
        json += '}';
    }
    json += "]}";
}

void append_site_bootstrap_json(std::string& json, const std::optional<SmokeEngineHost::SiteSnapshotProjection>& snapshot)
{
    if (!snapshot.has_value())
    {
        json += "null";
        return;
    }

    const auto& site_snapshot = snapshot.value();
    json += "{\"siteId\":";
    json += std::to_string(site_snapshot.site_id);
    json += ",\"siteArchetypeId\":";
    json += std::to_string(site_snapshot.site_archetype_id);
    json += ",\"width\":";
    json += std::to_string(site_snapshot.width);
    json += ",\"height\":";
    json += std::to_string(site_snapshot.height);
    json += ",\"tiles\":[";

    for (std::size_t tile_index = 0; tile_index < site_snapshot.tiles.size(); ++tile_index)
    {
        const auto& tile = site_snapshot.tiles[tile_index];
        if (tile_index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"x\":";
        json += std::to_string(tile.x);
        json += ",\"y\":";
        json += std::to_string(tile.y);
        json += ",\"terrainTypeId\":";
        json += std::to_string(tile.terrain_type_id);
        json += ",\"plantTypeId\":";
        json += std::to_string(tile.plant_type_id);
        json += ",\"structureTypeId\":";
        json += std::to_string(tile.structure_type_id);
        json += ",\"groundCoverTypeId\":";
        json += std::to_string(tile.ground_cover_type_id);
        json += ",\"plantDensity\":";
        json += std::to_string(tile.plant_density);
        json += ",\"sandBurial\":";
        json += std::to_string(tile.sand_burial);
        json += '}';
    }
    json += "]";

    json += ",\"camp\":";
    if (!site_snapshot.camp.has_value())
    {
        json += "null";
    }
    else
    {
        const auto& camp = site_snapshot.camp.value();
        json += "{\"tileX\":";
        json += std::to_string(camp.tile_x);
        json += ",\"tileY\":";
        json += std::to_string(camp.tile_y);
        json += ",\"durabilityNormalized\":";
        json += std::to_string(camp.durability_normalized);
        json += ",\"flags\":";
        json += std::to_string(camp.flags);
        json += '}';
    }

    json += '}';
}

void append_site_worker_json(std::string& json, const SmokeEngineHost::SiteSnapshotProjection& site_snapshot)
{
    json += "\"worker\":";
    if (!site_snapshot.worker.has_value())
    {
        json += "null";
        return;
    }

    const auto& worker = site_snapshot.worker.value();
    json += "{\"tileX\":";
    json += std::to_string(worker.tile_x);
    json += ",\"tileY\":";
    json += std::to_string(worker.tile_y);
    json += ",\"facingDegrees\":";
    json += std::to_string(worker.facing_degrees);
    json += ",\"healthNormalized\":";
    json += std::to_string(worker.health_normalized);
    json += ",\"hydrationNormalized\":";
    json += std::to_string(worker.hydration_normalized);
    json += ",\"energyNormalized\":";
    json += std::to_string(worker.energy_normalized);
    json += ",\"flags\":";
    json += std::to_string(worker.flags);
    json += ",\"currentActionKind\":";
    json += std::to_string(worker.current_action_kind);
    json += '}';
}

void append_site_camp_state_json(std::string& json, const SmokeEngineHost::SiteSnapshotProjection& site_snapshot)
{
    json += "\"camp\":";
    if (!site_snapshot.camp.has_value())
    {
        json += "null";
        return;
    }

    const auto& camp = site_snapshot.camp.value();
    json += "{\"tileX\":";
    json += std::to_string(camp.tile_x);
    json += ",\"tileY\":";
    json += std::to_string(camp.tile_y);
    json += ",\"durabilityNormalized\":";
    json += std::to_string(camp.durability_normalized);
    json += ",\"flags\":";
    json += std::to_string(camp.flags);
    json += '}';
}

void append_site_weather_json(std::string& json, const SmokeEngineHost::SiteSnapshotProjection& site_snapshot)
{
    json += "\"weather\":";
    if (!site_snapshot.weather.has_value())
    {
        json += "null";
        return;
    }

    const auto& weather = site_snapshot.weather.value();
    json += "{\"heat\":";
    json += std::to_string(weather.heat);
    json += ",\"wind\":";
    json += std::to_string(weather.wind);
    json += ",\"dust\":";
    json += std::to_string(weather.dust);
    json += ",\"eventTemplateId\":";
    json += std::to_string(weather.event_template_id);
    json += ",\"eventPhase\":";
    append_json_string(json, weather_phase_name(weather.event_phase));
    json += ",\"phaseMinutesRemaining\":";
    json += std::to_string(weather.phase_minutes_remaining);
    json += '}';
}

void append_inventory_slots_json(std::string& json, const SmokeEngineHost::SiteSnapshotProjection& site_snapshot)
{
    json += "\"inventorySlots\":[";
    for (std::size_t index = 0; index < site_snapshot.inventory_slots.size(); ++index)
    {
        const auto& slot = site_snapshot.inventory_slots[index];
        if (index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"containerKind\":";
        append_json_string(json, inventory_container_name(slot.container_kind));
        json += ",\"slotIndex\":";
        json += std::to_string(slot.slot_index);
        json += ",\"itemId\":";
        json += std::to_string(slot.item_id);
        json += ",\"itemName\":";
        const auto display_name = item_display_name(slot.item_id);
        if (display_name.empty())
        {
            json += "null";
        }
        else
        {
            append_json_string(json, display_name);
        }
        json += ",\"quantity\":";
        json += std::to_string(slot.quantity);
        json += ",\"condition\":";
        json += std::to_string(slot.condition);
        json += ",\"freshness\":";
        json += std::to_string(slot.freshness);
        json += ",\"flags\":";
        json += std::to_string(slot.flags);
        json += '}';
    }
    json += "]";
}

void append_site_tasks_json(std::string& json, const SmokeEngineHost::SiteSnapshotProjection& site_snapshot)
{
    json += "\"tasks\":[";
    for (std::size_t index = 0; index < site_snapshot.tasks.size(); ++index)
    {
        const auto& task = site_snapshot.tasks[index];
        if (index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"taskInstanceId\":";
        json += std::to_string(task.task_instance_id);
        json += ",\"taskTemplateId\":";
        json += std::to_string(task.task_template_id);
        json += ",\"publisherFactionId\":";
        json += std::to_string(task.publisher_faction_id);
        json += ",\"currentProgress\":";
        json += std::to_string(task.current_progress);
        json += ",\"targetProgress\":";
        json += std::to_string(task.target_progress);
        json += ",\"listKind\":";
        append_json_string(json, task_list_kind_name(task.list_kind));
        json += ",\"flags\":";
        json += std::to_string(task.flags);
        json += '}';
    }
    json += "]";
}

void append_site_phone_listings_json(std::string& json, const SmokeEngineHost::SiteSnapshotProjection& site_snapshot)
{
    json += "\"phoneListings\":[";
    for (std::size_t index = 0; index < site_snapshot.phone_listings.size(); ++index)
    {
        const auto& listing = site_snapshot.phone_listings[index];
        if (index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"listingId\":";
        json += std::to_string(listing.listing_id);
        json += ",\"itemOrUnlockableId\":";
        json += std::to_string(listing.item_or_unlockable_id);
        json += ",\"price\":";
        json += std::to_string(listing.price);
        json += ",\"relatedSiteId\":";
        json += std::to_string(listing.related_site_id);
        json += ",\"quantity\":";
        json += std::to_string(listing.quantity);
        json += ",\"listingKind\":";
        append_json_string(json, phone_listing_kind_name(listing.listing_kind));
        json += ",\"flags\":";
        json += std::to_string(listing.flags);
        json += '}';
    }
    json += "]";
}

void append_site_state_json(std::string& json, const std::optional<SmokeEngineHost::SiteSnapshotProjection>& snapshot)
{
    if (!snapshot.has_value())
    {
        json += "null";
        return;
    }

    const auto& site_snapshot = snapshot.value();
    json += "{\"siteId\":";
    json += std::to_string(site_snapshot.site_id);
    json += ",";
    append_site_worker_json(json, site_snapshot);
    json += ",";
    append_site_camp_state_json(json, site_snapshot);
    json += ",";
    append_site_weather_json(json, site_snapshot);
    json += ",";
    append_inventory_slots_json(json, site_snapshot);
    json += ",";
    append_site_tasks_json(json, site_snapshot);
    json += ",";
    append_site_phone_listings_json(json, site_snapshot);
    json += '}';
}

void append_site_state_patch_json(
    std::string& json,
    const std::optional<SmokeEngineHost::SiteSnapshotProjection>& snapshot,
    std::uint32_t field_mask)
{
    if (!snapshot.has_value())
    {
        json += "null";
        return;
    }

    const auto& site_snapshot = snapshot.value();
    json += "{";
    bool has_fields = false;
    const auto append_field = [&](auto&& append_value) {
        if (has_fields)
        {
            json += ",";
        }
        append_value();
        has_fields = true;
    };

    if ((field_mask & SmokeEngineHost::LiveStatePatchField_SiteStateWorker) != 0U)
    {
        append_field([&]() { append_site_worker_json(json, site_snapshot); });
    }
    if ((field_mask & SmokeEngineHost::LiveStatePatchField_SiteStateCamp) != 0U)
    {
        append_field([&]() { append_site_camp_state_json(json, site_snapshot); });
    }
    if ((field_mask & SmokeEngineHost::LiveStatePatchField_SiteStateWeather) != 0U)
    {
        append_field([&]() { append_site_weather_json(json, site_snapshot); });
    }
    if ((field_mask & SmokeEngineHost::LiveStatePatchField_SiteStateInventory) != 0U)
    {
        append_field([&]() { append_inventory_slots_json(json, site_snapshot); });
    }
    if ((field_mask & SmokeEngineHost::LiveStatePatchField_SiteStateTasks) != 0U)
    {
        append_field([&]() { append_site_tasks_json(json, site_snapshot); });
    }
    if ((field_mask & SmokeEngineHost::LiveStatePatchField_SiteStatePhone) != 0U)
    {
        append_field([&]() { append_site_phone_listings_json(json, site_snapshot); });
    }

    json += '}';
}

void append_hud_json(std::string& json, const std::optional<SmokeEngineHost::HudProjection>& hud_state)
{
    if (!hud_state.has_value())
    {
        json += "null";
        return;
    }

    const auto& hud = hud_state.value();
    json += "{\"playerHealth\":";
    json += std::to_string(hud.player_health);
    json += ",\"playerHydration\":";
    json += std::to_string(hud.player_hydration);
    json += ",\"playerEnergy\":";
    json += std::to_string(hud.player_energy);
    json += ",\"currentMoney\":";
    json += std::to_string(hud.current_money);
    json += ",\"activeTaskCount\":";
    json += std::to_string(hud.active_task_count);
    json += ",\"warningCode\":";
    json += std::to_string(hud.warning_code);
    json += ",\"currentActionKind\":";
    json += std::to_string(hud.current_action_kind);
    json += ",\"siteCompletionNormalized\":";
    json += std::to_string(hud.site_completion_normalized);
    json += '}';
}

void append_site_action_json(
    std::string& json,
    const std::optional<SmokeEngineHost::SiteActionProjection>& site_action)
{
    if (!site_action.has_value())
    {
        json += "null";
        return;
    }

    const auto& action = site_action.value();
    json += "{\"actionId\":";
    json += std::to_string(action.action_id);
    json += ",\"targetTileX\":";
    json += std::to_string(action.target_tile_x);
    json += ",\"targetTileY\":";
    json += std::to_string(action.target_tile_y);
    json += ",\"actionKind\":";
    json += std::to_string(action.action_kind);
    json += ",\"flags\":";
    json += std::to_string(action.flags);
    json += ",\"progressNormalized\":";
    json += std::to_string(action.progress_normalized);
    json += ",\"durationMinutes\":";
    json += std::to_string(action.duration_minutes);
    json += '}';
}

void append_site_result_json(std::string& json, const std::optional<SmokeEngineHost::SiteResultProjection>& site_result)
{
    if (!site_result.has_value())
    {
        json += "null";
        return;
    }

    const auto& result = site_result.value();
    json += "{\"siteId\":";
    json += std::to_string(result.site_id);
    json += ",\"result\":";
    append_json_string(json, site_attempt_result_name(result.result));
    json += ",\"newlyRevealedSiteCount\":";
    json += std::to_string(result.newly_revealed_site_count);
    json += '}';
}

}  // namespace

void SmokeEngineHost::write_json_string(std::string& destination, std::string_view value)
{
    append_json_string(destination, value);
}

std::string SmokeEngineHost::build_live_state_json() const
{
    return build_live_state_json(capture_live_state_snapshot());
}

std::string SmokeEngineHost::build_live_state_json(const LiveStateSnapshot& live_state)
{
    std::string json {};
    json.reserve(16384);

    json += "{\"frameNumber\":";
    json += std::to_string(live_state.frame_number);
    json += ",\"appState\":";
    append_optional_app_state_json(json, live_state.current_app_state);
    json += ",\"selectedSiteId\":";
    append_optional_u32_json(json, live_state.selected_site_id);
    json += ",\"scriptFailed\":";
    append_bool_json(json, live_state.script_failed);
    json += ",\"commandEntries\":";
    append_command_entries_json(json, live_state.current_frame_command_entries);
    json += ",\"logTail\":";
    append_command_entries_json(json, live_state.command_log_tail);
    json += ",\"uiSetups\":";
    append_ui_setups_json(json, live_state.active_ui_setups);
    json += ",\"regionalMap\":";
    append_regional_map_json(json, live_state.regional_map_sites, live_state.regional_map_links);
    json += ",\"siteBootstrap\":";
    append_site_bootstrap_json(json, live_state.active_site_snapshot);
    json += ",\"siteState\":";
    append_site_state_json(json, live_state.active_site_snapshot);
    json += ",\"hud\":";
    append_hud_json(json, live_state.hud_state);
    json += ",\"siteAction\":";
    append_site_action_json(json, live_state.site_action);
    json += ",\"siteResult\":";
    append_site_result_json(json, live_state.site_result);
    json += "}";
    return json;
}

std::string SmokeEngineHost::build_live_state_patch_json(
    const LiveStateSnapshot& snapshot,
    std::uint32_t field_mask)
{
    constexpr std::uint32_t k_any_site_state_patch_fields =
        LiveStatePatchField_SiteStateWorker |
        LiveStatePatchField_SiteStateCamp |
        LiveStatePatchField_SiteStateWeather |
        LiveStatePatchField_SiteStateInventory |
        LiveStatePatchField_SiteStateTasks |
        LiveStatePatchField_SiteStatePhone;

    std::string json {};
    json.reserve(4096);
    json += "{\"frameNumber\":";
    json += std::to_string(snapshot.frame_number);

    bool has_fields = false;
    const auto append_field = [&](const char* field_name, auto&& append_value) {
        if (has_fields)
        {
            json += ",\"";
            json += field_name;
            json += "\":";
        }
        else
        {
            json += ",\"";
            json += field_name;
            json += "\":";
            has_fields = true;
        }
        append_value(json);
    };

    if ((field_mask & LiveStatePatchField_AppState) != 0U)
    {
        append_field("appState", [&](std::string& destination) {
            append_optional_app_state_json(destination, snapshot.current_app_state);
        });
    }
    if ((field_mask & LiveStatePatchField_SelectedSiteId) != 0U)
    {
        append_field("selectedSiteId", [&](std::string& destination) {
            append_optional_u32_json(destination, snapshot.selected_site_id);
        });
    }
    if ((field_mask & LiveStatePatchField_ScriptFailed) != 0U)
    {
        append_field("scriptFailed", [&](std::string& destination) {
            append_bool_json(destination, snapshot.script_failed);
        });
    }
    if ((field_mask & LiveStatePatchField_UiSetups) != 0U)
    {
        append_field("uiSetups", [&](std::string& destination) {
            append_ui_setups_json(destination, snapshot.active_ui_setups);
        });
    }
    if ((field_mask & LiveStatePatchField_RegionalMap) != 0U)
    {
        append_field("regionalMap", [&](std::string& destination) {
            append_regional_map_json(destination, snapshot.regional_map_sites, snapshot.regional_map_links);
        });
    }
    if ((field_mask & LiveStatePatchField_SiteBootstrap) != 0U)
    {
        append_field("siteBootstrap", [&](std::string& destination) {
            append_site_bootstrap_json(destination, snapshot.active_site_snapshot);
        });
    }
    if ((field_mask & LiveStatePatchField_SiteState) != 0U)
    {
        append_field("siteState", [&](std::string& destination) {
            append_site_state_json(destination, snapshot.active_site_snapshot);
        });
    }
    else if ((field_mask & k_any_site_state_patch_fields) != 0U)
    {
        append_field("siteStatePatch", [&](std::string& destination) {
            append_site_state_patch_json(destination, snapshot.active_site_snapshot, field_mask);
        });
    }
    if ((field_mask & LiveStatePatchField_Hud) != 0U)
    {
        append_field("hud", [&](std::string& destination) {
            append_hud_json(destination, snapshot.hud_state);
        });
    }
    if ((field_mask & LiveStatePatchField_SiteAction) != 0U)
    {
        append_field("siteAction", [&](std::string& destination) {
            append_site_action_json(destination, snapshot.site_action);
        });
    }
    if ((field_mask & LiveStatePatchField_SiteResult) != 0U)
    {
        append_field("siteResult", [&](std::string& destination) {
            append_site_result_json(destination, snapshot.site_result);
        });
    }

    if (!has_fields)
    {
        return {};
    }

    json += '}';
    return json;
}
