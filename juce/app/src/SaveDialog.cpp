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
#include "SaveDialog.hpp"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <utility>
#include <vector>

#include "ChoiceBox.hpp"
#include "CutLabel.hpp"
#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-043 to RQ-APP-045, ADR-APP-005 (DEC-APP-039), ADR-APP-002, TASK-APP-060]
    namespace
    {
        constexpr const char* TITLE = "Save to disk";
        constexpr const char* ITEM_LABEL = "ITEM";
        constexpr const char* SCOPE_LABEL = "SAVE";
        constexpr const char* DESTINATION_LABEL = "INTO THE CURRENT FOLDER";
        constexpr const char* CHECK_NAMES_TEXT = "Check names: ask before replacing a file";
        constexpr const char* NO_ITEM = "Choose an item to save.";
        constexpr const char* CANCEL_TEXT = "Cancel";
        constexpr const char* SAVE_TEXT = "Save";
        constexpr const char* KIND_SEPARATOR = ": ";
        constexpr int FIRST_ITEM_ID = 1;
        constexpr int ROWS = 10;  // three labels and their fields, the check box, the message, the buttons (two rows)

        constexpr std::array<SaveScope, 5> SCOPES{SaveScope::Alone, SaveScope::WithChildren, SaveScope::AllOfKind,
                                                  SaveScope::AllOfKindWithChildren, SaveScope::WholeMemory};

        juce::String scopeText(SaveScope scope)
        {
            using enum app::SaveScope;
            switch (scope)
            {
                case Alone:
                    return "This item alone";
                case WithChildren:
                    return "This item with its children";
                case AllOfKind:
                    return "All the items of its kind";
                case AllOfKindWithChildren:
                    return "All the items of its kind, with their children";
                case WholeMemory:
                    break;
            }
            return "The entire memory";
        }

        class SaveContent final : public juce::Component
        {
        public:
            SaveContent(LookAndFeel& lookAndFeel, const MemoryState& memory, const std::optional<MemoryItem>& proposed, const std::string& destination,
                        bool checkNames, std::function<void(const SaveRequest&, bool)> onSave)
                : _onSave(std::move(onSave))
            {
                using namespace tokens;
                for (auto [label, text] : {std::pair{&_itemLabel, ITEM_LABEL}, std::pair{&_scopeLabel, SCOPE_LABEL},
                                           std::pair{&_destinationLabel, DESTINATION_LABEL}})
                {
                    label->setText(text, juce::dontSendNotification);
                    label->setFont(lookAndFeel.font(kFontLabelSize, true));
                    label->setColour(juce::Label::textColourId, colourOf(kColourTextSecondary));
                    addAndMakeVisible(*label);
                }
                for (const MemoryKind kind : MEMORY_KINDS)
                {
                    for (const std::string& name : memory.list(kind).names)
                    {
                        _items.push_back(MemoryItem{kind, name});
                        const int id = FIRST_ITEM_ID + static_cast<int>(_items.size()) - 1;
                        _item.addItem(juce::String(std::string(memoryHeading(kind))) + KIND_SEPARATOR + name, id);
                        if (proposed && *proposed == _items.back())
                            _item.setSelectedId(id, juce::dontSendNotification);
                    }
                }
                for (std::size_t i = 0; i < SCOPES.size(); ++i)
                    _scope.addItem(scopeText(SCOPES[i]), FIRST_ITEM_ID + static_cast<int>(i));
                _scope.setSelectedId(FIRST_ITEM_ID, juce::dontSendNotification);
                _destination.setText(destination, juce::dontSendNotification);
                _destination.setFont(lookAndFeel.font(kFontValueSize));
                _checkNames.setButtonText(CHECK_NAMES_TEXT);
                _checkNames.setToggleState(checkNames, juce::dontSendNotification);
                _message.setFont(lookAndFeel.font(kFontCaptionSize));
                _message.setColour(juce::Label::textColourId, colourOf(kColourStatusError));
                _cancel.setButtonText(CANCEL_TEXT);
                _save.setButtonText(SAVE_TEXT);
                _cancel.onClick = [this] { close(); };
                _save.onClick = [this] { validate(); };
                for (juce::Component* component :
                     std::initializer_list<juce::Component*>{&_item, &_scope, &_destination, &_checkNames, &_message, &_cancel, &_save})
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
                _itemLabel.setBounds(row());
                _item.setBounds(row());
                _scopeLabel.setBounds(row());
                _scope.setBounds(row());
                _destinationLabel.setBounds(row());
                _destination.setBounds(row());
                _checkNames.setBounds(row());
                _message.setBounds(row());
                auto buttons = area.removeFromBottom(pixels(kButtonHeight));
                const int buttonWidth = pixels(kSizeLeftMinWidth) / 2;
                _save.setBounds(buttons.removeFromRight(buttonWidth));
                buttons.removeFromRight(pixels(kSpaceBetween));
                _cancel.setBounds(buttons.removeFromRight(buttonWidth));
            }

        private:
            void validate()
            {
                const int item = _item.getSelectedId() - FIRST_ITEM_ID;
                const int scope = _scope.getSelectedId() - FIRST_ITEM_ID;
                if (item < 0 || static_cast<std::size_t>(item) >= _items.size() || scope < 0 || static_cast<std::size_t>(scope) >= SCOPES.size())
                {
                    _message.setText(NO_ITEM, juce::dontSendNotification);
                    return;
                }
                auto onSave = std::move(_onSave);
                const SaveRequest request{_items[static_cast<std::size_t>(item)], SCOPES[static_cast<std::size_t>(scope)]};
                const bool checkNames = _checkNames.getToggleState();
                close();
                onSave(request, checkNames);
            }

            void close()
            {
                if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                    window->exitModalState(0);
            }

            std::function<void(const SaveRequest&, bool)> _onSave;
            std::vector<MemoryItem> _items;
            CutLabel _itemLabel;
            CutLabel _scopeLabel;
            CutLabel _destinationLabel;
            ChoiceBox _item;
            ChoiceBox _scope;
            CutLabel _destination;
            juce::ToggleButton _checkNames;
            CutLabel _message;
            juce::TextButton _cancel;
            juce::TextButton _save;
        };
    }

    void showSaveDialog(LookAndFeel& lookAndFeel, const MemoryState& memory, const std::optional<MemoryItem>& proposed, const std::string& destination,
                        bool checkNames, std::function<void(const SaveRequest&, bool)> onSave)
    {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = TITLE;
        options.content.setOwned(new SaveContent(lookAndFeel, memory, proposed, destination, checkNames, std::move(onSave)));
        options.dialogBackgroundColour = colourOf(tokens::kDialogSurface);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }
}
