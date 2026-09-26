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

// Never called: it exists so that the build proves that a scenario written against `MidiBackend&` (here
// the Echo scenario, the plan's first hardware test) compiles against JuceMidiBackend, the backend that
// talks to the real S5000. [TASK-AKM-007, RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]
#include "akm/harness/EchoScenario.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "common/midi/JuceMidiBackend.hpp"

namespace akm::harness
{
    EchoResult compileCheckEchoScenarioOnJuceBackend(const ScenarioTarget& target)
    {
        common::midi::JuceMidiBackend backend;
        RealScenarioDriver driver;
        return runEchoScenario(backend, driver, target, {0x01, 0x02, 0x03, 0x04});
    }
}
