/*
 * XS56K - Editor for AKAI S5000/S6000 samplers
 * Copyright (C) 2026 xplorer2716
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// The keys unit of the sampler gateway: the front panel (section 20) - keys, data wheel, ASCII keys. It is the only unit that may call the front-panel
// primitives (checked by `CheckNoDestructiveCalls.cmake`), because a key can answer "ENT" to a delete or save screen and no `confirm` of this server
// guards it: the tools that reach it exist only with `--allow-front-panel`. A key held is released by the AKM session when it closes.
// [TASK-MCP-053, RQ-MCP-054, RQ-MCP-057, ADR-MCP-005 (DEC-MCP-032)]
#include <algorithm>
#include <functional>
#include <utility>

#include "GatewayDetail.hpp"
#include "akm/FrontPanel.hpp"
#include "akm/SamplerError.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"

namespace mcp
{
    using detail::await;
    using detail::explain;
    using detail::numberText;

    namespace
    {
        constexpr const char* SESSION_TIMED_OUT = "The sampler session did not complete the command in time.";
        // A press is a hold then a release: two commands, each waiting for the sampler.
        constexpr int COMMANDS_IN_A_PRESS = 2;

        struct PanelKey
        {
            const char* name;
            akm::FrontPanelKey key;
        };

        // Table 31, in the order of the specification: mode keys, function keys, numeric keys, the others.
        constexpr PanelKey PANEL_KEYS[] = {
            {"multi", akm::FrontPanelKey::Multi},
            {"fx", akm::FrontPanelKey::Fx},
            {"edit_sample", akm::FrontPanelKey::EditSample},
            {"edit_program", akm::FrontPanelKey::EditProgram},
            {"record", akm::FrontPanelKey::Record},
            {"utilities", akm::FrontPanelKey::Utilities},
            {"save", akm::FrontPanelKey::Save},
            {"load", akm::FrontPanelKey::Load},
            {"f1", akm::FrontPanelKey::F1},
            {"f2", akm::FrontPanelKey::F2},
            {"f3", akm::FrontPanelKey::F3},
            {"f4", akm::FrontPanelKey::F4},
            {"f5", akm::FrontPanelKey::F5},
            {"f6", akm::FrontPanelKey::F6},
            {"f7", akm::FrontPanelKey::F7},
            {"f8", akm::FrontPanelKey::F8},
            {"f9", akm::FrontPanelKey::F9},
            {"f10", akm::FrontPanelKey::F10},
            {"f11", akm::FrontPanelKey::F11},
            {"f12", akm::FrontPanelKey::F12},
            {"f13", akm::FrontPanelKey::F13},
            {"f14", akm::FrontPanelKey::F14},
            {"f15", akm::FrontPanelKey::F15},
            {"f16", akm::FrontPanelKey::F16},
            {"0", akm::FrontPanelKey::Digit0},
            {"1", akm::FrontPanelKey::Digit1},
            {"2", akm::FrontPanelKey::Digit2},
            {"3", akm::FrontPanelKey::Digit3},
            {"4", akm::FrontPanelKey::Digit4},
            {"5", akm::FrontPanelKey::Digit5},
            {"6", akm::FrontPanelKey::Digit6},
            {"7", akm::FrontPanelKey::Digit7},
            {"8", akm::FrontPanelKey::Digit8},
            {"9", akm::FrontPanelKey::Digit9},
            {"minus", akm::FrontPanelKey::Minus},
            {"plus", akm::FrontPanelKey::Plus},
            {"cursor_left", akm::FrontPanelKey::CursorLeft},
            {"cursor_right", akm::FrontPanelKey::CursorRight},
            {"window", akm::FrontPanelKey::Window},
            {"mark", akm::FrontPanelKey::Mark},
            {"jump", akm::FrontPanelKey::Jump},
            {"exit", akm::FrontPanelKey::Exit},
            {"ent_play", akm::FrontPanelKey::EntPlay},
        };

        std::string keyOf(std::string_view text)
        {
            std::string spaced(text);
            std::replace(spaced.begin(), spaced.end(), '_', ' ');
            return normalizeText(spaced);
        }

        const PanelKey* findKey(std::string_view name)
        {
            const std::string wanted = keyOf(name);
            for (const PanelKey& key : PANEL_KEYS)
            {
                if (keyOf(key.name) == wanted)
                    return &key;
            }
            return nullptr;
        }

        std::string unknownKey(std::string_view name)
        {
            std::string list;
            for (const std::string& known : panelKeyNames())
                list += (list.empty() ? "" : ", ") + known;
            return "No front-panel key is named \"" + std::string(name) + "\". The keys are: " + list + ".";
        }
    }

    std::vector<std::string> panelKeyNames()
    {
        std::vector<std::string> names;
        for (const PanelKey& key : PANEL_KEYS)
            names.emplace_back(key.name);
        return names;
    }

    Outcome<std::string> SamplerGateway::pressPanelKey(std::string_view name)
    {
        const PanelKey* key = findKey(name);
        if (key == nullptr)
            return Outcome<std::string>::failure(unknownKey(name));
        if (const auto problem = connect())
            return Outcome<std::string>::failure(*problem);
        const auto pressed = await<akm::KeyPressResult>(waitFor(COMMANDS_IN_A_PRESS), [&](std::function<void(const akm::KeyPressResult&)> done) {
            akm::pressKey(session(), key->key, std::move(done));
        });
        if (!pressed)
            return Outcome<std::string>::failure(SESSION_TIMED_OUT);
        const std::string doing = std::string("pressing the key \"") + key->name + "\"";
        if (!akm::succeeded(pressed->hold))
            return Outcome<std::string>::failure(explain(pressed->hold, doing, _config, false));
        if (!akm::succeeded(pressed->release))
            return Outcome<std::string>::failure(explain(pressed->release, "releasing the key \"" + std::string(key->name) + "\" after pressing it", _config, false));
        return Outcome<std::string>::success(key->name);
    }

    Outcome<std::string> SamplerGateway::holdPanelKey(std::string_view name)
    {
        const PanelKey* key = findKey(name);
        if (key == nullptr)
            return Outcome<std::string>::failure(unknownKey(name));
        if (const auto problem = connect())
            return Outcome<std::string>::failure(*problem);
        const auto held = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::holdKey(session(), key->key, std::move(done));
        });
        if (!held)
            return Outcome<std::string>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*held))
            return Outcome<std::string>::failure(explain(*held, std::string("holding the key \"") + key->name + "\"", _config, false));
        return Outcome<std::string>::success(key->name);
    }

    Outcome<std::string> SamplerGateway::releasePanelKey(std::string_view name)
    {
        const PanelKey* key = findKey(name);
        if (key == nullptr)
            return Outcome<std::string>::failure(unknownKey(name));
        if (const auto problem = connect())
            return Outcome<std::string>::failure(*problem);
        const auto released = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::releaseKey(session(), key->key, std::move(done));
        });
        if (!released)
            return Outcome<std::string>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*released))
            return Outcome<std::string>::failure(explain(*released, std::string("releasing the key \"") + key->name + "\"", _config, false));
        return Outcome<std::string>::success(key->name);
    }

    Outcome<bool> SamplerGateway::turnDataWheel(WheelDirection direction, int clicks)
    {
        if (const auto problem = connect())
            return Outcome<bool>::failure(*problem);
        const akm::DataWheelDirection wire = direction == WheelDirection::Forwards ? akm::DataWheelDirection::Forwards : akm::DataWheelDirection::Backwards;
        const auto turned = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::moveDataWheel(session(), wire, clicks, std::move(done));
        });
        if (!turned)
            return Outcome<bool>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*turned))
            return Outcome<bool>::failure(explain(*turned, "turning the data wheel by " + numberText(clicks) + " clicks", _config, false));
        return Outcome<bool>::success(true);
    }

    Outcome<bool> SamplerGateway::sendAsciiKey(int ascii)
    {
        if (const auto problem = connect())
            return Outcome<bool>::failure(*problem);
        const auto sent = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::sendAsciiKey(session(), ascii, std::move(done));
        });
        if (!sent)
            return Outcome<bool>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*sent))
            return Outcome<bool>::failure(explain(*sent, "sending the ASCII character " + numberText(ascii), _config, false));
        return Outcome<bool>::success(true);
    }
}
