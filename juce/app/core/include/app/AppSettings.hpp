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

#include <optional>
#include <string>
#include <string_view>

namespace app
{
    /// The person's settings, kept between runs: the application stores them with `juce::PropertiesFile` in the person's settings folder,
    /// the tests in memory. [RQ-APP-016, ADR-APP-001 (DEC-APP-009)]
    class AppSettings
    {
    public:
        virtual ~AppSettings() = default;

        /// The value stored under `key`, or nothing when none was ever written.
        [[nodiscard]] virtual std::optional<std::string> read(std::string_view key) const = 0;
        virtual void write(std::string_view key, std::string value) = 0;
    };
}
