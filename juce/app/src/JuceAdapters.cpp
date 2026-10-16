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

#include "CutLabel.hpp"
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
                // A destructor must not throw, and the answer has no one left to report to. [TASK-QLT-021, RQ-QLT-001]
                try
                {
                    if (_answer)
                        std::exchange(_answer, nullptr)(false);
                }
                catch (...)
                {
                    // Nothing can be done about a failing answer while the dialog is destroyed.
                }
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
            CutLabel _title;
            CutLabel _text;
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

    // [RQ-APP-085, ADR-APP-001 (DEC-APP-023), TASK-APP-033]
    namespace
    {
        constexpr const char* LOG_FOLDER = "log";
        constexpr const char* LOG_FILE = "XS56K.log";
        constexpr const char* APP_DATA_FOLDER = "XS56K";

        /// One line of `Logger` into the file.
        class FileLogSink final : public midiapp::service::ILogSink
        {
        public:
            FileLogSink(const juce::File& file, const juce::String& welcome) : _logger(file, welcome) {}
            void write(const std::string& line) override { _logger.logMessage(line); }

        private:
            juce::FileLogger _logger;
        };

        /// `log` beside the executable when it can be created and written, otherwise in the person's application-data folder.
        juce::File logFile()
        {
            const juce::File beside =
                juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile(LOG_FOLDER);
            if (beside.createDirectory().wasOk() && beside.hasWriteAccess())
                return beside.getChildFile(LOG_FILE);
            return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile(APP_DATA_FOLDER)
                .getChildFile(LOG_FOLDER)
                .getChildFile(LOG_FILE);
        }
    }

    LogSession::LogSession(const juce::String& version)
    {
        using namespace midiapp::service;
        const juce::File file = logFile();
        file.getParentDirectory().createDirectory();
        Logger::configure(std::make_unique<FileLogSink>(file, juce::String()));
        Logger::setLevel(TraceLevel::Info);
        XS56K_LOG(LogDomain::ControllerCalls, TraceLevel::Info, ("XS56K " + version + " started").toStdString());
    }

    LogSession::~LogSession()
    {
        using namespace midiapp::service;
        XS56K_LOG(LogDomain::ControllerCalls, TraceLevel::Info, std::string("XS56K stopped"));
        Logger::shutdown();
    }
}
