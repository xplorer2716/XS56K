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

// Smoke test of the test scaffolding itself: proves that the AKM library builds, links
// and runs under Catch2 through ctest. [TASK-AKM-003, RQ-AKM-016, RQ-AKM-019,
// ADR-AKM-001 (DEC-AKM-001, DEC-AKM-008)]
#include <catch2/catch_test_macros.hpp>

#include "akm/LibraryInfo.hpp"

namespace
{
    constexpr const char* expectedLibraryName = "xs56k_akm";
}

TEST_CASE("Given the AKM library, When its name is requested, Then it is xs56k_akm [RQ-AKM-019]",
          "[akm][smoke]")
{
    CHECK(akm::libraryName() == expectedLibraryName);
}
