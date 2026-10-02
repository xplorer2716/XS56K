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
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"

namespace akm
{
    // Disk discovery of section 10 (spec Tables 20-21, "General Disk Functions"): thin typed wrappers
    // over the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012), like every other primitives
    // file. Each returns at once and reports on the session's thread, like `Session::submit`. The
    // session must outlive every call. [RQ-AKM-060]

    /// Tells the sampler to refresh its list of connected disks (§10/&01). `getDiskCount` and
    /// `getConnectedDisks` are not guaranteed to reflect the sampler's actual disks until this has
    /// completed at least once in the session (spec Table 20, footnote b) — a precondition the spec
    /// states, not one this layer enforces. [RQ-AKM-060]
    void updateDiskList(Session& session, CommandCompletion completion);

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. [RQ-AKM-060]
    struct DiskCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using DiskCountCompletion = std::function<void(const DiskCountResult&)>;

    /// Gets the number of disks connected (§10/&04). [RQ-AKM-060]
    void getDiskCount(Session& session, DiskCountCompletion completion);

    /// One entry of `&05`'s REPLY, in the spec's own field order. [RQ-AKM-060]
    struct DiskInfo
    {
        int handle = 0;
        int type = 0;
        int format = 0;
        int scsiId = 0;
        bool writable = false;
        std::string name;

        friend bool operator==(const DiskInfo&, const DiskInfo&) = default;
    };

    /// `disks` is empty when the command did not complete on a decodable REPLY (not the same as a
    /// decoded REPLY naming zero disks, which is `disks` holding an empty vector); `outcome` is the
    /// result of the command. [RQ-AKM-060]
    struct DiskListResult
    {
        std::optional<std::vector<DiskInfo>> disks{};
        CommandResult outcome{};
    };
    using DiskListCompletion = std::function<void(const DiskListResult&)>;

    /// Gets the list of every connected disk (§10/&05): one `DiskInfo` per disk, in the order the
    /// sampler sent them. Refused as `ChecksumModeUnknown` while the port's checksum mode is unknown:
    /// the REPLY repeats a record of unknown count ending in a variable-length name, so it has no fixed
    /// length either (mirrors `getAllProgramNames`, ADR-AKM-001 DEC-AKM-014). [RQ-AKM-060, RQ-AKM-041]
    void getConnectedDisks(Session& session, DiskListCompletion completion);
}
