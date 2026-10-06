/*
XS56K - a realtime editor for the AKAI S5000/S6000 samplers
Copyright (C) 2026 https://github.com/xplorer2716

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "akm/CommandResult.hpp"
#include "akm/SamplerError.hpp"
#include "mcp/SamplerGateway.hpp"

namespace mcp::detail
{
    // What the units of the sampler gateway share (the main one and the disk one): waiting for the completion of an
    // asynchronous AKM call, and saying in plain words why a command did not succeed. Internal to the library.
    // [RQ-MCP-009, ADR-MCP-001 (DEC-MCP-003)]

    // What is added to the session's own timeouts before the gateway gives up waiting for a completion that should always
    // come (a session completes every command, DEC-AKM-004): it only guards against a lost one.
    inline constexpr std::chrono::milliseconds WAIT_MARGIN{3000};

    inline std::string numberText(std::int64_t value)
    {
        return std::to_string(value);
    }

    /// Waits for the one completion of an asynchronous AKM call, with a deadline; nothing when it did not come.
    template <typename Result>
    std::optional<Result> await(std::chrono::milliseconds deadline, const std::function<void(std::function<void(const Result&)>)>& start)
    {
        auto promise = std::make_shared<std::promise<Result>>();
        std::future<Result> future = promise->get_future();
        start([promise](const Result& result) { promise->set_value(result); });
        if (future.wait_for(deadline) != std::future_status::ready)
            return std::nullopt;
        return future.get();
    }

    /// What a command's outcome says when it did not succeed, as one sentence for the person. `currentObject` is what
    /// must be current for the command to find anything ("program", "sample", "multi", "disk").
    inline std::string explain(const akm::CommandResult& outcome, const std::string& doing, const GatewayConfig& config,
                               bool needsCurrentProgram, const std::string& currentObject = "program")
    {
        if (std::holds_alternative<akm::Timeout>(outcome))
            return "The sampler did not answer while " + doing + " (no reply within " + numberText(config.commandTimeout.count()) +
                   " ms). Check that it is on and connected.";
        if (const auto* refusal = std::get_if<akm::Refused>(&outcome))
            return "The command was not sent while " + doing + ": " + std::string(akm::describe(refusal->reason)) + ".";
        if (const auto* error = std::get_if<akm::Error>(&outcome))
        {
            const akm::ErrorInfo info = akm::describeError(error->number);
            std::string text = "The sampler refused while " + doing + ": " + std::string(info.meaning) + " (error " +
                               numberText(error->number) + ").";
            if (error->number == akm::error_number::KEYGROUP_NOT_IN_PROGRAM)
                text += " The current program does not have that keygroup.";
            else if (needsCurrentProgram && error->number == akm::error_number::NOT_FOUND)
                text += " Is a " + currentObject + " selected? Use select_" + currentObject + " first.";
            // The real S5000 answers 257 (not 4) to a disk command when no disk is selected (TASK-MCP-041).
            else if (needsCurrentProgram && error->number == akm::error_number::DISK_SELECTED_DISK_INVALID)
                text += " Is a disk selected? Use select_disk first.";
            return text;
        }
        if (std::holds_alternative<akm::Cancelled>(outcome))
            return "The command was cancelled while " + doing + ": the connection is closing.";
        return "The sampler's answer to the command while " + doing + " was not understood.";
    }
}
