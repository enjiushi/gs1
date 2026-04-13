#include "smoke_engine_host.h"

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
}  // namespace

void SmokeEngineHost::write_json_string(std::string& destination, std::string_view value)
{
    destination.push_back('"');

    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
            destination += "\\\\";
            break;
        case '"':
            destination += "\\\"";
            break;
        case '\n':
            destination += "\\n";
            break;
        case '\r':
            destination += "\\r";
            break;
        case '\t':
            destination += "\\t";
            break;
        case '<':
            destination += "\\u003c";
            break;
        case '>':
            destination += "\\u003e";
            break;
        case '&':
            destination += "\\u0026";
            break;
        default:
            destination.push_back(ch);
            break;
        }
    }

    destination.push_back('"');
}

std::string SmokeEngineHost::build_live_state_json() const
{
    return build_live_state_json(capture_live_state_snapshot());
}

std::string SmokeEngineHost::build_live_state_json(const LiveStateSnapshot& live_state)
{
    std::string json {};
    json.reserve(16384);

    auto append_optional_app_state = [&](const std::optional<Gs1AppState>& value) {
        if (!value.has_value())
        {
            json += "null";
            return;
        }

        write_json_string(json, app_state_name(value.value()));
    };

    auto append_optional_u32 = [&](const std::optional<std::uint32_t>& value) {
        if (!value.has_value())
        {
            json += "null";
            return;
        }

        json += std::to_string(value.value());
    };

    auto append_ui_action = [&](const Gs1UiAction& action) {
        json += "{\"type\":";
        write_json_string(json, ui_action_name(action.type));
        json += ",\"targetId\":";
        json += std::to_string(action.target_id);
        json += ",\"arg0\":";
        json += std::to_string(action.arg0);
        json += ",\"arg1\":";
        json += std::to_string(action.arg1);
        json += '}';
    };

    json += "{\"frameNumber\":";
    json += std::to_string(live_state.frame_number);
    json += ",\"appState\":";
    append_optional_app_state(live_state.current_app_state);
    json += ",\"selectedSiteId\":";
    append_optional_u32(live_state.selected_site_id);
    json += ",\"scriptFailed\":";
    json += live_state.script_failed ? "true" : "false";

    json += ",\"commandEntries\":[";
    for (std::size_t index = 0; index < live_state.current_frame_command_entries.size(); ++index)
    {
        if (index > 0U)
        {
            json.push_back(',');
        }
        write_json_string(json, live_state.current_frame_command_entries[index]);
    }
    json += "]";

    json += ",\"logTail\":[";
    for (std::size_t index = 0; index < live_state.command_log_tail.size(); ++index)
    {
        if (index > 0U)
        {
            json.push_back(',');
        }
        write_json_string(json, live_state.command_log_tail[index]);
    }
    json += "]";

    json += ",\"uiSetups\":[";
    for (std::size_t setup_index = 0; setup_index < live_state.active_ui_setups.size(); ++setup_index)
    {
        const auto& setup = live_state.active_ui_setups[setup_index];
        if (setup_index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"setupId\":";
        write_json_string(json, ui_setup_name(setup.setup_id));
        json += ",\"presentationType\":";
        write_json_string(json, ui_setup_presentation_type_name(setup.presentation_type));
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
            write_json_string(json, ui_element_type_name(element.element_type));
            json += ",\"flags\":";
            json += std::to_string(element.flags);
            json += ",\"text\":";
            write_json_string(json, element.text);
            json += ",\"action\":";
            append_ui_action(element.action);
            json += "}";
        }

        json += "]}";
    }
    json += "]";

    json += ",\"regionalMap\":{\"sites\":[";
    for (std::size_t site_index = 0; site_index < live_state.regional_map_sites.size(); ++site_index)
    {
        const auto& site = live_state.regional_map_sites[site_index];
        if (site_index > 0U)
        {
            json.push_back(',');
        }

        json += "{\"siteId\":";
        json += std::to_string(site.site_id);
        json += ",\"siteState\":";
        write_json_string(json, site_state_name(site.site_state));
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
        json += "}";
    }
    json += "],\"links\":[";
    for (std::size_t link_index = 0; link_index < live_state.regional_map_links.size(); ++link_index)
    {
        const auto& link = live_state.regional_map_links[link_index];
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
        json += "}";
    }
    json += "]}";

    json += ",\"siteBootstrap\":";
    if (!live_state.active_site_snapshot.has_value())
    {
        json += "null";
    }
    else
    {
        const auto& site_snapshot = live_state.active_site_snapshot.value();
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
            json += "}";
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
            json += "}";
        }

        json += "}";
    }

    json += ",\"siteState\":";
    if (!live_state.active_site_snapshot.has_value())
    {
        json += "null";
    }
    else
    {
        const auto& site_snapshot = live_state.active_site_snapshot.value();
        json += "{\"siteId\":";
        json += std::to_string(site_snapshot.site_id);

        json += ",\"worker\":";
        if (!site_snapshot.worker.has_value())
        {
            json += "null";
        }
        else
        {
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
            json += "}";
        }

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
            json += "}";
        }

        json += ",\"weather\":";
        if (!site_snapshot.weather.has_value())
        {
            json += "null";
        }
        else
        {
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
            write_json_string(json, weather_phase_name(weather.event_phase));
            json += ",\"phaseMinutesRemaining\":";
            json += std::to_string(weather.phase_minutes_remaining);
            json += "}";
        }

        json += "}";
    }

    json += ",\"hud\":";
    if (!live_state.hud_state.has_value())
    {
        json += "null";
    }
    else
    {
        const auto& hud = live_state.hud_state.value();
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
        json += "}";
    }

    json += ",\"siteResult\":";
    if (!live_state.site_result.has_value())
    {
        json += "null";
    }
    else
    {
        const auto& site_result = live_state.site_result.value();
        json += "{\"siteId\":";
        json += std::to_string(site_result.site_id);
        json += ",\"result\":";
        write_json_string(json, site_attempt_result_name(site_result.result));
        json += ",\"newlyRevealedSiteCount\":";
        json += std::to_string(site_result.newly_revealed_site_count);
        json += "}";
    }

    json += "}";
    return json;
}
