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
#include "TextPrompt.hpp"

#include <memory>
#include <utility>

namespace app::ui
{
    // [RQ-APP-030, RQ-APP-031, RQ-APP-038, RQ-APP-039, TASK-APP-018]
    namespace
    {
        constexpr const char* OK_TEXT = "OK";
        constexpr const char* CANCEL_TEXT = "Cancel";
        constexpr const char* FIELD_PREFIX = "field";
        constexpr int OK_RESULT = 1;
        constexpr int CANCEL_RESULT = 0;
    }

    void askTexts(const juce::String& title, const juce::String& message, std::vector<TextField> fields,
                  std::function<void(const std::vector<std::string>&)> onOk)
    {
        auto window = std::make_shared<juce::AlertWindow>(title, message, juce::MessageBoxIconType::NoIcon);
        for (std::size_t index = 0; index < fields.size(); ++index)
            window->addTextEditor(FIELD_PREFIX + juce::String(static_cast<int>(index)), fields[index].initial, fields[index].label);
        window->addButton(OK_TEXT, OK_RESULT, juce::KeyPress(juce::KeyPress::returnKey));
        window->addButton(CANCEL_TEXT, CANCEL_RESULT, juce::KeyPress(juce::KeyPress::escapeKey));
        // The callback owns the window: it is deleted once the callback has read the texts.
        window->enterModalState(true, juce::ModalCallbackFunction::create([window, count = fields.size(), onOk = std::move(onOk)](int result) {
                                    window->setVisible(false);
                                    if (result != OK_RESULT || !onOk)
                                        return;
                                    std::vector<std::string> texts;
                                    for (std::size_t index = 0; index < count; ++index)
                                        texts.push_back(
                                            window->getTextEditorContents(FIELD_PREFIX + juce::String(static_cast<int>(index))).trim().toStdString());
                                    onOk(texts);
                                }));
    }
}
