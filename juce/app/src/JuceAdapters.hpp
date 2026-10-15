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

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <memory>

#include "app/AppSettings.hpp"
#include "app/UiDispatcher.hpp"
#include "app/UserPrompts.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// The UI thread of the application: the JUCE message thread. [RQ-APP-012, ADR-APP-001 (DEC-APP-003), TASK-APP-017]
    class MessageThreadDispatcher final : public UiDispatcher
    {
    public:
        void post(std::function<void()> task) override;
    };

    /// The questions to the person, as dialogs that do not block the message thread: the answer comes back when a button is pressed or the
    /// dialog is closed (a close is a cancel). [RQ-APP-011, ADR-APP-001 (DEC-APP-007), TASK-APP-017]
    class DialogPrompts final : public UserPrompts
    {
    public:
        explicit DialogPrompts(LookAndFeel& lookAndFeel);

        void confirm(const Confirmation& question, std::function<void(bool)> answer) override;

    private:
        LookAndFeel& _lookAndFeel;
    };

    /// The person's settings in a `juce::PropertiesFile` of the settings folder of the operating system, saved at each write.
    /// [RQ-APP-016, ADR-APP-001 (DEC-APP-009), TASK-APP-017]
    class PropertiesSettings final : public AppSettings
    {
    public:
        PropertiesSettings();

        [[nodiscard]] std::optional<std::string> read(std::string_view key) const override;
        void write(std::string_view key, std::string value) override;

    private:
        std::unique_ptr<juce::PropertiesFile> _file;
    };
}
