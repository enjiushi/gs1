#pragma once

#include "smoke_script_directive.h"

#include <deque>
#include <optional>
#include <sstream>
#include <string>

class SmokeScriptRunner final
{
public:
    [[nodiscard]] bool load_file(const std::string& script_path);
    [[nodiscard]] bool has_next_directive() const noexcept { return !queued_directives_.empty(); }
    [[nodiscard]] std::optional<SmokeScriptDirective> pop_next_directive();

private:
    [[nodiscard]] bool parse_line(
        const std::string& script_path,
        std::size_t line_number,
        const std::string& line);

    [[nodiscard]] bool parse_command(
        const std::string& script_path,
        std::size_t line_number,
        const std::string& trimmed_line,
        const std::string& opcode,
        std::istringstream& arguments);

    bool fail(
        const std::string& script_path,
        std::size_t line_number,
        const std::string& message);

private:
    std::deque<SmokeScriptDirective> queued_directives_ {};
    bool failed_ {false};
};
