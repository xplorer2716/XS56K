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

#include <cstdio>
#include <exception>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

#include "CutLabel.hpp"
#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-011, RQ-APP-012, RQ-APP-016, RQ-APP-061, ADR-APP-001 (DEC-APP-003, DEC-APP-007, DEC-APP-009), ADR-APP-004 (DEC-APP-034),
    // TASK-APP-017, TASK-APP-047]
    namespace
    {
        constexpr const char* SETTINGS_NAME = "XS56K";
        constexpr const char* SETTINGS_SUFFIX = ".settings";
        constexpr const char* MAC_SETTINGS_FOLDER = "Application Support";
        constexpr const char* CANCEL_TEXT = "Cancel";
        constexpr const char* CONFIRM_TEXT = "Confirm";
        constexpr int COUNT_DIGITS = 2;  // a count of keygroups: up to 99 [RQ-APP-061]
        constexpr const char* DIGITS = "0123456789";
        constexpr int TEXT_LINES = 3;
        constexpr const char* CLOSED_DIALOG_FAILED = "The answer of a closed dialog failed";
        constexpr const char* LOG_STOP_FAILED = "The log could not be closed";

        /// Logs a failure met in a destructor; when the log itself fails, the text goes to the standard error. Never throws. [TASK-QLT-025]
        void logFromDestructor(const char* text, const char* detail = nullptr) noexcept
        {
            try
            {
                using namespace midiapp::service;
                XS56K_LOG(LogDomain::ControllerCalls, TraceLevel::Error, detail == nullptr ? std::string(text) : std::string(text) + ": " + detail);
            }
            catch (...)
            {
                std::fputs(text, stderr);
            }
        }

        /// A dialog's content that answers once: on a button, or with `cancelled` when its window closes. [RQ-APP-011, DEC-APP-007]
        template <typename Answer>
        class AnsweredContent : public juce::Component
        {
        public:
            ~AnsweredContent() override
            {
                // Closed by its window: a cancel; the window is going, so it is not asked to end its modal state.
                // A destructor must not throw, and the answer has no one left to report to. [TASK-QLT-021, RQ-QLT-001]
                try
                {
                    if (_answer)
                        std::exchange(_answer, nullptr)(_cancelled);
                }
                catch (const std::exception& error)
                {
                    // Nothing can be done about a failing answer while the dialog is destroyed: it is logged.
                    logFromDestructor(CLOSED_DIALOG_FAILED, error.what());
                }
                catch (...)
                {
                    logFromDestructor(CLOSED_DIALOG_FAILED);
                }
            }

        protected:
            AnsweredContent(std::function<void(Answer)> answer, Answer cancelled) : _answer(std::move(answer)), _cancelled(std::move(cancelled)) {}

            void finish(Answer given)
            {
                if (!_answer)
                    return;
                auto answer = std::exchange(_answer, nullptr);
                answer(std::move(given));
                if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                    window->exitModalState(0);
            }

            /// The title, the text under it and the two buttons, laid out as every dialog of the application; `field`, when given, between
            /// the text and the buttons.
            void layOut(juce::Component* field)
            {
                using namespace tokens;
                auto area = getLocalBounds().reduced(pixels(kSpaceGroup));
                title.setBounds(area.removeFromTop(pixels(kRowHeight)));
                area.removeFromTop(pixels(kSpaceBetween));
                auto buttons = area.removeFromBottom(pixels(kButtonHeight));
                area.removeFromBottom(pixels(kSpaceBetween));
                const int buttonWidth = pixels(kSizeLeftMinWidth) / 2;
                if (field != nullptr)
                {
                    field->setBounds(area.removeFromBottom(pixels(kValueboxHeight)).withWidth(buttonWidth));
                    area.removeFromBottom(pixels(kSpaceBetween));
                }
                text.setBounds(area);
                confirm.setBounds(buttons.removeFromRight(buttonWidth));
                buttons.removeFromRight(pixels(kSpaceBetween));
                cancel.setBounds(buttons.removeFromRight(buttonWidth));
            }

            void setUp(LookAndFeel& look, const std::string& heading, const std::string& body, const char* confirmText)
            {
                using namespace tokens;
                title.setText(heading, juce::dontSendNotification);
                title.setFont(look.font(kFontHeadingSize, true));
                text.setText(body, juce::dontSendNotification);
                text.setFont(look.font(kFontValueSize));
                text.setJustificationType(juce::Justification::topLeft);
                text.setMinimumHorizontalScale(1.0f);
                cancel.setButtonText(CANCEL_TEXT);
                confirm.setButtonText(confirmText);
                for (juce::Component* part : std::initializer_list<juce::Component*>{&title, &text, &cancel, &confirm})
                    addAndMakeVisible(*part);
            }

            CutLabel title;
            CutLabel text;
            juce::TextButton cancel;
            juce::TextButton confirm;

        private:
            std::function<void(Answer)> _answer;
            Answer _cancelled;
        };

        /// The confirmation: its question as the title, the object and what is lost, and two buttons; the confirm button is the destructive
        /// one of the design system. Answers once, on a button or on the window's close.
        class ConfirmationContent final : public AnsweredContent<bool>
        {
        public:
            ConfirmationContent(LookAndFeel& lookAndFeel, const Confirmation& question, std::function<void(bool)> answer)
                : AnsweredContent<bool>(std::move(answer), false)
            {
                using namespace tokens;
                setUp(lookAndFeel, question.question, question.consequence, CONFIRM_TEXT);
                confirm.getProperties().set(DESTRUCTIVE_PROPERTY, true);
                cancel.onClick = [this] { finish(false); };
                confirm.onClick = [this] { finish(true); };
                const int lineHeight = pixels(kRowHeight);
                setSize(pixels(kDialogWidth), pixels(kSpaceGroup) * 2 + lineHeight * (TEXT_LINES + 1) + pixels(kSpaceBetween) * 2 +
                                                  pixels(kButtonHeight));
            }

            void resized() override { layOut(nullptr); }
        };

        /// A count asked: the question as the title, what it is for, a field holding the count offered, and two buttons. A text that is
        /// not a count from `min` to `max` keeps the dialog open, the field in the pending colour. [RQ-APP-061, DEC-APP-034, TASK-APP-047]
        class CountContent final : public AnsweredContent<std::optional<int>>
        {
        public:
            CountContent(LookAndFeel& lookAndFeel, const CountQuestion& question, std::function<void(std::optional<int>)> answer)
                : AnsweredContent<std::optional<int>>(std::move(answer), std::nullopt), _min(question.min), _max(question.max)
            {
                using namespace tokens;
                setUp(lookAndFeel, question.question, question.text, question.action.c_str());
                _field.setFont(lookAndFeel.font(kFontValueSize));
                _field.setJustification(juce::Justification::centred);
                _field.setInputRestrictions(COUNT_DIGITS, DIGITS);
                _field.setText(juce::String(question.initial), juce::dontSendNotification);
                _field.setColour(juce::TextEditor::backgroundColourId, colourOf(kValueboxSurface));
                _field.setColour(juce::TextEditor::textColourId, colourOf(kValueboxText));
                _field.onReturnKey = [this] { given(); };
                addAndMakeVisible(_field);
                cancel.onClick = [this] { finish(std::nullopt); };
                confirm.onClick = [this] { given(); };
                const int lineHeight = pixels(kRowHeight);
                setSize(pixels(kDialogWidth), pixels(kSpaceGroup) * 2 + lineHeight * (TEXT_LINES + 1) + pixels(kSpaceBetween) * 3 +
                                                  pixels(kValueboxHeight) + pixels(kButtonHeight));
            }

            void resized() override { layOut(&_field); }

        private:
            void given()
            {
                const int count = _field.getText().getIntValue();
                if (_field.getText().isEmpty() || count < _min || count > _max)
                {
                    _field.setColour(juce::TextEditor::textColourId, colourOf(tokens::kColourStatusPending));  // not a count it takes
                    _field.repaint();
                    return;
                }
                finish(count);
            }

            int _min;
            int _max;
            juce::TextEditor _field;
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

    void DialogPrompts::askCount(const CountQuestion& question, std::function<void(std::optional<int>)> answer)
    {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = question.question;
        options.content.setOwned(new CountContent(_lookAndFeel, question, std::move(answer)));
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
        try  // a destructor must not throw [TASK-QLT-025]
        {
            using namespace midiapp::service;
            XS56K_LOG(LogDomain::ControllerCalls, TraceLevel::Info, std::string("XS56K stopped"));
            Logger::shutdown();
        }
        catch (...)
        {
            std::fputs(LOG_STOP_FAILED, stderr);
        }
    }
}
