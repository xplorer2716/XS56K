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
#include <optional>
#include <string>

#include "app/Model.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// The bottom line of the main window: the connection state (a dot and a word, never the colour alone), the job in progress with a
    /// turning indicator, and the last problem until the person dismisses it. It draws what it is given; it holds no state of the sampler.
    /// [RQ-APP-010, RQ-APP-012, RQ-APP-013, RQ-APP-019, RQ-APP-005, TASK-APP-017]
    class StatusBar final : public juce::Component, private juce::Timer
    {
    public:
        explicit StatusBar(LookAndFeel& lookAndFeel);

        void show(const ConnectionStatus& connection, const std::optional<std::string>& busy, const std::optional<Problem>& problem);
        /// Called when the person dismisses the problem.
        std::function<void()> onDismiss;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void timerCallback() override;

        LookAndFeel& _lookAndFeel;
        ConnectionState _state = ConnectionState::Disconnected;
        bool _busy = false;
        float _angle = 0.0f;
        juce::Label _connection;
        juce::Label _job;
        juce::Label _problem;
        juce::TextButton _dismiss;
    };
}
