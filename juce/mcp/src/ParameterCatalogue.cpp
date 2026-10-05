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
#include "mcp/ParameterCatalogue.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <utility>

#include "StandardParameters.hpp"

namespace mcp
{
    namespace
    {
        constexpr std::size_t MAX_SUGGESTIONS = 3;
        constexpr std::size_t MIN_SUBSTRING_QUERY = 3;
        constexpr std::size_t MIN_EDIT_BUDGET = 2;
        constexpr std::size_t EDIT_BUDGET_DIVISOR = 3;

        constexpr const char* WORD_PLUS = "plus";
        constexpr const char* WORD_MINUS = "minus";
        constexpr const char* WORD_POLES = "poles";
        constexpr const char* WORD_POLE = "pole";

        // U+2212 MINUS SIGN and U+2013 EN DASH, which the spec's PDF uses for the minus of SQUARE-.
        constexpr const char* UTF8_MINUS_SIGN = "\xE2\x88\x92";
        constexpr const char* UTF8_EN_DASH = "\xE2\x80\x93";

        constexpr const char* TEXT_ON = "on";
        constexpr const char* TEXT_OFF = "off";

        void replaceAll(std::string& text, const std::string& from, const std::string& to)
        {
            for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
                text.replace(at, from.size(), to);
        }

        std::string trim(std::string_view text)
        {
            const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
            while (!text.empty() && isSpace(text.front()))
                text.remove_prefix(1);
            while (!text.empty() && isSpace(text.back()))
                text.remove_suffix(1);
            return std::string(text);
        }

        std::size_t editDistance(const std::string& a, const std::string& b)
        {
            std::vector<std::size_t> previous(b.size() + 1);
            std::vector<std::size_t> current(b.size() + 1);
            for (std::size_t j = 0; j <= b.size(); ++j)
                previous[j] = j;
            for (std::size_t i = 1; i <= a.size(); ++i)
            {
                current[0] = i;
                for (std::size_t j = 1; j <= b.size(); ++j)
                {
                    const std::size_t substitution = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
                    current[j] = std::min({previous[j] + 1, current[j - 1] + 1, substitution});
                }
                std::swap(previous, current);
            }
            return previous[b.size()];
        }

        // The whole of `text` as a whole number (an optional + or -), or nothing.
        std::optional<std::int64_t> parseWholeNumber(const std::string& text)
        {
            std::size_t start = 0;
            if (!text.empty() && text[0] == '+')
                start = 1;
            std::int64_t number = 0;
            const char* begin = text.data() + start;
            const char* end = text.data() + text.size();
            const auto parsed = std::from_chars(begin, end, number);
            if (parsed.ec != std::errc() || parsed.ptr != end)
                return std::nullopt;
            return number;
        }

        // A whole number followed by nothing, or by the parameter's unit ("12 dB").
        std::optional<std::int64_t> parseNumberWithUnit(const ParameterDefinition& parameter, const std::string& text)
        {
            std::size_t digitsEnd = 0;
            if (digitsEnd < text.size() && (text[digitsEnd] == '+' || text[digitsEnd] == '-'))
                ++digitsEnd;
            while (digitsEnd < text.size() && text[digitsEnd] >= '0' && text[digitsEnd] <= '9')
                ++digitsEnd;
            const auto number = parseWholeNumber(text.substr(0, digitsEnd));
            if (!number)
                return std::nullopt;
            const std::string rest = normalizeText(text.substr(digitsEnd));
            if (!rest.empty() && (parameter.unit.empty() || rest != normalizeText(parameter.unit)))
                return std::nullopt;
            return number;
        }

        std::string numberText(std::int64_t value)
        {
            return std::to_string(value);
        }

        std::string choicesText(const ParameterDefinition& parameter)
        {
            std::string text;
            for (std::size_t code = 0; code < parameter.labels.size(); ++code)
            {
                if (code != 0)
                    text += ", ";
                text += parameter.labels[code] + " (" + numberText(static_cast<std::int64_t>(code)) + ")";
            }
            return text;
        }

        ValueResolution refused(std::string problem)
        {
            return ValueResolution{std::nullopt, std::move(problem)};
        }

        // `got` is what was said, quoted when it was text.
        ValueResolution refusedValue(const ParameterDefinition& parameter, const std::string& got)
        {
            switch (parameter.kind)
            {
                case ParameterKind::Number:
                    if (parameter.step == 1)
                        return refused(parameter.name + " must be a whole number from " + describeRange(parameter) + " (got " +
                                       got + ").");
                    return refused(parameter.name + " must be " + describeRange(parameter) + " (got " + got + ").");
                case ParameterKind::Signed:
                    return refused(parameter.name + " must be a whole number from " + describeRange(parameter) + " (got " + got +
                                   ").");
                case ParameterKind::Choice:
                    return refused(parameter.name + " has no choice " + got + ". The choices are: " + choicesText(parameter) +
                                   ".");
                case ParameterKind::Switch:
                    return refused(parameter.name + " must be on or off (got " + got + ").");
            }
            return refused(parameter.name + ": unusable value " + got + ".");
        }

        ValueResolution accepted(std::int64_t value)
        {
            return ValueResolution{value, {}};
        }

        bool oneOf(const std::string& text, std::initializer_list<const char*> options)
        {
            return std::any_of(options.begin(), options.end(), [&text](const char* option) { return text == option; });
        }
    }

    std::string normalizeText(std::string_view text)
    {
        std::string lowered(text);
        replaceAll(lowered, UTF8_MINUS_SIGN, "-");
        replaceAll(lowered, UTF8_EN_DASH, "-");
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
            return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        });

        std::string joined;
        std::size_t at = 0;
        while (at < lowered.size())
        {
            while (at < lowered.size() && (lowered[at] == ' ' || lowered[at] == '\t' || lowered[at] == '\r' || lowered[at] == '\n'))
                ++at;
            const std::size_t start = at;
            while (at < lowered.size() && lowered[at] != ' ' && lowered[at] != '\t' && lowered[at] != '\r' && lowered[at] != '\n')
                ++at;
            std::string word = lowered.substr(start, at - start);
            if (word.empty())
                continue;
            if (word == WORD_PLUS)
                word = "+";
            else if (word == WORD_MINUS)
                word = "-";
            else if (word == WORD_POLES)
                word = WORD_POLE;
            joined += word;
        }

        std::string normalized;
        for (std::size_t i = 0; i < joined.size(); ++i)
        {
            const char c = joined[i];
            const bool isFinalMinus = c == '-' && i + 1 == joined.size() && joined.size() > 1;
            if (c == '_' || (c == '-' && !isFinalMinus))
                continue;
            normalized += c;
        }
        return normalized;
    }

    const ParameterCatalogue& ParameterCatalogue::standard()
    {
        static const ParameterCatalogue catalogue(standardGroups(), standardParameters());
        return catalogue;
    }

    ParameterCatalogue::ParameterCatalogue(std::vector<GroupDefinition> groups, std::vector<ParameterDefinition> parameters)
        : _groups(std::move(groups)), _parameters(std::move(parameters))
    {
    }

    std::vector<const GroupDefinition*> ParameterCatalogue::findGroups(std::string_view text) const
    {
        const std::string wanted = normalizeText(text);
        std::vector<const GroupDefinition*> found;
        for (const GroupDefinition& group : _groups)
        {
            const bool named = normalizeText(group.name) == wanted;
            const bool aliased = std::any_of(group.aliases.begin(), group.aliases.end(),
                                             [&wanted](const std::string& alias) { return normalizeText(alias) == wanted; });
            if (!wanted.empty() && (named || aliased))
                found.push_back(&group);
        }
        return found;
    }

    std::vector<const ParameterDefinition*> ParameterCatalogue::parametersInGroup(const GroupDefinition& group) const
    {
        std::vector<const ParameterDefinition*> found;
        for (const ParameterDefinition& parameter : _parameters)
        {
            if (parameter.group == group.name)
                found.push_back(&parameter);
        }
        return found;
    }

    NameResolution ParameterCatalogue::resolveName(std::string_view text) const
    {
        NameResolution resolution;
        const std::string wanted = normalizeText(text);

        struct Candidate
        {
            std::size_t distance;
            std::size_t order;
            const ParameterDefinition* parameter;
        };
        std::vector<Candidate> candidates;
        const std::size_t budget = std::max(MIN_EDIT_BUDGET, wanted.size() / EDIT_BUDGET_DIVISOR);

        for (std::size_t order = 0; order < _parameters.size(); ++order)
        {
            const ParameterDefinition& parameter = _parameters[order];
            std::vector<std::string> spellings{normalizeText(parameter.name)};
            for (const std::string& alias : parameter.aliases)
                spellings.push_back(normalizeText(alias));

            std::size_t best = wanted.size() + spellings.front().size() + 1;
            for (const std::string& spelling : spellings)
            {
                if (spelling == wanted)
                {
                    resolution.parameter = &parameter;
                    return resolution;
                }
                const bool contained = wanted.size() >= MIN_SUBSTRING_QUERY && spelling.find(wanted) != std::string::npos;
                best = std::min(best, contained ? std::size_t{0} : editDistance(wanted, spelling));
            }
            if (!wanted.empty() && best <= budget)
                candidates.push_back({best, order, &parameter});
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.distance != b.distance ? a.distance < b.distance : a.order < b.order;
        });
        for (std::size_t i = 0; i < candidates.size() && i < MAX_SUGGESTIONS; ++i)
            resolution.suggestions.push_back(candidates[i].parameter->name);
        return resolution;
    }

    ValueResolution resolveValue(const ParameterDefinition& parameter, std::int64_t number)
    {
        const std::string got = numberText(number);
        switch (parameter.kind)
        {
            case ParameterKind::Number:
                if (number < parameter.min || number > parameter.max || (number - parameter.min) % parameter.step != 0)
                    return refusedValue(parameter, got);
                return accepted(number);
            case ParameterKind::Signed:
            case ParameterKind::Choice:
            case ParameterKind::Switch:
                if (number < parameter.min || number > parameter.max)
                    return refusedValue(parameter, got);
                return accepted(number);
        }
        return refusedValue(parameter, got);
    }

    ValueResolution resolveValue(const ParameterDefinition& parameter, std::string_view text)
    {
        const std::string said = trim(text);
        const std::string quoted = "'" + said + "'";
        if (said.empty())
            return refusedValue(parameter, "nothing");

        switch (parameter.kind)
        {
            case ParameterKind::Number:
            case ParameterKind::Signed:
            {
                const auto number = parseNumberWithUnit(parameter, said);
                return number ? resolveValue(parameter, *number) : refusedValue(parameter, quoted);
            }
            case ParameterKind::Choice:
            {
                const std::string wanted = normalizeText(said);
                for (std::size_t code = 0; code < parameter.labels.size(); ++code)
                {
                    if (normalizeText(parameter.labels[code]) == wanted)
                        return accepted(static_cast<std::int64_t>(code));
                }
                for (const auto& [alias, code] : parameter.choiceAliases)
                {
                    if (normalizeText(alias) == wanted)
                        return accepted(code);
                }
                const auto number = parseWholeNumber(said);
                return number ? resolveValue(parameter, *number) : refusedValue(parameter, quoted);
            }
            case ParameterKind::Switch:
            {
                const std::string wanted = normalizeText(said);
                if (oneOf(wanted, {TEXT_ON, "true", "yes", "1", "enabled", "enable"}))
                    return accepted(1);
                if (oneOf(wanted, {TEXT_OFF, "false", "no", "0", "disabled", "disable"}))
                    return accepted(0);
                return refusedValue(parameter, quoted);
            }
        }
        return refusedValue(parameter, quoted);
    }

    std::vector<std::int64_t> toItemValues(const ParameterDefinition& parameter, std::int64_t value)
    {
        switch (parameter.kind)
        {
            case ParameterKind::Number:
                return {value / parameter.step};
            case ParameterKind::Signed:
                return {value < 0 ? 1 : 0, std::abs(value)};
            case ParameterKind::Choice:
            case ParameterKind::Switch:
                return {value};
        }
        return {value};
    }

    std::optional<std::int64_t> fromItemValues(const ParameterDefinition& parameter, std::span<const std::int64_t> reply)
    {
        switch (parameter.kind)
        {
            case ParameterKind::Number:
                if (reply.size() != 1)
                    return std::nullopt;
                return reply[0] * parameter.step;
            case ParameterKind::Signed:
                if (reply.size() != 2)
                    return std::nullopt;
                return reply[0] != 0 ? -reply[1] : reply[1];
            case ParameterKind::Choice:
            case ParameterKind::Switch:
                if (reply.size() != 1)
                    return std::nullopt;
                return reply[0];
        }
        return std::nullopt;
    }

    std::string describeValue(const ParameterDefinition& parameter, std::int64_t value)
    {
        const bool hasLabel = value >= 0 && static_cast<std::size_t>(value) < parameter.labels.size();
        switch (parameter.kind)
        {
            case ParameterKind::Number:
                return numberText(value) + (parameter.unit.empty() ? "" : " " + parameter.unit);
            case ParameterKind::Signed:
                return numberText(value);
            case ParameterKind::Choice:
                return hasLabel ? parameter.labels[static_cast<std::size_t>(value)] + " (code " + numberText(value) + ")"
                                : numberText(value);
            case ParameterKind::Switch:
                return hasLabel ? parameter.labels[static_cast<std::size_t>(value)] : numberText(value);
        }
        return numberText(value);
    }

    std::string describeRange(const ParameterDefinition& parameter)
    {
        const std::string unit = parameter.unit.empty() ? "" : " " + parameter.unit;
        switch (parameter.kind)
        {
            case ParameterKind::Number:
            {
                if (parameter.step == 1)
                    return numberText(parameter.min) + " to " + numberText(parameter.max) + unit;
                std::string values;
                std::vector<std::int64_t> allowed;
                for (std::int64_t value = parameter.min; value <= parameter.max; value += parameter.step)
                    allowed.push_back(value);
                for (std::size_t i = 0; i < allowed.size(); ++i)
                {
                    if (i != 0)
                        values += i + 1 == allowed.size() ? " or " : ", ";
                    values += numberText(allowed[i]);
                }
                return values + unit;
            }
            case ParameterKind::Signed:
                return numberText(parameter.min) + " to " + numberText(parameter.max);
            case ParameterKind::Choice:
                return choicesText(parameter);
            case ParameterKind::Switch:
                return std::string(TEXT_ON) + " or " + TEXT_OFF;
        }
        return {};
    }
}
