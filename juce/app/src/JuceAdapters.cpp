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
#include "JuceAdapters.hpp"

#include <utility>

#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-011, RQ-APP-012, RQ-APP-016, ADR-APP-001 (DEC-APP-003, DEC-APP-007, DEC-APP-009), TASK-APP-017]
    namespace
    {
        constexpr const char* SETTINGS_NAME = "XS56K";
        constexpr const char* SETTINGS_SUFFIX = ".settings";
        constexpr const char* MAC_SETTINGS_FOLDER = "Application Support";
        constexpr const char* CANCEL_TEXT = "Cancel";
        constexpr const char* CONFIRM_TEXT = "Confirm";
        constexpr int TEXT_LINES = 3;

        /// The confirmation: its question as the title, the object and what is lost, and two buttons; the confirm button is the destructive
        /// one of the design system. Answers once, on a button or on the window's close.
        class ConfirmationContent final : public juce::Component
        {
        public:
            ConfirmationContent(LookAndFeel& lookAndFeel, const Confirmation& question, std::function<void(bool)> answer)
                : _answer(std::move(answer))
            {
                using namespace tokens;
                _title.setText(question.question, juce::dontSendNotification);
                _title.setFont(lookAndFeel.font(kFontHeadingSize, true));
                _text.setText(question.consequence, juce::dontSendNotification);
                _text.setFont(lookAndFeel.font(kFontValueSize));
                _text.setJustificationType(juce::Justification::topLeft);
                _text.setMinimumHorizontalScale(1.0f);
                _cancel.setButtonText(CANCEL_TEXT);
                _confirm.setButtonText(CONFIRM_TEXT);
                _confirm.getProperties().set(DESTRUCTIVE_PROPERTY, true);
                _cancel.onClick = [this] { finish(false); };
                _confirm.onClick = [this] { finish(true); };
                addAndMakeVisible(_title);
                addAndMakeVisible(_text);
                addAndMakeVisible(_cancel);
                addAndMakeVisible(_confirm);
                const int lineHeight = pixels(kRowHeight);
                setSize(pixels(kDialogWidth), pixels(kSpaceGroup) * 2 + lineHeight * (TEXT_LINES + 1) + pixels(kSpaceBetween) * 2 +
                                                  pixels(kButtonHeight));
            }

            ~ConfirmationContent() override
            {
                // Closed by its window: a cancel; the window is going, so it is not asked to end its modal state.
                if (_answer)
                    std::exchange(_answer, nullptr)(false);
            }

            void resized() override
            {
                using namespace tokens;
                auto area = getLocalBounds().reduced(pixels(kSpaceGroup));
                _title.setBounds(area.removeFromTop(pixels(kRowHeight)));
                area.removeFromTop(pixels(kSpaceBetween));
                auto buttons = area.removeFromBottom(pixels(kButtonHeight));
                area.removeFromBottom(pixels(kSpaceBetween));
                _text.setBounds(area);
                const int buttonWidth = pixels(kSizeLeftMinWidth) / 2;
                _confirm.setBounds(buttons.removeFromRight(buttonWidth));
                buttons.removeFromRight(pixels(kSpaceBetween));
                _cancel.setBounds(buttons.removeFromRight(buttonWidth));
            }

        private:
            void finish(bool confirmed)
            {
                if (!_answer)
                    return;
                auto answer = std::exchange(_answer, nullptr);
                answer(confirmed);
                if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                    window->exitModalState(0);
            }

            std::function<void(bool)> _answer;
            juce::Label _title;
            juce::Label _text;
            juce::TextButton _cancel;
            juce::TextButton _confirm;
        };
    }

    void MessageThreadDispatcher::post(std::function<void()> task)
    {
        juce::MessageManager::callAsync(std::move(task));
    }

    DialogPrompts::DialogPrompts(LookAndFeel& lookAndFeel) : _lookAndFeel(lookAndFeel) {}

    void DialogPrompts::confirm(const Confirmation& question, std::function<void(bool)> answer)
    {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = question.question;
        options.content.setOwned(new ConfirmationContent(_lookAndFeel, question, std::move(answer)));
        options.dialogBackgroundColour = colourOf(tokens::kDialogSurface);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

    PropertiesSettings::PropertiesSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = SETTINGS_NAME;
        options.folderName = SETTINGS_NAME;
        options.filenameSuffix = SETTINGS_SUFFIX;
        options.osxLibrarySubFolder = MAC_SETTINGS_FOLDER;
        _file = std::make_unique<juce::PropertiesFile>(options);
    }

    std::optional<std::string> PropertiesSettings::read(std::string_view key) const
    {
        const juce::String name{std::string(key)};
        if (!_file->containsKey(name))
            return std::nullopt;
        return _file->getValue(name).toStdString();
    }

    void PropertiesSettings::write(std::string_view key, std::string value)
    {
        _file->setValue(juce::String(std::string(key)), juce::String(value));
        _file->saveIfNeeded();
    }
}
