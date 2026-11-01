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

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace app
{
    /// A question asked before an action that loses data or can leave the sampler unresponsive: the question, the object it names and what
    /// will be lost. [RQ-APP-011]
    struct Confirmation
    {
        std::string question;
        std::string object;
        std::string consequence;
    };

    /// A question that asks for a count from `min` to `max`, `initial` offered: the title, and the lines that say what it is for.
    /// [RQ-APP-061, ADR-APP-004 (DEC-APP-034)]
    struct CountQuestion
    {
        std::string question;
        std::string text;
        int min = 1;
        int max = 1;
        int initial = 1;
        std::string action = "OK";  ///< the confirm button's text: "Add"
    };

    /// The person's answer to files already on the disk: replace them, skip them (they stay), or cancel the save. With one file, "replace
    /// all" would be "replace": it is not offered. [RQ-APP-045, ADR-APP-005 (DEC-APP-040)]
    enum class ExistingAnswer
    {
        Replace,
        Skip,
        Cancel,
    };

    /// A question asked before a save would write files the folder already holds: the title, what it is about, and the files known to be
    /// there. [RQ-APP-045, DEC-APP-040]
    struct ExistingQuestion
    {
        std::string question;
        std::string text;
        std::vector<std::string> files;
    };

    /// Every question the controller asks the person. The answer comes back on the UI thread, possibly later: the application asks with JUCE
    /// dialogs that do not block the message thread, the tests answer from a script. [RQ-APP-011, ADR-APP-001 (DEC-APP-007)]
    class UserPrompts
    {
    public:
        virtual ~UserPrompts() = default;

        /// Asks the question; `answer` is called once, with true when the person confirmed.
        virtual void confirm(const Confirmation& question, std::function<void(bool confirmed)> answer) = 0;
        /// Asks for a count; `answer` is called once, with the count from `min` to `max`, or with none when the person cancelled.
        virtual void askCount(const CountQuestion& question, std::function<void(std::optional<int> count)> answer) = 0;
        /// Asks what to do with files already on the disk; `answer` is called once, with `Cancel` when the person closed the question.
        /// [RQ-APP-045]
        virtual void askExisting(const ExistingQuestion& question, std::function<void(ExistingAnswer answer)> answer) = 0;
    };
}
