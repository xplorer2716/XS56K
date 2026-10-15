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
#include "app/Edit.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <string_view>

namespace app
{
    // [RQ-APP-051, RQ-APP-086, RQ-APP-087, ADR-APP-004 (DEC-APP-027), TASK-APP-038]
    namespace
    {
        /// The parameter read as a note name (the sampler's screen shows "C3"): 60 is C3, 21 is A-1.
        constexpr std::string_view NOTE_PARAMETER = "sample original pitch";
        constexpr std::array<std::string_view, 12> NOTE_NAMES{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        constexpr std::int64_t NOTES_PER_OCTAVE = 12;
        constexpr std::int64_t OCTAVE_OF_NOTE_ZERO = -2;  // note 0 is C-2, so 60 is C3
        constexpr char SHARP = '#';
        constexpr char PLUS = '+';
        constexpr char MINUS = '-';

        constexpr std::string_view TYPE_NAME = "sample type";
        constexpr std::string_view CHANNELS_NAME = "sample channels";
        constexpr std::string_view LENGTH_NAME = "sample length";
        constexpr std::string_view RATE_NAME = "sample rate";
        constexpr std::int64_t MONO_CHANNELS = 1;
        constexpr std::string_view MONO = "mono";
        constexpr std::string_view STEREO = "stereo";
        constexpr std::string_view POINTS = " points";
        constexpr std::string_view HERTZ = " Hz";
        constexpr std::string_view INFO_SEPARATOR = ", ";

        bool isNote(const EditValue& value)
        {
            return value.definition->name == NOTE_PARAMETER;
        }

        std::string noteName(std::int64_t note)
        {
            const std::int64_t octave = note / NOTES_PER_OCTAVE + OCTAVE_OF_NOTE_ZERO;
            return std::string(NOTE_NAMES[static_cast<std::size_t>(note % NOTES_PER_OCTAVE)]) + std::to_string(octave);
        }

        std::string trimmed(std::string_view text)
        {
            const auto first = text.find_first_not_of(' ');
            if (first == std::string_view::npos)
                return {};
            return std::string(text.substr(first, text.find_last_not_of(' ') - first + 1));
        }

        std::string upper(std::string_view text)
        {
            std::string result(text);
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return result;
        }

        /// The whole of `text` as a number, a leading + allowed.
        std::optional<std::int64_t> number(std::string_view text)
        {
            if (!text.empty() && text.front() == PLUS)
                text.remove_prefix(1);
            std::int64_t value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size())
                return std::nullopt;
            return value;
        }

        /// "C3", "c#3" or "A-1" as a note number.
        std::optional<std::int64_t> noteFrom(std::string_view text)
        {
            const std::string name = upper(text);
            for (std::size_t length : {std::size_t{2}, std::size_t{1}})
            {
                if (name.size() <= length)
                    continue;
                const auto found = std::find(NOTE_NAMES.begin(), NOTE_NAMES.end(), std::string_view(name).substr(0, length));
                if (found == NOTE_NAMES.end() || (length == 1 && name[1] == SHARP))
                    continue;
                const auto octave = number(std::string_view(name).substr(length));
                if (!octave)
                    return std::nullopt;
                return (*octave - OCTAVE_OF_NOTE_ZERO) * NOTES_PER_OCTAVE + (found - NOTE_NAMES.begin());
            }
            return std::nullopt;
        }

        const EditValue* infoNamed(const EditState& state, std::string_view name)
        {
            const auto found = std::find_if(state.info.begin(), state.info.end(), [name](const EditValue& value) { return value.definition->name == name; });
            return found == state.info.end() ? nullptr : &*found;
        }
    }

    std::string valueText(const EditValue& value)
    {
        const mcp::ParameterDefinition& definition = *value.definition;
        if (isNote(value))
            return noteName(value.value);
        if (definition.kind == mcp::ParameterKind::Choice && value.value >= 0 && static_cast<std::size_t>(value.value) < definition.labels.size())
            return definition.labels[static_cast<std::size_t>(value.value)];
        if (definition.kind == mcp::ParameterKind::Signed && value.value > 0)
            return PLUS + std::to_string(value.value);
        return std::to_string(value.value);
    }

    std::optional<std::int64_t> valueFrom(const EditValue& value, std::string_view text)
    {
        const std::string typed = trimmed(text);
        if (const auto read = number(typed))
            return read;
        if (isNote(value))
            return noteFrom(typed);
        const auto& labels = value.definition->labels;
        const auto found = std::find_if(labels.begin(), labels.end(), [key = upper(typed)](const std::string& label) { return label == key; });
        if (found != labels.end())
            return static_cast<std::int64_t>(found - labels.begin());
        return std::nullopt;
    }

    std::string infoText(const EditState& state)
    {
        std::string text;
        const auto add = [&text](std::string_view part) {
            text += (text.empty() ? std::string() : std::string(INFO_SEPARATOR)) + std::string(part);
        };
        if (const EditValue* type = infoNamed(state, TYPE_NAME))
            add(valueText(*type));
        if (const EditValue* channels = infoNamed(state, CHANNELS_NAME))
            add(channels->value == MONO_CHANNELS ? MONO : STEREO);
        if (const EditValue* length = infoNamed(state, LENGTH_NAME))
            add(std::to_string(length->value) + std::string(POINTS));
        if (const EditValue* rate = infoNamed(state, RATE_NAME))
            add(std::to_string(rate->value) + std::string(HERTZ));
        return text;
    }
}
