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
#include "MemoryView.hpp"

#include <string>
#include <utility>

#include "LookAndFeel.hpp"
#include "TextPrompt.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-035 to RQ-APP-042, RQ-APP-005, ADR-APP-001 (DEC-APP-001), ADR-APP-002 (DEC-APP-013, DEC-APP-014), TASK-APP-019]
    namespace
    {
        constexpr const char* REFRESH_TEXT = "Refresh";
        constexpr const char* NAME_LABEL = "Name";
        constexpr const char* KEYGROUPS_LABEL = "Keygroups (1 to 99)";
        constexpr const char* ONE_KEYGROUP = "1";
        constexpr const char* NEW_PROGRAM_TITLE = "Create a program";
        constexpr const char* NEW_PROGRAM_ACTION = "Creating a program";
        constexpr const char* BAD_KEYGROUPS = "The number of keygroups is a number from 1 to 99.";
        constexpr const char* NO_NAME = "A program needs a name.";
        constexpr const char* DIGITS = "0123456789";
        constexpr int MIN_KEYGROUPS = 1;
        constexpr int MAX_KEYGROUPS = 99;
        constexpr std::size_t NAME_FIELD = 0;
        constexpr std::size_t KEYGROUPS_FIELD = 1;
        constexpr int REFRESH_SHARE = 3;  // the refresh button takes a third of the left column's width

        enum MenuId
        {
            CREATE_PROGRAM_ID = 1,
            RENAME_ID,
            DELETE_ID,
            AUDITION_ID,
            STOP_AUDITION_ID,
        };

        RowIcon iconOf(MemoryKind kind)
        {
            switch (kind)
            {
                case MemoryKind::Multi:
                    return RowIcon::Multi;
                case MemoryKind::Program:
                    return RowIcon::Program;
                case MemoryKind::Sample:
                    return RowIcon::Sample;
                case MemoryKind::SongFile:
                    return RowIcon::SongFile;
                case MemoryKind::SetList:
                    return RowIcon::SetList;
                case MemoryKind::SceneList:
                    break;
            }
            return RowIcon::SceneList;
        }

        /// "2.0 MB free of 8.0 MB (25 %), MPKS 40 % free", or with the exact counts of bytes for the tooltip. [RQ-APP-042, RQ-APP-081]
        juce::String freeText(const std::optional<FreeMemory>& free, std::string (*size)(std::uint64_t))
        {
            if (!free)
                return {};
            return juce::String(size(free->freeWaveBytes)) + " free of " + juce::String(size(free->totalWaveBytes)) + " (" +
                   juce::String(free->freeWavePercent) + " %), MPKS " + juce::String(free->freeMpksPercent) + " % free";
        }
    }

    MemoryView::MemoryView(LookAndFeel& lookAndFeel, Model& model, MemoryController& memory)
        : _lookAndFeel(lookAndFeel), _model(model), _memory(memory), _list(lookAndFeel)
    {
        _list.onClick = [this](int row) {
            if (const auto& item = _items[static_cast<std::size_t>(row)])
                _memory.select(*item);
        };
        _list.onMenu = [this](int row) { showMenu(row); };
        _refresh.setButtonText(REFRESH_TEXT);
        _refresh.onClick = [this] { _memory.refresh(); };
        addAndMakeVisible(_list);
        addAndMakeVisible(_refresh);
        _free.setFont(_lookAndFeel.font(tokens::kFontCaptionSize));
        _free.setColour(juce::Label::textColourId, colourOf(tokens::kColourTextSecondary));
        _free.setMinimumHorizontalScale(1.0f);
        _free.setBorderSize({});
        addAndMakeVisible(_free);
        _model.addListener(*this);
        rebuild();
    }

    MemoryView::~MemoryView()
    {
        _model.removeListener(*this);
    }

    void MemoryView::modelChanged(ModelPart part)
    {
        if (part == ModelPart::Memory)
            rebuild();
    }

    void MemoryView::rebuild()
    {
        const MemoryState& state = _model.memory();
        std::vector<ItemRow> rows;
        _items.clear();
        for (const MemoryKind kind : MEMORY_KINDS)
        {
            const MemoryList& list = state.list(kind);
            ItemRow heading;
            heading.heading = true;
            heading.name = juce::String(std::string(memoryHeading(kind))) + " (" + juce::String(static_cast<int>(list.names.size())) + ")";
            rows.push_back(heading);
            _items.emplace_back(std::nullopt);
            for (std::size_t index = 0; index < list.names.size(); ++index)
            {
                const MemoryItem item{kind, list.names[index]};
                rows.push_back(ItemRow{iconOf(kind), list.names[index], {}, 1, false, list.current == index, state.selection == item});
                _items.emplace_back(item);
            }
        }
        _list.setRows(std::move(rows));
        _free.setText(freeText(state.free, sizeText), juce::dontSendNotification);
        _free.setTooltip(freeText(state.free, bytesText));
    }

    void MemoryView::resized()
    {
        using namespace tokens;
        auto area = getLocalBounds();
        auto footer = area.removeFromBottom(pixels(kButtonHeight));
        area.removeFromBottom(pixels(kSpaceBetween));
        _refresh.setBounds(footer.removeFromRight(pixels(kSizeLeftMinWidth) / REFRESH_SHARE));
        footer.removeFromRight(pixels(kSpaceBetween));
        _free.setBounds(footer);
        _list.setBounds(area);
    }

    void MemoryView::showMenu(int row)
    {
        const auto& item = _items[static_cast<std::size_t>(row)];
        if (!item)
            return;
        juce::PopupMenu menu;
        menu.addItem(CREATE_PROGRAM_ID, "Create a program" + ELLIPSIS);
        menu.addSeparator();
        menu.addItem(RENAME_ID, "Rename" + ELLIPSIS);
        menu.addItem(DELETE_ID, "Delete" + ELLIPSIS);
        if (item->kind == MemoryKind::Sample)
        {
            menu.addItem(AUDITION_ID, "Audition");
            menu.addItem(STOP_AUDITION_ID, "Stop the audition");
        }
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(),
                           [this, chosen = *item, alive = juce::Component::SafePointer<MemoryView>(this)](int command) {
                               if (alive != nullptr && command != 0)
                                   run(command, chosen);
                           });
    }

    void MemoryView::run(int command, const MemoryItem& item)
    {
        switch (command)
        {
            case CREATE_PROGRAM_ID:
                askNewProgram();
                break;
            case RENAME_ID:
                askTexts("Rename " + juce::String(item.name), "The new name:", {TextField{NAME_LABEL, juce::String(item.name)}},
                         [this, item, alive = juce::Component::SafePointer<MemoryView>(this)](const std::vector<std::string>& texts) {
                             if (alive != nullptr && !texts.front().empty() && texts.front() != item.name)
                                 _memory.rename(item, texts.front());
                         });
                break;
            case DELETE_ID:
                _memory.remove(item);
                break;
            case AUDITION_ID:
                _memory.audition(item);
                break;
            case STOP_AUDITION_ID:
                _memory.stopAudition();
                break;
            default:
                break;
        }
    }

    void MemoryView::askNewProgram()
    {
        askTexts(NEW_PROGRAM_TITLE, {}, {TextField{NAME_LABEL, {}}, TextField{KEYGROUPS_LABEL, ONE_KEYGROUP}},
                 [this, alive = juce::Component::SafePointer<MemoryView>(this)](const std::vector<std::string>& texts) {
                     if (alive == nullptr)
                         return;
                     const juce::String count(texts[KEYGROUPS_FIELD]);
                     const int keygroups = count.getIntValue();
                     if (texts[NAME_FIELD].empty())
                         _model.setProblem(Problem{NEW_PROGRAM_ACTION, NO_NAME});
                     else if (count.isEmpty() || !count.containsOnly(DIGITS) || keygroups < MIN_KEYGROUPS || keygroups > MAX_KEYGROUPS)
                         _model.setProblem(Problem{NEW_PROGRAM_ACTION, BAD_KEYGROUPS});
                     else
                         _memory.createProgram(texts[NAME_FIELD], keygroups);
                 });
    }
}
