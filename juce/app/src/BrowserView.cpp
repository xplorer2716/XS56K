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
#include "BrowserView.hpp"

#include <algorithm>
#include <utility>

#include "LookAndFeel.hpp"
#include "TextPrompt.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-024 to RQ-APP-033, RQ-APP-005, ADR-APP-001 (DEC-APP-001), ADR-APP-002 (DEC-APP-013, DEC-APP-014), TASK-APP-018]
    namespace
    {
        constexpr const char* PATH_SEPARATOR = ":\\";
        constexpr const char* DETAIL_SEPARATOR = " - ";
        constexpr const char* FREE_PREFIX = "free ";
        constexpr const char* NO_DISK = "No disk is current.";
        constexpr const char* NAME_LABEL = "Name";
        constexpr int DETAIL_LINES = 2;  // the selected file's name, kind and size may wrap once

        /// The menu's item ids: one per command of the core, and the extra ones of the view.
        enum MenuId
        {
            LOAD_ID = 1,
            LOAD_RAM_ID,
            LOAD_VIRTUAL_ID,
            LOAD_WITH_DEPENDENTS_ID,
            LOAD_FOLDER_ID,
            CREATE_FOLDER_ID,
            RENAME_ID,
            DELETE_ID,
            AUDITION_ID,
            STOP_AUDITION_ID,
        };

        RowIcon iconOf(FileKind kind)
        {
            switch (kind)
            {
                case FileKind::Program:
                    return RowIcon::Program;
                case FileKind::Sample:
                    return RowIcon::Sample;
                case FileKind::Multi:
                    return RowIcon::Multi;
                case FileKind::SongFile:
                    return RowIcon::SongFile;
                case FileKind::SetList:
                    return RowIcon::SetList;
                case FileKind::SceneList:
                    return RowIcon::SceneList;
                case FileKind::Unknown:
                    break;
            }
            return RowIcon::Unknown;
        }

        bool sameItem(const std::optional<BrowserItem>& selection, const BrowserItem& item)
        {
            return selection && selection->kind == item.kind && selection->folder == item.folder && selection->name == item.name;
        }

        std::vector<std::string> pathOf(const BrowserItem& folder)
        {
            std::vector<std::string> path = folder.folder;
            path.push_back(folder.name);
            return path;
        }

        /// The name a file is renamed from: without its extension, which the sampler keeps. [RQ-APP-031]
        juce::String withoutExtension(const BrowserItem& item)
        {
            const juce::String name(item.name);
            if (item.kind != BrowserItemKind::File || !name.containsChar('.'))
                return name;
            return name.upToLastOccurrenceOf(".", false, false);
        }
    }

    BrowserView::BrowserView(LookAndFeel& lookAndFeel, Model& model, FileBrowser& browser)
        : _lookAndFeel(lookAndFeel), _model(model), _browser(browser), _list(lookAndFeel)
    {
        _list.onClick = [this](int row) { clicked(row); };
        _list.onDoubleClick = [this](int row) { doubleClicked(row); };
        _list.onMenu = [this](int row) { showMenu(row); };
        _list.onSpace = [this](int row) { _browser.toggleAudition(_entries[static_cast<std::size_t>(row)].item); };  // [RQ-APP-083]
        addAndMakeVisible(_list);
        for (CutLabel* label : {&_path, &_details})
        {
            label->setFont(_lookAndFeel.font(tokens::kFontCaptionSize));
            label->setColour(juce::Label::textColourId, colourOf(tokens::kColourTextSecondary));
            label->setBorderSize({});
            addAndMakeVisible(*label);
        }
        _details.setJustificationType(juce::Justification::centredLeft);
        _model.addListener(*this);
        rebuild();
    }

    BrowserView::~BrowserView()
    {
        _model.removeListener(*this);
    }

    void BrowserView::modelChanged(ModelPart part)
    {
        if (part == ModelPart::Browser)
            rebuild();
    }

    void BrowserView::rebuild()
    {
        const BrowserState& state = _model.browser();
        std::vector<ItemRow> rows;
        _entries.clear();
        for (const mcp::DiskEntry& disk : state.disks)
        {
            const BrowserItem item{BrowserItemKind::Disk, {}, disk.name};
            const bool current = state.currentDisk == disk.handle;
            rows.push_back(ItemRow{RowIcon::Disk, disk.name, juce::String(std::string(diskFormatName(disk.format))), 0, false, current,
                                   sameItem(state.selection, item)});
            _entries.push_back(Entry{item, disk.handle});
            if (current)
                addFolder(state, {}, 1, rows);
        }
        _list.setRows(std::move(rows));

        const mcp::DiskEntry* disk = state.currentDiskEntry();
        const juce::String path =
            disk == nullptr ? juce::String(NO_DISK) : juce::String(disk->name) + PATH_SEPARATOR + juce::String(pathText(state.currentFolder));
        _path.setText(path, juce::dontSendNotification);
        // The sizes in the unit that reads best, the exact count of bytes in the tooltip. [RQ-APP-081, RQ-APP-026, RQ-APP-024, TASK-APP-020]
        juce::String details;
        juce::String exact;
        if (const auto file = state.selectedFile())
        {
            details = juce::String(file->name) + DETAIL_SEPARATOR + juce::String(std::string(fileKindName(file->kind))) + DETAIL_SEPARATOR +
                      juce::String(sizeText(file->sizeBytes));
            exact = juce::String(file->name) + DETAIL_SEPARATOR + juce::String(bytesText(file->sizeBytes));
        }
        else if (disk != nullptr)
        {
            const juce::String head = juce::String(disk->name) + DETAIL_SEPARATOR + juce::String(std::string(diskTypeName(disk->type))) +
                                      DETAIL_SEPARATOR + juce::String(std::string(diskFormatName(disk->format))) + DETAIL_SEPARATOR + FREE_PREFIX;
            details = head + juce::String(freeSpaceText(state.freeBytes.value_or(0)));
            // No exact count to add when the disk reports none: the tooltip is then the whole text, when it is cut. [RQ-APP-080]
            exact = state.freeBytes.value_or(0) == 0 ? juce::String() : head + juce::String(bytesText(*state.freeBytes));
        }
        _details.setText(details, juce::dontSendNotification);
        _details.setTooltip(exact);
    }

    void BrowserView::addFolder(const BrowserState& state, const std::vector<std::string>& folder, int depth, std::vector<ItemRow>& rows)
    {
        const FolderListing* listing = state.listing(folder);
        if (listing == nullptr)
        {
            // Not listed yet, on the way to the sampler's current folder (the session opened in a sub-folder): the path's next folder is
            // shown, so that the current folder is in the tree. [RQ-APP-025]
            const auto& current = state.currentFolder;
            if (folder.size() < current.size() && std::equal(folder.begin(), folder.end(), current.begin()))
            {
                const BrowserItem item{BrowserItemKind::Folder, folder, current[folder.size()]};
                const std::vector<std::string> path = pathOf(item);
                rows.push_back(ItemRow{RowIcon::Folder, item.name, {}, depth, false, path == current, sameItem(state.selection, item)});
                _entries.push_back(Entry{item, std::nullopt});
                if (!state.isFolded(path))
                    addFolder(state, path, depth + 1, rows);
            }
            return;
        }
        for (const std::string& name : listing->folders)
        {
            const BrowserItem item{BrowserItemKind::Folder, folder, name};
            const std::vector<std::string> path = pathOf(item);
            // The sampler's current folder: the dot, and "(current)" in the tooltip. [RQ-APP-025, RQ-APP-005]
            rows.push_back(ItemRow{RowIcon::Folder, name, {}, depth, false, path == state.currentFolder, sameItem(state.selection, item)});
            _entries.push_back(Entry{item, std::nullopt});
            if (!state.isFolded(path))
                addFolder(state, path, depth + 1, rows);
        }
        for (const mcp::DiskFileEntry& file : listing->files)
        {
            const BrowserItem item{BrowserItemKind::File, folder, file.name};
            rows.push_back(ItemRow{iconOf(fileKindOf(file.name)), file.name, {}, depth, false, false, sameItem(state.selection, item)});
            _entries.push_back(Entry{item, std::nullopt});
        }
    }

    void BrowserView::resized()
    {
        using namespace tokens;
        auto area = getLocalBounds();
        _path.setBounds(area.removeFromTop(pixels(kRowHeight)));
        _details.setBounds(area.removeFromBottom(pixels(kRowHeight) * DETAIL_LINES));
        _list.setBounds(area);
    }

    void BrowserView::clicked(int row)
    {
        const Entry& entry = _entries[static_cast<std::size_t>(row)];
        const BrowserState& state = _model.browser();
        if (entry.diskHandle && state.currentDisk != entry.diskHandle)
            _browser.selectDisk(*entry.diskHandle);
        else
            _browser.select(entry.item);
    }

    void BrowserView::doubleClicked(int row)
    {
        const Entry& entry = _entries[static_cast<std::size_t>(row)];
        const BrowserState& state = _model.browser();
        if (entry.item.kind == BrowserItemKind::Folder)
        {
            // A folder that shows what it holds is folded, nothing sent; any other one is opened on the sampler. [RQ-APP-079, TASK-APP-021]
            const std::vector<std::string> path = pathOf(entry.item);
            if (state.listing(path) != nullptr && !state.isFolded(path))
                _browser.fold(path);
            else
                _browser.openFolder(path);
        }
        else if (entry.diskHandle && state.currentDisk == entry.diskHandle)
            _browser.openFolder({});
    }

    void BrowserView::showMenu(int row)
    {
        const Entry entry = _entries[static_cast<std::size_t>(row)];
        const bool sample = entry.item.kind == BrowserItemKind::File && fileKindOf(entry.item.name) == FileKind::Sample;
        juce::PopupMenu menu;
        for (const BrowserCommand command : commandsFor(entry.item))
        {
            switch (command)
            {
                case BrowserCommand::Load:
                    menu.addItem(LOAD_ID, "Load");
                    if (sample)
                    {
                        menu.addItem(LOAD_RAM_ID, "Load as RAM");
                        menu.addItem(LOAD_VIRTUAL_ID, "Load as virtual");
                    }
                    break;
                case BrowserCommand::LoadWithDependents:
                    menu.addItem(LOAD_WITH_DEPENDENTS_ID, "Load with dependents");
                    break;
                case BrowserCommand::LoadFolder:
                    menu.addItem(LOAD_FOLDER_ID, "Load the folder" + ELLIPSIS);
                    break;
                case BrowserCommand::CreateFolder:
                    menu.addItem(CREATE_FOLDER_ID, "Create a folder" + ELLIPSIS);
                    break;
                case BrowserCommand::Rename:
                    menu.addItem(RENAME_ID, "Rename" + ELLIPSIS);
                    break;
                case BrowserCommand::Delete:
                    menu.addItem(DELETE_ID, "Delete" + ELLIPSIS);
                    break;
                case BrowserCommand::Audition:
                    menu.addItem(AUDITION_ID, "Audition");
                    menu.addItem(STOP_AUDITION_ID, "Stop the audition");
                    break;
            }
        }
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(),
                           [this, entry, alive = juce::Component::SafePointer<BrowserView>(this)](int command) {
                               if (alive != nullptr && command != 0)
                                   run(command, entry);
                           });
    }

    void BrowserView::run(int command, const Entry& entry)
    {
        const BrowserItem& item = entry.item;
        switch (command)
        {
            case LOAD_ID:
                _browser.load(item, false, mcp::SampleLoadMode::Normal);
                break;
            case LOAD_RAM_ID:
                _browser.load(item, false, mcp::SampleLoadMode::Ram);
                break;
            case LOAD_VIRTUAL_ID:
                _browser.load(item, false, mcp::SampleLoadMode::Virtual);
                break;
            case LOAD_WITH_DEPENDENTS_ID:
                _browser.load(item, true, mcp::SampleLoadMode::Normal);
                break;
            case LOAD_FOLDER_ID:
                _browser.loadFolder(item);
                break;
            case CREATE_FOLDER_ID:
            {
                const std::vector<std::string> parent = item.kind == BrowserItemKind::Folder ? pathOf(item) : std::vector<std::string>{};
                askTexts("Create a folder", "In " + juce::String(parent.empty() ? std::string("the root") : pathText(parent)) + ":",
                         {TextField{NAME_LABEL, {}}},
                         [this, parent, alive = juce::Component::SafePointer<BrowserView>(this)](const std::vector<std::string>& texts) {
                             if (alive != nullptr && !texts.front().empty())
                                 _browser.createFolder(parent, texts.front());
                         });
                break;
            }
            case RENAME_ID:
                askTexts("Rename " + juce::String(item.name),
                         item.kind == BrowserItemKind::File ? "The new name, without the extension, which the sampler keeps:" : "The new name:",
                         {TextField{NAME_LABEL, withoutExtension(item)}},
                         [this, item, alive = juce::Component::SafePointer<BrowserView>(this)](const std::vector<std::string>& texts) {
                             if (alive != nullptr && !texts.front().empty())
                                 _browser.rename(item, texts.front());
                         });
                break;
            case DELETE_ID:
                _browser.remove(item);
                break;
            case AUDITION_ID:
                _browser.audition(item);
                break;
            case STOP_AUDITION_ID:
                _browser.stopAudition();
                break;
            default:
                break;
        }
    }
}
