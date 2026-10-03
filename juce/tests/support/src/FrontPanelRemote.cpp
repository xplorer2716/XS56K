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
#include "akm/harness/FrontPanelRemote.hpp"

#include <array>
#include <cctype>

namespace akm::harness
{
    namespace
    {
        constexpr int FUNCTION_KEYS_MAPPED = 8;
        constexpr int FIRST_PRINTABLE = 32;
        constexpr int LAST_PRINTABLE = 126;
        constexpr int ASCII_BACKSPACE = 8;
        constexpr int ASCII_ENTER = 13;
        constexpr int WHEEL_ONE_CLICK = 1;
        constexpr int WHEEL_PAGE_CLICKS = 8;
        constexpr char END_KEY = 'q';

        RemoteAction press(FrontPanelKey key)
        {
            RemoteAction action;
            action.kind = RemoteActionKind::Press;
            action.key = key;
            return action;
        }

        RemoteAction wheel(DataWheelDirection direction, int clicks)
        {
            RemoteAction action;
            action.kind = RemoteActionKind::Wheel;
            action.direction = direction;
            action.clicks = clicks;
            return action;
        }

        RemoteAction ascii(int character)
        {
            RemoteAction action;
            action.kind = RemoteActionKind::Ascii;
            action.ascii = character;
            return action;
        }

        RemoteAction ofKind(RemoteActionKind kind)
        {
            RemoteAction action;
            action.kind = kind;
            return action;
        }

        // F1 to F8 of the PC are the sampler's own F1 to F8.
        constexpr std::array<FrontPanelKey, FUNCTION_KEYS_MAPPED> FUNCTION_KEYS{
            FrontPanelKey::F1, FrontPanelKey::F2, FrontPanelKey::F3, FrontPanelKey::F4,
            FrontPanelKey::F5, FrontPanelKey::F6, FrontPanelKey::F7, FrontPanelKey::F8};

        // The digits of the PC are the sampler's, in order.
        constexpr std::array<FrontPanelKey, 10> DIGIT_KEYS{
            FrontPanelKey::Digit0, FrontPanelKey::Digit1, FrontPanelKey::Digit2, FrontPanelKey::Digit3, FrontPanelKey::Digit4,
            FrontPanelKey::Digit5, FrontPanelKey::Digit6, FrontPanelKey::Digit7, FrontPanelKey::Digit8, FrontPanelKey::Digit9};

        RemoteAction normalModeAction(int pcKey)
        {
            if (pcKey >= pc_key::F1 && pcKey < pc_key::F1 + FUNCTION_KEYS_MAPPED)
                return press(FUNCTION_KEYS[static_cast<std::size_t>(pcKey - pc_key::F1)]);
            if (pcKey >= '0' && pcKey <= '9')
                return press(DIGIT_KEYS[static_cast<std::size_t>(pcKey - '0')]);
            switch (pcKey)
            {
                case '-':
                    return press(FrontPanelKey::Minus);
                case '+':
                case '=':
                    return press(FrontPanelKey::Plus);
                case pc_key::LEFT:
                    return press(FrontPanelKey::CursorLeft);
                case pc_key::RIGHT:
                    return press(FrontPanelKey::CursorRight);
                case pc_key::ENTER:
                    return press(FrontPanelKey::EntPlay);
                case pc_key::ESCAPE:
                    return press(FrontPanelKey::Exit);
                case pc_key::SPACE:
                {
                    RemoteAction hold = ofKind(RemoteActionKind::ToggleHold);
                    hold.key = FrontPanelKey::EntPlay;
                    return hold;
                }
                case pc_key::UP:
                    return wheel(DataWheelDirection::Forwards, WHEEL_ONE_CLICK);
                case pc_key::DOWN:
                    return wheel(DataWheelDirection::Backwards, WHEEL_ONE_CLICK);
                case pc_key::PAGE_UP:
                    return wheel(DataWheelDirection::Forwards, WHEEL_PAGE_CLICKS);
                case pc_key::PAGE_DOWN:
                    return wheel(DataWheelDirection::Backwards, WHEEL_PAGE_CLICKS);
                case pc_key::TAB:
                    return ofKind(RemoteActionKind::ToggleTextMode);
                default:
                    break;
            }
            // Letters: the mode keys, read in either case, and the end key.
            if (pcKey < FIRST_PRINTABLE || pcKey > LAST_PRINTABLE)
                return ofKind(RemoteActionKind::None);
            switch (std::tolower(pcKey))
            {
                case 'm':
                    return press(FrontPanelKey::Multi);
                case 'x':
                    return press(FrontPanelKey::Fx);
                case 's':
                    return press(FrontPanelKey::EditSample);
                case 'p':
                    return press(FrontPanelKey::EditProgram);
                case 'r':
                    return press(FrontPanelKey::Record);
                case 'u':
                    return press(FrontPanelKey::Utilities);
                case 'v':
                    return press(FrontPanelKey::Save);
                case 'l':
                    return press(FrontPanelKey::Load);
                case 'w':
                    return press(FrontPanelKey::Window);
                case 'k':
                    return press(FrontPanelKey::Mark);
                case 'j':
                    return press(FrontPanelKey::Jump);
                case END_KEY:
                    return ofKind(RemoteActionKind::End);
                default:
                    return ofKind(RemoteActionKind::None);
            }
        }

        RemoteAction textModeAction(int pcKey)
        {
            if (pcKey == pc_key::TAB || pcKey == pc_key::ESCAPE)
                return ofKind(RemoteActionKind::LeaveTextMode);
            if (pcKey == pc_key::BACKSPACE)
                return ascii(ASCII_BACKSPACE);
            if (pcKey == pc_key::ENTER)
                return ascii(ASCII_ENTER);
            if (pcKey >= FIRST_PRINTABLE && pcKey <= LAST_PRINTABLE)
                return ascii(pcKey);
            return ofKind(RemoteActionKind::None);
        }
    }

    RemoteAction remoteAction(bool textMode, int pcKey)
    {
        return textMode ? textModeAction(pcKey) : normalModeAction(pcKey);
    }

    namespace
    {
        // The name the front panel gives each key of Table 31: one table, the one place a key's name is written.
        struct KeyName
        {
            FrontPanelKey key;
            std::string_view name;
        };
        constexpr std::array<KeyName, 43> KEY_NAMES{{
            {FrontPanelKey::Multi, "MULTI"},           {FrontPanelKey::Fx, "FX"},
            {FrontPanelKey::EditSample, "EDIT SAMPLE"}, {FrontPanelKey::EditProgram, "EDIT PROGRAM"},
            {FrontPanelKey::Record, "RECORD"},         {FrontPanelKey::Utilities, "UTILITIES"},
            {FrontPanelKey::Save, "SAVE"},             {FrontPanelKey::Load, "LOAD"},
            {FrontPanelKey::F1, "F1"},                 {FrontPanelKey::F2, "F2"},
            {FrontPanelKey::F3, "F3"},                 {FrontPanelKey::F4, "F4"},
            {FrontPanelKey::F5, "F5"},                 {FrontPanelKey::F6, "F6"},
            {FrontPanelKey::F7, "F7"},                 {FrontPanelKey::F8, "F8"},
            {FrontPanelKey::F9, "F9"},                 {FrontPanelKey::F10, "F10"},
            {FrontPanelKey::F11, "F11"},               {FrontPanelKey::F12, "F12"},
            {FrontPanelKey::F13, "F13"},               {FrontPanelKey::F14, "F14"},
            {FrontPanelKey::F15, "F15"},               {FrontPanelKey::F16, "F16"},
            {FrontPanelKey::Digit0, "0"},              {FrontPanelKey::Digit1, "1"},
            {FrontPanelKey::Digit2, "2"},              {FrontPanelKey::Digit3, "3"},
            {FrontPanelKey::Digit4, "4"},              {FrontPanelKey::Digit5, "5"},
            {FrontPanelKey::Digit6, "6"},              {FrontPanelKey::Digit7, "7"},
            {FrontPanelKey::Digit8, "8"},              {FrontPanelKey::Digit9, "9"},
            {FrontPanelKey::Minus, "-"},               {FrontPanelKey::Plus, "+"},
            {FrontPanelKey::CursorLeft, "CURSOR <"},   {FrontPanelKey::CursorRight, "CURSOR >"},
            {FrontPanelKey::Window, "WINDOW"},         {FrontPanelKey::Mark, "MARK"},
            {FrontPanelKey::Jump, "JUMP"},             {FrontPanelKey::Exit, "EXIT"},
            {FrontPanelKey::EntPlay, "ENT/PLAY"},
        }};
    }

    std::string_view remoteKeyName(FrontPanelKey key)
    {
        for (const KeyName& entry : KEY_NAMES)
        {
            if (entry.key == key)
                return entry.name;
        }
        return {};
    }

    const std::vector<std::string>& remoteMappingLines()
    {
        static const std::vector<std::string> LINES{
            "Normal mode (one PC key = one short press of the sampler key: Hold then Release)",
            "  F1 .. F8                F1 .. F8 of the sampler",
            "  0 .. 9                  digits 0 .. 9",
            "  -   +  (or =)           -   +",
            "  left / right arrow      CURSOR <  /  CURSOR >",
            "  Enter                   ENT/PLAY, a short press",
            "  Space                   ENT/PLAY held, then released by the next Space (to hear a sample)",
            "  Escape                  EXIT",
            "  m  x  s  p  r  u       MULTI  FX  EDIT SAMPLE  EDIT PROGRAM  RECORD  UTILITIES",
            "  v  l                    SAVE  LOAD",
            "  w  k  j                 WINDOW  MARK  JUMP",
            "  up / down arrow         data wheel forwards / backwards, 1 click",
            "  Page Up / Page Down     data wheel forwards / backwards, 8 clicks",
            "  Tab                     text mode",
            "  q                       end of the check: every key still held is released",
            "Text mode (after Tab)",
            "  a printable key         sent as ASCII (0-127)",
            "  Backspace / Enter       ASCII 8 / 13",
            "  Tab or Escape           back to the normal mode",
        };
        return LINES;
    }
}
