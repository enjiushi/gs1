#pragma once

#include "gs1/types.h"

#include <cstddef>
#include <cstdint>
#include <string>

enum class SmokeScriptOpcode
{
    WaitFrames,
    ClickUiAction,
    WaitAppState,
    WaitSelectedSite,
    AssertAppState,
    AssertSelectedSite,
    AssertSawCommand,
    AssertLogContains
};

struct SmokeScriptDirective
{
    SmokeScriptOpcode opcode {SmokeScriptOpcode::WaitFrames};
    std::string script_path {};
    std::size_t line_number {0};
    std::string source_text {};
    Gs1UiAction ui_action {};
    Gs1AppState app_state {GS1_APP_STATE_BOOT};
    std::uint32_t site_id {0};
    Gs1EngineCommandType engine_command_type {GS1_ENGINE_COMMAND_NONE};
    std::string text {};
    std::uint32_t remaining_frames {0};
};
