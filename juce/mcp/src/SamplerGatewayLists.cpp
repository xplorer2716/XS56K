/*
 * XS56K - Editor for AKAI S5000/S6000 samplers
 * Copyright (C) 2026 xplorer2716
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// The lists unit of the sampler gateway: the song files (section 16), the set lists (section 16) and the scenelists (section 14) -
// listing, selecting, renaming and deleting. It is the only unit that may call their rename and delete primitives (checked by
// `CheckNoDestructiveCalls.cmake`). [TASK-MCP-047, TASK-MCP-048, RQ-MCP-048, RQ-MCP-049, RQ-MCP-057, ADR-MCP-005 (DEC-MCP-031)]
#include <algorithm>
#include <functional>
#include <utility>
#include <variant>

#include "GatewayDetail.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SceneListPrimitives.hpp"
#include "akm/SongPrimitives.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"

namespace mcp
{
    using detail::await;
    using detail::explain;
    using detail::numberText;

    namespace
    {
        constexpr const char* SESSION_TIMED_OUT = "The sampler session did not complete the command in time.";
        constexpr const char* NOUN_SONG_FILE = "song file";
        constexpr const char* NOUN_SET_LIST = "set list";
        constexpr const char* NOUN_SCENELIST = "scenelist";

        /// The AKM primitives of one kind of list, and the tools that name it in the answers. The set lists have no current item, so the
        /// primitives that need one are null. The results of the three kinds are the same types (`SongPrimitives.hpp`).
        struct ListCalls
        {
            const char* noun;
            const char* listTool;
            const char* selectTool;
            void (*count)(akm::Session&, akm::SongCountCompletion);
            void (*nameAt)(akm::Session&, int, akm::SongNameCompletion);
            void (*currentIndex)(akm::Session&, akm::SongIndexCompletion);
            void (*selectByName)(akm::Session&, std::string_view, akm::CommandCompletion);
            void (*selectByIndex)(akm::Session&, int, akm::CommandCompletion);
            void (*renameCurrent)(akm::Session&, std::string_view, akm::CommandCompletion);
            void (*deleteCurrent)(akm::Session&, akm::CommandCompletion);
        };

        const ListCalls& callsOf(NamedListKind kind)
        {
            static const ListCalls SONGS{NOUN_SONG_FILE,
                                         "list_song_files",
                                         "select_song_file",
                                         &akm::getSongCount,
                                         &akm::getSongNameByIndex,
                                         &akm::getCurrentSongIndex,
                                         &akm::selectSongByName,
                                         &akm::selectSongByIndex,
                                         &akm::renameCurrentSong,
                                         &akm::deleteCurrentSong};
            static const ListCalls SET_LISTS{NOUN_SET_LIST, "list_set_lists", "", &akm::getSetListCount, &akm::getSetListNameByIndex,
                                             nullptr,       nullptr,          nullptr, nullptr,         nullptr};
            static const ListCalls SCENELISTS{NOUN_SCENELIST,
                                              "list_scenelists",
                                              "select_scenelist",
                                              &akm::getSceneListCount,
                                              &akm::getSceneListNameByIndex,
                                              &akm::getCurrentSceneListIndex,
                                              &akm::selectSceneListByName,
                                              &akm::selectSceneListByIndex,
                                              &akm::renameCurrentSceneList,
                                              &akm::deleteCurrentSceneList};
            switch (kind)
            {
                case NamedListKind::SongFile:
                    return SONGS;
                case NamedListKind::SetList:
                    return SET_LISTS;
                case NamedListKind::SceneList:
                    break;
            }
            return SCENELISTS;
        }

        /// "0", "0 and 1" or "0, 1 and 2": positions as a sentence.
        std::string positionsText(const std::vector<int>& positions)
        {
            std::string text;
            for (std::size_t i = 0; i < positions.size(); ++i)
            {
                if (i > 0)
                    text += i + 1 == positions.size() ? " and " : ", ";
                text += numberText(positions[i]);
            }
            return text;
        }

        /// The item of the listing whose name is `name`, exactly or else without regard to case, spaces and hyphens; the positions of every
        /// item that matches at the better of the two levels.
        std::vector<int> positionsNamed(const NamedListing& listing, std::string_view name)
        {
            std::vector<int> exact;
            std::vector<int> similar;
            const std::string wanted = normalizeText(name);
            for (const NamedListEntry& entry : listing.entries)
            {
                if (entry.name == name)
                    exact.push_back(entry.index);
                if (normalizeText(entry.name) == wanted)
                    similar.push_back(entry.index);
            }
            return exact.empty() ? similar : exact;
        }

        /// The item other than `except` that bears `name` (without regard to case, spaces and hyphens), if any.
        const NamedListEntry* otherBearing(const NamedListing& listing, int except, std::string_view name)
        {
            const std::string wanted = normalizeText(name);
            for (const NamedListEntry& entry : listing.entries)
            {
                if (entry.index != except && normalizeText(entry.name) == wanted)
                    return &entry;
            }
            return nullptr;
        }

        std::string alreadyHolds(const char* noun, const NamedListEntry& entry)
        {
            return std::string("The sampler already holds a ") + noun + " named \"" + entry.name + "\": nothing was renamed. Choose another name.";
        }
    }

    const char* namedListNoun(NamedListKind kind)
    {
        return callsOf(kind).noun;
    }

    Outcome<int> SamplerGateway::readNamedCount(NamedListKind kind)
    {
        const ListCalls& calls = callsOf(kind);
        const auto count = await<akm::SongCountResult>(waitFor(1), [&](std::function<void(const akm::SongCountResult&)> done) {
            calls.count(session(), std::move(done));
        });
        if (!count)
            return Outcome<int>::failure(SESSION_TIMED_OUT);
        if (!count->count)
            return Outcome<int>::failure(explain(count->outcome, std::string("counting the ") + calls.noun + "s", _config, false));
        return Outcome<int>::success(*count->count);
    }

    Outcome<std::string> SamplerGateway::readNamedName(NamedListKind kind, int index)
    {
        const ListCalls& calls = callsOf(kind);
        const auto name = await<akm::SongNameResult>(waitFor(1), [&](std::function<void(const akm::SongNameResult&)> done) {
            calls.nameAt(session(), index, std::move(done));
        });
        if (!name)
            return Outcome<std::string>::failure(SESSION_TIMED_OUT);
        if (!name->name)
            return Outcome<std::string>::failure(
                explain(name->outcome, std::string("reading the name of the ") + calls.noun + " at position " + numberText(index), _config, false));
        return Outcome<std::string>::success(*name->name);
    }

    Outcome<std::optional<int>> SamplerGateway::readCurrentNamedIndex(NamedListKind kind)
    {
        const ListCalls& calls = callsOf(kind);
        const auto current = await<akm::SongIndexResult>(waitFor(1), [&](std::function<void(const akm::SongIndexResult&)> done) {
            calls.currentIndex(session(), std::move(done));
        });
        if (!current)
            return Outcome<std::optional<int>>::failure(SESSION_TIMED_OUT);
        if (current->index)
            return Outcome<std::optional<int>>::success(current->index);
        // The sampler answers ERROR 04 while none is current (modelled; not yet observed on a real sampler).
        const auto* error = std::get_if<akm::Error>(&current->outcome);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<std::optional<int>>::success(std::nullopt);
        return Outcome<std::optional<int>>::failure(
            explain(current->outcome, std::string("reading which ") + calls.noun + " is current", _config, false));
    }

    Outcome<NamedListEntry> SamplerGateway::currentNamedEntry(NamedListKind kind)
    {
        const auto index = readCurrentNamedIndex(kind);
        if (!index.ok())
            return Outcome<NamedListEntry>::failure(index.problem);
        if (!*index.value)
            return Outcome<NamedListEntry>::failure(std::string("No ") + namedListNoun(kind) + " is selected.");
        const auto name = readNamedName(kind, **index.value);
        if (!name.ok())
            return Outcome<NamedListEntry>::failure(name.problem);
        return Outcome<NamedListEntry>::success(NamedListEntry{**index.value, *name.value});
    }

    Outcome<NamedListing> SamplerGateway::listNamedItems(NamedListKind kind)
    {
        if (const auto problem = connect())
            return Outcome<NamedListing>::failure(*problem);
        const auto count = readNamedCount(kind);
        if (!count.ok())
            return Outcome<NamedListing>::failure(count.problem);
        NamedListing listing;
        for (int index = 0; index < *count.value; ++index)
        {
            const auto name = readNamedName(kind, index);
            if (!name.ok())
                return Outcome<NamedListing>::failure(name.problem);
            listing.entries.push_back(NamedListEntry{index, *name.value});
        }
        if (callsOf(kind).currentIndex != nullptr && !listing.entries.empty())
        {
            const auto current = readCurrentNamedIndex(kind);
            if (!current.ok())
                return Outcome<NamedListing>::failure(current.problem);
            listing.current = *current.value;
        }
        return Outcome<NamedListing>::success(std::move(listing));
    }

    Outcome<NamedListEntry> SamplerGateway::selectNamedItemByName(NamedListKind kind, std::string_view name)
    {
        const ListCalls& calls = callsOf(kind);
        if (calls.selectByName == nullptr)
            return Outcome<NamedListEntry>::failure(std::string("The sampler has no current ") + calls.noun + ": a " + calls.noun + " is not selected.");
        if (const auto problem = connect())
            return Outcome<NamedListEntry>::failure(*problem);
        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            calls.selectByName(session(), name, std::move(done));
        });
        if (!selected)
            return Outcome<NamedListEntry>::failure(SESSION_TIMED_OUT);
        const auto* error = std::get_if<akm::Error>(&*selected);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<NamedListEntry>::failure(std::string("No ") + calls.noun + " is named \"" + std::string(name) + "\". Use " + calls.listTool +
                                                    " to see the names.");
        if (!akm::succeeded(*selected))
            return Outcome<NamedListEntry>::failure(
                explain(*selected, std::string("selecting the ") + calls.noun + " \"" + std::string(name) + "\"", _config, false));
        return currentNamedEntry(kind);
    }

    Outcome<NamedListEntry> SamplerGateway::selectNamedItemByIndex(NamedListKind kind, int index)
    {
        const ListCalls& calls = callsOf(kind);
        if (calls.selectByIndex == nullptr)
            return Outcome<NamedListEntry>::failure(std::string("The sampler has no current ") + calls.noun + ": a " + calls.noun + " is not selected.");
        if (const auto problem = connect())
            return Outcome<NamedListEntry>::failure(*problem);
        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            calls.selectByIndex(session(), index, std::move(done));
        });
        if (!selected)
            return Outcome<NamedListEntry>::failure(SESSION_TIMED_OUT);
        const auto* error = std::get_if<akm::Error>(&*selected);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<NamedListEntry>::failure(std::string("No ") + calls.noun + " is at position " + numberText(index) + ". Use " +
                                                    calls.listTool + " to see the positions.");
        if (!akm::succeeded(*selected))
            return Outcome<NamedListEntry>::failure(
                explain(*selected, std::string("selecting the ") + calls.noun + " at position " + numberText(index), _config, false));
        return currentNamedEntry(kind);
    }

    Outcome<RenamedItem> SamplerGateway::renameCurrentNamedItem(NamedListKind kind, std::string_view newName)
    {
        const ListCalls& calls = callsOf(kind);
        if (calls.renameCurrent == nullptr)
            return Outcome<RenamedItem>::failure(std::string("The sampler has no current ") + calls.noun + ": a " + calls.noun + " is renamed by its name.");
        if (const auto problem = connect())
            return Outcome<RenamedItem>::failure(*problem);
        const auto current = currentNamedEntry(kind);
        if (!current.ok())
            return Outcome<RenamedItem>::failure(std::string("No ") + calls.noun + " is selected: use " + calls.selectTool + " first.");
        const auto listing = listNamedItems(kind);
        if (!listing.ok())
            return Outcome<RenamedItem>::failure(listing.problem);
        if (const NamedListEntry* bearer = otherBearing(*listing.value, current.value->index, newName))
            return Outcome<RenamedItem>::failure(alreadyHolds(calls.noun, *bearer));

        const auto renamed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            calls.renameCurrent(session(), newName, std::move(done));
        });
        if (!renamed)
            return Outcome<RenamedItem>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*renamed))
            return Outcome<RenamedItem>::failure(
                explain(*renamed, std::string("renaming the ") + calls.noun + " \"" + current.value->name + "\"", _config, false));
        const auto after = readNamedName(kind, current.value->index);
        if (!after.ok())
            return Outcome<RenamedItem>::failure(after.problem);
        return Outcome<RenamedItem>::success(RenamedItem{current.value->name, *after.value});
    }

    Outcome<SetListRenaming> SamplerGateway::renameSetList(std::string_view name, std::string_view newName)
    {
        const char* noun = namedListNoun(NamedListKind::SetList);
        const auto listing = listNamedItems(NamedListKind::SetList);
        if (!listing.ok())
            return Outcome<SetListRenaming>::failure(listing.problem);
        const std::vector<int> found = positionsNamed(*listing.value, name);
        if (found.empty())
            return Outcome<SetListRenaming>::failure(std::string("No ") + noun + " is named \"" + std::string(name) + "\". Use " +
                                                     callsOf(NamedListKind::SetList).listTool + " to see the names.");
        if (found.size() > 1)
            return Outcome<SetListRenaming>::failure(std::string("Several ") + noun + "s are named \"" + std::string(name) + "\" (positions " +
                                                     positionsText(found) + "): nothing was renamed, because a " + noun + " is found by its name.");
        const NamedListEntry& target = listing.value->entries[static_cast<std::size_t>(found.front())];
        if (const NamedListEntry* bearer = otherBearing(*listing.value, target.index, newName))
            return Outcome<SetListRenaming>::failure(alreadyHolds(noun, *bearer));

        const auto renamed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::renameSetList(session(), target.index, newName, std::move(done));
        });
        if (!renamed)
            return Outcome<SetListRenaming>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*renamed))
            return Outcome<SetListRenaming>::failure(explain(*renamed, std::string("renaming the ") + noun + " \"" + target.name + "\"", _config, false));
        const auto after = readNamedName(NamedListKind::SetList, target.index);
        if (!after.ok())
            return Outcome<SetListRenaming>::failure(after.problem);
        return Outcome<SetListRenaming>::success(SetListRenaming{target.index, target.name, *after.value});
    }

    Outcome<NamedDeletion> SamplerGateway::deleteCurrentNamedItem(NamedListKind kind, std::string_view confirm)
    {
        const ListCalls& calls = callsOf(kind);
        if (calls.deleteCurrent == nullptr)
            return Outcome<NamedDeletion>::failure(std::string("The sampler has no current ") + calls.noun + ": a " + calls.noun + " is deleted by its name.");
        if (const auto problem = connect())
            return Outcome<NamedDeletion>::failure(*problem);
        const auto current = currentNamedEntry(kind);
        if (!current.ok())
            return Outcome<NamedDeletion>::failure(std::string("No ") + calls.noun + " is selected: use " + calls.selectTool + " first.");
        if (current.value->name != confirm)
            return Outcome<NamedDeletion>::success(NamedDeletion{false, current.value->index, current.value->name, {}});

        const auto deleted = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            calls.deleteCurrent(session(), std::move(done));
        });
        if (!deleted)
            return Outcome<NamedDeletion>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*deleted))
            return Outcome<NamedDeletion>::failure(
                explain(*deleted, std::string("deleting the ") + calls.noun + " \"" + current.value->name + "\"", _config, false));
        const auto after = listNamedItems(kind);
        if (!after.ok())
            return Outcome<NamedDeletion>::failure(after.problem);
        return Outcome<NamedDeletion>::success(NamedDeletion{true, current.value->index, current.value->name, after.value->entries});
    }

    Outcome<NamedDeletion> SamplerGateway::deleteSetList(std::string_view name, std::string_view confirm)
    {
        const char* noun = namedListNoun(NamedListKind::SetList);
        const auto listing = listNamedItems(NamedListKind::SetList);
        if (!listing.ok())
            return Outcome<NamedDeletion>::failure(listing.problem);
        const std::vector<int> found = positionsNamed(*listing.value, name);
        if (found.empty())
            return Outcome<NamedDeletion>::failure(std::string("No ") + noun + " is named \"" + std::string(name) + "\". Use " +
                                                   callsOf(NamedListKind::SetList).listTool + " to see the names.");
        if (found.size() > 1)
            return Outcome<NamedDeletion>::failure(std::string("Several ") + noun + "s are named \"" + std::string(name) + "\" (positions " +
                                                   positionsText(found) + "): nothing was deleted, because a " + noun + " is found by its name.");
        const NamedListEntry& target = listing.value->entries[static_cast<std::size_t>(found.front())];
        if (target.name != confirm)
            return Outcome<NamedDeletion>::success(NamedDeletion{false, target.index, target.name, {}});

        const auto deleted = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::deleteSetList(session(), target.index, std::move(done));
        });
        if (!deleted)
            return Outcome<NamedDeletion>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*deleted))
            return Outcome<NamedDeletion>::failure(explain(*deleted, std::string("deleting the ") + noun + " \"" + target.name + "\"", _config, false));
        const auto after = listNamedItems(NamedListKind::SetList);
        if (!after.ok())
            return Outcome<NamedDeletion>::failure(after.problem);
        return Outcome<NamedDeletion>::success(NamedDeletion{true, target.index, target.name, after.value->entries});
    }
}
