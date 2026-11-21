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
#include "MidiSetupDialog.hpp"

#include <juce_gui_extra/juce_gui_extra.h>

#include <string>
#include <utility>
#include <vector>

#include "ChoiceBox.hpp"
#include "CutLabel.hpp"
#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-015, RQ-APP-016, ADR-APP-001 (DEC-APP-009), TASK-APP-017]
    namespace
    {
        constexpr const char* TITLE = "MIDI setup";
        constexpr const char* INPUT_LABEL = "MIDI INPUT (THE SAMPLER SENDS ON IT)";
        constexpr const char* OUTPUT_LABEL = "MIDI OUTPUT (THE SAMPLER RECEIVES ON IT)";
        constexpr const char* DEVICE_ID_LABEL = "SAMPLER DEVICEID (0 TO 31)";
        constexpr const char* IN_USE = " (in use)";
        constexpr const char* NO_PORT = "Choose an input port and an output port.";
        constexpr const char* NOT_A_NUMBER = "The DeviceID is a number from 0 to 31.";
        constexpr const char* CANCEL_TEXT = "Cancel";
        constexpr const char* OK_TEXT = "OK";
        constexpr const char* DIGITS = "0123456789";
        constexpr int DEVICE_ID_DIGITS = 2;
        constexpr int FIRST_ITEM_ID = 1;
        constexpr int ROWS = 8;  // three labels, three fields, the message, the buttons

        class MidiSetupContent final : public juce::Component
        {
        public:
            MidiSetupContent(LookAndFeel& lookAndFeel, const MidiPortLists& ports, const std::optional<MidiSetup>& current,
                             std::function<void(const MidiSetup&)> onValidated)
                : _ports(ports), _onValidated(std::move(onValidated))
            {
                using namespace tokens;
                for (auto [label, text] : {std::pair{&_inputLabel, INPUT_LABEL}, std::pair{&_outputLabel, OUTPUT_LABEL},
                                           std::pair{&_deviceIdLabel, DEVICE_ID_LABEL}})
                {
                    label->setText(text, juce::dontSendNotification);
                    label->setFont(lookAndFeel.font(kFontLabelSize, true));
                    label->setColour(juce::Label::textColourId, colourOf(kColourTextSecondary));
                    addAndMakeVisible(*label);
                }
                fill(_input, _ports.inputs, current ? current->inputPort : std::string());
                fill(_output, _ports.outputs, current ? current->outputPort : std::string());
                _deviceId.setInputRestrictions(DEVICE_ID_DIGITS, DIGITS);
                _deviceId.setFont(lookAndFeel.font(kFontValueSize));
                _deviceId.setText(juce::String(current ? current->deviceId : MIN_DEVICE_ID), false);
                _message.setFont(lookAndFeel.font(kFontCaptionSize));
                _message.setColour(juce::Label::textColourId, colourOf(kColourStatusError));
                _cancel.setButtonText(CANCEL_TEXT);
                _ok.setButtonText(OK_TEXT);
                _cancel.onClick = [this] { close(); };
                _ok.onClick = [this] { validate(); };
                for (juce::Component* component : std::initializer_list<juce::Component*>{&_input, &_output, &_deviceId, &_message, &_cancel, &_ok})
                    addAndMakeVisible(*component);
                setSize(pixels(kDialogWidth), pixels(kSpaceGroup) * 2 + pixels(kRowHeight) * ROWS + pixels(kSpaceBetween) * (ROWS - 1));
            }

            void resized() override
            {
                using namespace tokens;
                auto area = getLocalBounds().reduced(pixels(kSpaceGroup));
                const auto row = [&area] {
                    auto taken = area.removeFromTop(pixels(kRowHeight));
                    area.removeFromTop(pixels(kSpaceBetween));
                    return taken;
                };
                _inputLabel.setBounds(row());
                _input.setBounds(row());
                _outputLabel.setBounds(row());
                _output.setBounds(row());
                _deviceIdLabel.setBounds(row());
                _deviceId.setBounds(row().removeFromLeft(pixels(kKnobSizeDefault) + pixels(kSizeSpace8)));
                _message.setBounds(row());
                auto buttons = area.removeFromBottom(pixels(kButtonHeight));
                const int buttonWidth = pixels(kSizeLeftMinWidth) / 2;
                _ok.setBounds(buttons.removeFromRight(buttonWidth));
                buttons.removeFromRight(pixels(kSpaceBetween));
                _cancel.setBounds(buttons.removeFromRight(buttonWidth));
            }

        private:
            static void fill(juce::ComboBox& box, const std::vector<std::string>& names, const std::string& inUse)
            {
                for (std::size_t i = 0; i < names.size(); ++i)
                {
                    const bool used = names[i] == inUse;
                    box.addItem(juce::String(names[i]) + (used ? IN_USE : ""), FIRST_ITEM_ID + static_cast<int>(i));
                    if (used)
                        box.setSelectedId(FIRST_ITEM_ID + static_cast<int>(i), juce::dontSendNotification);
                }
            }

            static std::optional<std::string> chosen(const juce::ComboBox& box, const std::vector<std::string>& names)
            {
                const int index = box.getSelectedId() - FIRST_ITEM_ID;
                if (index < 0 || static_cast<std::size_t>(index) >= names.size())
                    return std::nullopt;
                return names[static_cast<std::size_t>(index)];
            }

            void validate()
            {
                const auto input = chosen(_input, _ports.inputs);
                const auto output = chosen(_output, _ports.outputs);
                if (!input || !output)
                {
                    _message.setText(NO_PORT, juce::dontSendNotification);
                    return;
                }
                const juce::String text = _deviceId.getText().trim();
                if (text.isEmpty() || !text.containsOnly(DIGITS))
                {
                    _message.setText(NOT_A_NUMBER, juce::dontSendNotification);
                    return;
                }
                const int deviceId = text.getIntValue();
                if (const auto problem = deviceIdProblem(deviceId))
                {
                    _message.setText(*problem, juce::dontSendNotification);
                    return;
                }
                auto onValidated = std::move(_onValidated);
                const MidiSetup setup{*input, *output, deviceId};
                close();
                onValidated(setup);
            }

            void close()
            {
                if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                    window->exitModalState(0);
            }

            MidiPortLists _ports;
            std::function<void(const MidiSetup&)> _onValidated;
            CutLabel _inputLabel;
            CutLabel _outputLabel;
            CutLabel _deviceIdLabel;
            ChoiceBox _input;
            ChoiceBox _output;
            juce::TextEditor _deviceId;
            CutLabel _message;
            juce::TextButton _cancel;
            juce::TextButton _ok;
        };
    }

    void showMidiSetupDialog(LookAndFeel& lookAndFeel, const common::midi::MidiBackend& backend, const std::optional<MidiSetup>& current,
                             std::function<void(const MidiSetup&)> onValidated)
    {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = TITLE;
        options.content.setOwned(new MidiSetupContent(lookAndFeel, listMidiPorts(backend), current, std::move(onValidated)));
        options.dialogBackgroundColour = colourOf(tokens::kDialogSurface);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }
}
