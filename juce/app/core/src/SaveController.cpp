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
#include "app/SaveController.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "app/FileBrowser.hpp"

namespace app
{
    // [RQ-APP-043 to RQ-APP-047, ADR-APP-005 (DEC-APP-038 to DEC-APP-040), TASK-APP-056 to TASK-APP-059]
    namespace
    {
        constexpr const char* CHECK_NAMES_KEY = "save.checkNames";
        constexpr const char* ON = "true";
        constexpr const char* OFF = "false";
        constexpr const char* SAVING_LABEL = "Saving ";
        constexpr const char* SAVING_ALL_LABEL = "Saving all the ";
        constexpr const char* SAVING_MEMORY_LABEL = "Saving the whole memory";
        constexpr const char* SAVE_ACTION = "Saving";
        constexpr const char* NOT_CONNECTED = "The sampler is not connected.";
        constexpr const char* NO_DISK = "Select a disk in the browser first: the save goes into its current folder.";
        constexpr const char* NOT_MSDOS = "The sampler saves only to an MS-DOS formatted disk (MSDOS or FAT32); the disk \"";
        constexpr const char* IS_FORMATTED = "\" is formatted ";
        constexpr const char* READ_ONLY = "The disk \"";
        constexpr const char* IS_READ_ONLY = "\" cannot be written.";
        constexpr const char* QUESTION_ONE = "Replace the file?";
        constexpr const char* QUESTION_SOME = "Replace the files?";
        constexpr const char* TEXT_ONE = "The folder already holds the file it would write. Replace it, or skip it and keep the file on the disk?";
        constexpr const char* TEXT_SOME = "The folder already holds files this save would write, and it may write more (children) that the "
                                          "application cannot know before sending. Replace existing files, or skip them and keep them on the disk?";
        constexpr int FORMAT_MSDOS = 1;
        constexpr int FORMAT_FAT32 = 2;

        /// The kinds the whole memory is saved by, in order (DEC-APP-040).
        constexpr std::array<MemoryKind, 6> WHOLE_MEMORY_ORDER{MemoryKind::Program,  MemoryKind::Sample,  MemoryKind::Multi,
                                                               MemoryKind::SongFile, MemoryKind::SetList, MemoryKind::SceneList};

        mcp::SaveKind saveKindOf(MemoryKind kind)
        {
            using enum app::MemoryKind;
            switch (kind)
            {
                case Multi:
                    return mcp::SaveKind::Multi;
                case Program:
                    return mcp::SaveKind::Program;
                case Sample:
                    return mcp::SaveKind::Sample;
                case SongFile:
                    return mcp::SaveKind::SongFile;
                case SetList:
                    return mcp::SaveKind::SetList;
                case SceneList:
                    break;
            }
            return mcp::SaveKind::SceneList;
        }

        bool withChildren(SaveScope scope)
        {
            return scope == SaveScope::WithChildren || scope == SaveScope::AllOfKindWithChildren;
        }

        /// One gateway save of `items` and what it gave.
        struct SavePart
        {
            std::vector<MemoryItem> items;
            mcp::SaveOutcome outcome;
        };

        /// What the job did: the saves that went through, in order, and the failure that stopped it, if any.
        struct SaveDone
        {
            std::vector<SavePart> parts;
            std::string failure;
        };

        std::vector<MemoryItem> ofKind(const std::vector<MemoryItem>& items, MemoryKind kind)
        {
            std::vector<MemoryItem> found;
            std::ranges::copy_if(items, std::back_inserter(found), [kind](const MemoryItem& item) { return item.kind == kind; });
            return found;
        }

        SaveDone saveOnSampler(mcp::SamplerGateway& gateway, std::optional<int> disk, const std::vector<std::string>& folder,
                               const SaveRequest& request, const std::vector<MemoryItem>& items, mcp::ExistingFile existing)
        {
            SaveDone done;
            if (const auto there = moveToFolder(gateway, disk, folder); !there.ok())
            {
                done.failure = there.problem;
                return done;
            }
            const bool children = withChildren(request.scope);
            if (request.scope == SaveScope::Alone || request.scope == SaveScope::WithChildren)
            {
                const auto saved = gateway.saveMemoryItem(saveKindOf(request.item.kind), request.item.name, existing, children);
                if (!saved.ok())
                    done.failure = saved.problem;
                else
                    done.parts.push_back(SavePart{{request.item}, *saved.value});
                return done;
            }
            // All of a kind, or each kind of the whole memory in turn; the first failure stops the job. [RQ-APP-044, DEC-APP-040]
            for (const MemoryKind kind : WHOLE_MEMORY_ORDER)
            {
                const std::vector<MemoryItem> these = ofKind(items, kind);
                if (these.empty())
                    continue;
                const auto saved = gateway.saveAllMemoryItems(saveKindOf(kind), static_cast<int>(these.size()), existing, children);
                if (!saved.ok())
                {
                    done.failure = saved.problem;
                    return done;
                }
                done.parts.push_back(SavePart{these, *saved.value});
            }
            return done;
        }

        void append(std::vector<std::string>& to, const std::vector<std::string>& from)
        {
            to.insert(to.end(), from.begin(), from.end());
        }
    }

    SaveController::SaveController(Model& model, Controller& controller, AppSettings& settings)
        : _model(model), _controller(controller), _settings(settings)
    {
    }

    std::optional<MemoryItem> SaveController::proposedItem() const
    {
        if (_model.memory().selection)
            return _model.memory().selection;
        if (const auto current = _model.memory().current(MemoryKind::Program))
            return MemoryItem{MemoryKind::Program, *current};
        return std::nullopt;
    }

    std::optional<std::string> SaveController::refusal() const
    {
        if (_model.connection().state != ConnectionState::Connected)
            return NOT_CONNECTED;
        const mcp::DiskEntry* disk = _model.browser().currentDiskEntry();
        if (disk == nullptr)
            return NO_DISK;
        if (disk->format != FORMAT_MSDOS && disk->format != FORMAT_FAT32)
            return NOT_MSDOS + disk->name + IS_FORMATTED + std::string(diskFormatName(disk->format)) + ".";
        if (!disk->writable)
            return READ_ONLY + disk->name + IS_READ_ONLY;
        return std::nullopt;
    }

    bool SaveController::checkNames() const
    {
        return _settings.read(CHECK_NAMES_KEY).value_or(ON) != OFF;
    }

    void SaveController::setCheckNames(bool on)
    {
        _settings.write(CHECK_NAMES_KEY, on ? ON : OFF);
    }

    std::vector<MemoryItem> SaveController::itemsOf(const SaveRequest& request) const
    {
        if (request.scope == SaveScope::Alone || request.scope == SaveScope::WithChildren)
            return {request.item};
        std::vector<MemoryItem> items;
        for (const MemoryKind kind : WHOLE_MEMORY_ORDER)
        {
            if (request.scope != SaveScope::WholeMemory && kind != request.item.kind)
                continue;
            for (const std::string& name : _model.memory().list(kind).names)
                items.push_back(MemoryItem{kind, name});
        }
        return items;
    }

    std::vector<std::string> SaveController::filesThere(const std::vector<MemoryItem>& items) const
    {
        std::vector<std::string> there;
        const FolderListing* listing = _model.browser().listing(_model.browser().currentFolder);
        if (listing == nullptr)
            return there;
        for (const mcp::DiskFileEntry& file : listing->files)
        {
            if (std::ranges::any_of(items, [&file](const MemoryItem& item) { return isFileOf(item, file.name); }))
                there.push_back(file.name);
        }
        return there;
    }

    void SaveController::save(const SaveRequest& request)
    {
        if (const auto refused = refusal())
        {
            _model.setProblem(Problem{SAVE_ACTION, *refused});
            return;
        }
        std::vector<MemoryItem> items = itemsOf(request);
        if (!checkNames())
        {
            run(request, std::move(items), mcp::ExistingFile::Overwrite);
            return;
        }
        std::vector<std::string> there = filesThere(items);
        if (there.empty())
        {
            // No file known to be there: the skip flag keeps any child the save would meet on the disk. [RQ-APP-045]
            run(request, std::move(items), mcp::ExistingFile::Skip);
            return;
        }
        const bool perFile = request.scope == SaveScope::Alone;
        const ExistingQuestion question{perFile ? QUESTION_ONE : QUESTION_SOME, perFile ? TEXT_ONE : TEXT_SOME, std::move(there)};
        _controller.askExisting(question, [this, alive = std::weak_ptr<int>(_alive), request, items = std::move(items)](ExistingAnswer answer) {
            if (alive.expired() || answer == ExistingAnswer::Cancel)
                return;
            run(request, items, answer == ExistingAnswer::Replace ? mcp::ExistingFile::Overwrite : mcp::ExistingFile::Skip);
        });
    }

    void SaveController::run(const SaveRequest& request, std::vector<MemoryItem> items, mcp::ExistingFile existing)
    {
        std::string label = SAVING_MEMORY_LABEL;
        if (request.scope == SaveScope::Alone || request.scope == SaveScope::WithChildren)
            label = SAVING_LABEL + request.item.name;
        else if (request.scope != SaveScope::WholeMemory)
            label = SAVING_ALL_LABEL + std::string(memoryHeading(request.item.kind));
        const BrowserState& browser = _model.browser();
        _controller.runOnSampler<SaveDone>(
            label,
            [disk = browser.currentDisk, folder = browser.currentFolder, request, items = std::move(items), existing](mcp::SamplerGateway& gateway) {
                return mcp::Outcome<SaveDone>::success(saveOnSampler(gateway, disk, folder, request, items, existing));
            },
            [this, alive = std::weak_ptr<int>(_alive), whole = request.scope == SaveScope::WholeMemory, label](const SaveDone& done) {
                if (alive.expired())
                    return;
                if (!done.parts.empty())
                {
                    // The folder as listed after the last save. [RQ-APP-046]
                    const mcp::SaveOutcome& last = done.parts.back().outcome;
                    BrowserState state = _model.browser();
                    state.currentFolder = splitPath(last.path);
                    state.listings[pathText(state.currentFolder)].files = last.filesAfter;
                    _model.setBrowser(std::move(state));
                }
                if (!done.parts.empty() || whole)
                {
                    SaveReport report;
                    for (const SavePart& part : done.parts)
                    {
                        const SaveReport one = reportOf(part.items, part.outcome);
                        report.diskName = one.diskName;
                        report.path = one.path;
                        append(report.saved, one.saved);
                        append(report.skipped, one.skipped);
                        append(report.missing, one.missing);
                        report.kindsSaved.push_back(part.items.front().kind);
                    }
                    _model.setSaved(std::move(report));
                }
                if (!done.failure.empty())
                    _model.setProblem(Problem{label, done.failure});
            });
    }
}
