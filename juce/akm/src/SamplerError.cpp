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
#include "akm/SamplerError.hpp"

#include <array>

namespace akm
{
    namespace
    {
        struct Entry
        {
            std::uint16_t number;
            std::string_view meaning;
        };

        constexpr std::string_view UNKNOWN_MEANING = "unknown error number";

        // Spec Table 3, in the order of the table.
        constexpr std::array<Entry, 27> TABLE{{
            {error_number::NOT_SUPPORTED, "not supported: the requested command is not supported"},
            {error_number::INVALID_FORMAT, "invalid message format: insufficient data supplied"},
            {error_number::OUT_OF_RANGE, "parameter out of range"},
            {error_number::UNKNOWN_ERROR, "unknown error: the command could not be completed"},
            {error_number::NOT_FOUND, "requested program, multi, sample or other item could not be found"},
            {error_number::COULD_NOT_CREATE, "new element could not be created"},
            {error_number::DELETION_FAILED, "deletion of the requested item could not be completed"},
            {error_number::CHECKSUM_INVALID, "checksum invalid"},
            {error_number::DISK_SELECTED_DISK_INVALID, "disk error: selected disk is invalid"},
            {error_number::DISK_ERROR_DURING_LOAD, "disk error: error during load"},
            {error_number::DISK_ITEM_NOT_FOUND, "disk error: item not found"},
            {error_number::DISK_UNABLE_TO_CREATE, "disk error: unable to create"},
            {error_number::DISK_FOLDER_NOT_EMPTY, "disk error: folder not empty"},
            {error_number::DISK_UNABLE_TO_DELETE, "disk error: unable to delete"},
            {error_number::DISK_UNKNOWN_ERROR, "disk error: unknown error"},
            {error_number::DISK_ERROR_DURING_SAVE, "disk error: error during save"},
            {error_number::DISK_INSUFFICIENT_SPACE, "disk error: insufficient disk space"},
            {error_number::DISK_WRITE_PROTECTED, "disk error: media is write-protected"},
            {error_number::DISK_NAME_NOT_UNIQUE, "disk error: cannot save because the name is not unique"},
            {error_number::DISK_INVALID_HANDLE, "disk error: invalid disk handle"},
            {error_number::DISK_EMPTY, "disk error: disk is empty"},
            {error_number::DISK_OPERATION_ABORTED, "disk error: disk operation was aborted"},
            {error_number::DISK_FAILED_ON_OPEN, "disk error: failed on open"},
            {error_number::DISK_READ_ERROR, "disk error: read error"},
            {error_number::DISK_NOT_READY, "disk error: disk not ready"},
            {error_number::DISK_SCSI_ERROR, "disk error: SCSI error"},
            {error_number::KEYGROUP_NOT_IN_PROGRAM, "requested keygroup does not exist in the current program"},
        }};
    }

    ErrorInfo describeError(std::uint16_t number)
    {
        for (const Entry& entry : TABLE)
        {
            if (entry.number == number)
                return ErrorInfo{number, true, entry.meaning};
        }
        return ErrorInfo{number, false, UNKNOWN_MEANING};
    }
}
