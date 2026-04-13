#include "smoke_script_runner.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace
{
std::string trim_copy(const std::string& value)
{
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        start += 1;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        end -= 1;
    }

    return value.substr(start, end - start);
}

std::optional<Gs1UiActionType> parse_ui_action_type(const std::string& value)
{
    if (value == "START_NEW_CAMPAIGN")
    {
        return GS1_UI_ACTION_START_NEW_CAMPAIGN;
    }
    if (value == "SELECT_DEPLOYMENT_SITE")
    {
        return GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE;
    }
    if (value == "CLEAR_DEPLOYMENT_SITE_SELECTION")
    {
        return GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    }
    if (value == "START_SITE_ATTEMPT")
    {
        return GS1_UI_ACTION_START_SITE_ATTEMPT;
    }
    if (value == "RETURN_TO_REGIONAL_MAP")
    {
        return GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP;
    }
    if (value == "ACCEPT_TASK")
    {
        return GS1_UI_ACTION_ACCEPT_TASK;
    }
    if (value == "CLAIM_TASK_REWARD")
    {
        return GS1_UI_ACTION_CLAIM_TASK_REWARD;
    }
    if (value == "BUY_PHONE_LISTING")
    {
        return GS1_UI_ACTION_BUY_PHONE_LISTING;
    }
    if (value == "SELL_PHONE_LISTING")
    {
        return GS1_UI_ACTION_SELL_PHONE_LISTING;
    }
    if (value == "USE_INVENTORY_ITEM")
    {
        return GS1_UI_ACTION_USE_INVENTORY_ITEM;
    }
    if (value == "TRANSFER_INVENTORY_ITEM")
    {
        return GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM;
    }
    if (value == "HIRE_CONTRACTOR")
    {
        return GS1_UI_ACTION_HIRE_CONTRACTOR;
    }
    if (value == "PURCHASE_SITE_UNLOCKABLE")
    {
        return GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE;
    }

    return std::nullopt;
}

std::optional<Gs1AppState> parse_app_state(const std::string& value)
{
    if (value == "BOOT")
    {
        return GS1_APP_STATE_BOOT;
    }
    if (value == "MAIN_MENU")
    {
        return GS1_APP_STATE_MAIN_MENU;
    }
    if (value == "CAMPAIGN_SETUP")
    {
        return GS1_APP_STATE_CAMPAIGN_SETUP;
    }
    if (value == "REGIONAL_MAP")
    {
        return GS1_APP_STATE_REGIONAL_MAP;
    }
    if (value == "SITE_LOADING")
    {
        return GS1_APP_STATE_SITE_LOADING;
    }
    if (value == "SITE_ACTIVE")
    {
        return GS1_APP_STATE_SITE_ACTIVE;
    }
    if (value == "SITE_PAUSED")
    {
        return GS1_APP_STATE_SITE_PAUSED;
    }
    if (value == "SITE_RESULT")
    {
        return GS1_APP_STATE_SITE_RESULT;
    }
    if (value == "CAMPAIGN_END")
    {
        return GS1_APP_STATE_CAMPAIGN_END;
    }

    return std::nullopt;
}

std::optional<Gs1EngineCommandType> parse_engine_command_type(const std::string& value)
{
    if (value == "SET_APP_STATE")
    {
        return GS1_ENGINE_COMMAND_SET_APP_STATE;
    }
    if (value == "BEGIN_REGIONAL_MAP_SNAPSHOT")
    {
        return GS1_ENGINE_COMMAND_BEGIN_REGIONAL_MAP_SNAPSHOT;
    }
    if (value == "BEGIN_UI_SETUP")
    {
        return GS1_ENGINE_COMMAND_BEGIN_UI_SETUP;
    }
    if (value == "CLOSE_UI_SETUP")
    {
        return GS1_ENGINE_COMMAND_CLOSE_UI_SETUP;
    }
    if (value == "UI_ELEMENT_UPSERT")
    {
        return GS1_ENGINE_COMMAND_UI_ELEMENT_UPSERT;
    }
    if (value == "REGIONAL_MAP_SITE_UPSERT")
    {
        return GS1_ENGINE_COMMAND_REGIONAL_MAP_SITE_UPSERT;
    }
    if (value == "REGIONAL_MAP_LINK_UPSERT")
    {
        return GS1_ENGINE_COMMAND_REGIONAL_MAP_LINK_UPSERT;
    }
    if (value == "BEGIN_SITE_SNAPSHOT")
    {
        return GS1_ENGINE_COMMAND_BEGIN_SITE_SNAPSHOT;
    }
    if (value == "SITE_TILE_UPSERT")
    {
        return GS1_ENGINE_COMMAND_SITE_TILE_UPSERT;
    }
    if (value == "SITE_WORKER_UPDATE")
    {
        return GS1_ENGINE_COMMAND_SITE_WORKER_UPDATE;
    }
    if (value == "SITE_CAMP_UPDATE")
    {
        return GS1_ENGINE_COMMAND_SITE_CAMP_UPDATE;
    }
    if (value == "SITE_WEATHER_UPDATE")
    {
        return GS1_ENGINE_COMMAND_SITE_WEATHER_UPDATE;
    }
    if (value == "SITE_ACTION_UPDATE")
    {
        return GS1_ENGINE_COMMAND_SITE_ACTION_UPDATE;
    }
    if (value == "HUD_STATE")
    {
        return GS1_ENGINE_COMMAND_HUD_STATE;
    }
    if (value == "SITE_RESULT_READY")
    {
        return GS1_ENGINE_COMMAND_SITE_RESULT_READY;
    }
    if (value == "LOG_TEXT")
    {
        return GS1_ENGINE_COMMAND_LOG_TEXT;
    }

    return std::nullopt;
}

void consume_legacy_delta_argument(std::istringstream& arguments)
{
    double ignored_delta_seconds = 0.0;
    arguments >> ignored_delta_seconds;
}
}  // namespace

bool SmokeScriptRunner::load_file(const std::string& script_path)
{
    queued_directives_.clear();
    failed_ = false;

    std::ifstream input {script_path};
    if (!input.is_open())
    {
        std::cerr << "Failed to open smoke script: " << script_path << '\n';
        failed_ = true;
        return false;
    }

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line))
    {
        line_number += 1;
        if (!parse_line(script_path, line_number, line))
        {
            return false;
        }
    }

    return true;
}

std::optional<SmokeScriptDirective> SmokeScriptRunner::pop_next_directive()
{
    if (queued_directives_.empty())
    {
        return std::nullopt;
    }

    auto directive = std::move(queued_directives_.front());
    queued_directives_.pop_front();
    return directive;
}

bool SmokeScriptRunner::parse_line(
    const std::string& script_path,
    std::size_t line_number,
    const std::string& line)
{
    const auto trimmed = trim_copy(line);
    if (trimmed.empty() || trimmed.starts_with('#'))
    {
        return true;
    }

    std::istringstream stream {trimmed};
    std::string opcode;
    stream >> opcode;
    return parse_command(script_path, line_number, trimmed, opcode, stream);
}

bool SmokeScriptRunner::parse_command(
    const std::string& script_path,
    std::size_t line_number,
    const std::string& trimmed_line,
    const std::string& opcode,
    std::istringstream& arguments)
{
    SmokeScriptDirective directive {};
    directive.script_path = script_path;
    directive.line_number = line_number;
    directive.source_text = trimmed_line;

    if (opcode == "update")
    {
        directive.opcode = SmokeScriptOpcode::WaitFrames;
        directive.remaining_frames = 1U;
        consume_legacy_delta_argument(arguments);
        queued_directives_.push_back(std::move(directive));
        return true;
    }

    if (opcode == "wait_frames")
    {
        directive.opcode = SmokeScriptOpcode::WaitFrames;
        if (!(arguments >> directive.remaining_frames))
        {
            return fail(script_path, line_number, "wait_frames requires <frame_count>");
        }

        queued_directives_.push_back(std::move(directive));
        return true;
    }

    if (opcode == "click_ui_action")
    {
        std::string action_name;
        if (!(arguments >> action_name))
        {
            return fail(script_path, line_number, "click_ui_action requires an action name");
        }

        const auto action_type = parse_ui_action_type(action_name);
        if (!action_type.has_value())
        {
            return fail(script_path, line_number, "unknown ui action: " + action_name);
        }

        directive.opcode = SmokeScriptOpcode::ClickUiAction;
        directive.ui_action.type = action_type.value();

        switch (directive.ui_action.type)
        {
        case GS1_UI_ACTION_START_NEW_CAMPAIGN:
        case GS1_UI_ACTION_START_SITE_ATTEMPT:
        case GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP:
            break;

        case GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE:
            if (!(arguments >> directive.ui_action.target_id))
            {
                return fail(
                    script_path,
                    line_number,
                    "SELECT_DEPLOYMENT_SITE requires <site_id>");
            }
            break;

        default:
            return fail(script_path, line_number, "unsupported ui action type");
        }

        queued_directives_.push_back(std::move(directive));
        return true;
    }

    if (opcode == "wait_app_state")
    {
        std::string state_name;
        if (!(arguments >> state_name >> directive.remaining_frames))
        {
            return fail(
                script_path,
                line_number,
                "wait_app_state requires <state> <max_frames>");
        }

        const auto state = parse_app_state(state_name);
        if (!state.has_value())
        {
            return fail(script_path, line_number, "unknown app state: " + state_name);
        }

        directive.opcode = SmokeScriptOpcode::WaitAppState;
        directive.app_state = state.value();
        consume_legacy_delta_argument(arguments);
        queued_directives_.push_back(std::move(directive));
        return true;
    }

    if (opcode == "wait_selected_site")
    {
        if (!(arguments >> directive.site_id >> directive.remaining_frames))
        {
            return fail(
                script_path,
                line_number,
                "wait_selected_site requires <site_id> <max_frames>");
        }

        directive.opcode = SmokeScriptOpcode::WaitSelectedSite;
        consume_legacy_delta_argument(arguments);
        queued_directives_.push_back(std::move(directive));
        return true;
    }

    if (opcode == "assert_app_state")
    {
        std::string state_name;
        if (!(arguments >> state_name))
        {
            return fail(script_path, line_number, "assert_app_state requires <state>");
        }

        const auto state = parse_app_state(state_name);
        if (!state.has_value())
        {
            return fail(script_path, line_number, "unknown app state: " + state_name);
        }

        directive.opcode = SmokeScriptOpcode::AssertAppState;
        directive.app_state = state.value();
        queued_directives_.push_back(std::move(directive));
        return true;
    }

    if (opcode == "assert_selected_site")
    {
        if (!(arguments >> directive.site_id))
        {
            return fail(script_path, line_number, "assert_selected_site requires <site_id>");
        }

        directive.opcode = SmokeScriptOpcode::AssertSelectedSite;
        queued_directives_.push_back(std::move(directive));
        return true;
    }

    if (opcode == "assert_saw_command")
    {
        std::string command_name;
        if (!(arguments >> command_name))
        {
            return fail(script_path, line_number, "assert_saw_command requires <command_name>");
        }

        const auto command_type = parse_engine_command_type(command_name);
        if (!command_type.has_value())
        {
            return fail(script_path, line_number, "unknown engine command: " + command_name);
        }

        directive.opcode = SmokeScriptOpcode::AssertSawCommand;
        directive.engine_command_type = command_type.value();
        queued_directives_.push_back(std::move(directive));
        return true;
    }

    if (opcode == "assert_log_contains")
    {
        std::getline(arguments, directive.text);
        directive.text = trim_copy(directive.text);
        if (directive.text.empty())
        {
            return fail(script_path, line_number, "assert_log_contains requires text");
        }

        directive.opcode = SmokeScriptOpcode::AssertLogContains;
        queued_directives_.push_back(std::move(directive));
        return true;
    }

    return fail(script_path, line_number, "unknown opcode: " + opcode);
}

bool SmokeScriptRunner::fail(
    const std::string& script_path,
    std::size_t line_number,
    const std::string& message)
{
    failed_ = true;
    std::cerr << script_path << ':' << line_number << ": " << message << '\n';
    return false;
}
