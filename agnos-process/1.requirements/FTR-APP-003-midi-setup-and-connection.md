# FTR-APP-003: MIDI Setup of the Application and Connection to the Sampler

## Overview

Session APP (2026-10-13). The owner requires a menu for the MIDI setup of the application: the MIDI ports of the computer the application
runs on, through which it talks to the sampler. This feature covers that setup, the settings kept between two runs, opening and closing the
session with the sampler, and the connection state always shown.

The AKM layer already opens a session (discovery, then the DeviceID, then the section 00 settings, RQ-AKM-039 to RQ-AKM-041) and closes
it (RQ-AKM-042); the MIDI layer lists the ports and opens them by name (RQ-MID-001 to RQ-MID-003). The sampler's DeviceID is 0 to 31, set on
the sampler, and cannot be read or changed by SysEx (`juce/akm/include/akm/SessionConfig.hpp`).

**Priorities.** Priority 1 (mandatory) is written `Must`, priority 2 (nice to have) `Could` (owner, 2026-10-13).

**Out of scope here.** The sampler's own MIDI setup (§04, its MIDI channels and filters) is a priority-2 feature (FTR-APP-009).

**Depends on** FTR-APP-001, FTR-APP-002, FTR-AKM-001, RQ-MID-001 to RQ-MID-003, RQ-MID-023.

**Sources.** `juce/akm/include/akm/SessionConfig.hpp`, `process/1.requirements/FTR-AKM-001-transport-et-config-sysex.md`,
`process/1.requirements/RQ-MID-midi-layer.md`, `process/1.requirements/FTR-AKM-006-system-setup.md` (model, name), RQ-AKM-044 (OS version).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person editing the sampler; every feature that sends a command to the sampler.

---

## Functional Requirements

### RQ-APP-015: The MIDI setup dialog
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person chooses "MIDI setup" in the menu bar, the application SHALL show the MIDI input ports and the MIDI output ports of the computer by their names (RQ-MID-001), the ones in use marked, and SHALL let the person choose one input, one output and the sampler's DeviceID (0 to 31), refusing any other DeviceID.
- **Rationale**: the owner requires it; the computer may have several MIDI interfaces.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a computer with MIDI inputs "IN A", "IN B" and outputs "OUT A", "OUT B", *When* the person opens the MIDI setup, *Then* both lists are shown; *When* the person types the DeviceID 40, *Then* it is refused and the range 0 to 31 is shown.
- **Dependencies**: RQ-APP-001, RQ-APP-007, RQ-MID-001, RQ-AKM-039

### RQ-APP-016: The MIDI settings are kept between runs
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person validates the MIDI setup, the application SHALL store the chosen input port, output port and DeviceID in the person's settings, and SHALL show them again in the MIDI setup at the next run.
- **Rationale**: the setup is done once per computer, not at every start.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* "IN B", "OUT B" and DeviceID 3 validated, *When* the application is closed and started again, *Then* the MIDI setup shows "IN B", "OUT B" and 3.
- **Dependencies**: RQ-APP-001, RQ-APP-015

### RQ-APP-017: Connecting to the sampler
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person chooses "Connect", the application SHALL open the configured ports and the session with the sampler (RQ-AKM-039, RQ-AKM-040), and, once the session is ready, SHALL read and show the sampler's model, name and operating-system version, then fill the file browser (RQ-APP-024) and the memory view (RQ-APP-035).
- **Rationale**: nothing can be browsed or edited before the session is open; the model and version tell the person which sampler answered.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler at DeviceID 0 and the MIDI setup at DeviceID 0, *When* the person chooses "Connect", *Then* the status shows connected with the model, name and OS version, and the browser and the memory view are filled.
- **Dependencies**: RQ-APP-001, RQ-APP-015, RQ-AKM-039, RQ-AKM-040, RQ-AKM-044, RQ-AKM-052, RQ-AKM-053

### RQ-APP-018: A failed connection says why
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF the connection fails (a configured port missing, no sampler at the DeviceID, several samplers answering, a section 00 setting refused), THEN the application SHALL stay disconnected and say which of these happened, naming the missing port or the DeviceIDs that answered, without stopping or freezing.
- **Rationale**: the AKM layer tells these cases apart (`SessionConfig.hpp`, the open statuses); the person needs to know what to fix.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the MIDI setup at DeviceID 2 and the simulated sampler answering at DeviceID 0 only, *When* the person connects, *Then* the application stays disconnected and says no sampler answered at DeviceID 2 and one answered at 0; *Given* the stored input port absent from the computer, *Then* it names the port.
- **Dependencies**: RQ-APP-001, RQ-APP-017, RQ-MID-002, RQ-AKM-039, RQ-AKM-041

### RQ-APP-019: The connection state is always shown
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The main window SHALL always show the connection state: disconnected, connecting, connected (with the sampler's model and DeviceID) or failed (with the reason of RQ-APP-018), and the browser, the memory view and the edit panel SHALL refuse every action that needs the sampler while it is not connected.
- **Rationale**: the person must know whether what is shown comes from the sampler.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the application disconnected, *When* the person looks at the window, *Then* it says disconnected and the load, save and edit actions are disabled; *When* the connection succeeds, *Then* it says connected, the model and the DeviceID.
- **Dependencies**: RQ-APP-001, RQ-APP-017, RQ-APP-018

### RQ-APP-020: Changing the ports while connected
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person validates other ports or another DeviceID while connected, the application SHALL close the session (RQ-AKM-042) and the old ports (RQ-MID-003) before opening the new ones, and SHALL connect again with the new setup.
- **Rationale**: a session belongs to one pair of ports and one DeviceID.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a session open on "IN A"/"OUT A", *When* the person validates "IN B"/"OUT B", *Then* the session is closed and "IN A"/"OUT A" are released before "IN B"/"OUT B" are opened.
- **Dependencies**: RQ-APP-001, RQ-APP-016, RQ-APP-017, RQ-MID-003, RQ-AKM-042

### RQ-APP-021: Disconnecting and quitting close the session
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person chooses "Disconnect" or quits the application, the application SHALL close the session (RQ-AKM-042, which puts back the section 00 settings and releases a held front-panel key, DEC-AKM-019), then release the MIDI ports (RQ-MID-006).
- **Rationale**: the sampler is left as it was found.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a session open with checksums on, *When* the person quits, *Then* the session closing is sent before the ports are released.
- **Dependencies**: RQ-APP-001, RQ-APP-017, RQ-AKM-042, RQ-AKM-075, RQ-MID-006

### RQ-APP-022: A lost MIDI port is reported
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a MIDI port in use disappears or reports an error while connected (RQ-MID-023), THEN the application SHALL show the connection as failed with the port's name, stop sending, and let the person connect again once the port is back.
- **Rationale**: a USB MIDI interface can be unplugged.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a session open on a loopback port, *When* the port reports an error, *Then* the status shows failed with the port's name and "Connect" is offered again.
- **Dependencies**: RQ-APP-001, RQ-APP-019, RQ-MID-023

### RQ-APP-023: Connecting at start
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the application starts with a stored MIDI setup and the person has chosen to connect at start, the application SHALL connect as in RQ-APP-017 without waiting for "Connect".
- **Rationale**: the person usually works with the same sampler on the same ports.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* a stored setup with "connect at start" checked, *When* the application starts, *Then* it connects; *Given* the option unchecked, *Then* it stays disconnected.
- **Dependencies**: RQ-APP-001, RQ-APP-016, RQ-APP-017
