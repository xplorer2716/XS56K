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

// xs56k_akm_probe: the first contact with an AKAI S5000/S6000, run by the owner. It opens the sampler's two
// MIDI ports through JuceMidiBackend, sends the fifteen frames of the probe one at a time and writes what
// goes out and what comes back to a log, on the console and in a file. The logic is
// akm::harness::runFirstContactProbe, which CI runs against the simulated sampler. With --session it runs
// the session smoke test instead, akm::harness::runSessionSmokeTest: a real Session driven through the
// section 00 primitives, logged the same way. With --suite it runs the real-sampler suite,
// akm::harness::runRealSamplerSuite: checks on sessions opened with Session::open and closed with Session::close,
// and the observations of RQ-AKM-017 that remain.
// [TASK-AKM-010, TASK-AKM-012, TASK-AKM-013, RQ-AKM-017, RQ-AKM-018, RQ-AKM-044,
// ADR-AKM-001 (DEC-AKM-007, DEC-AKM-009)]
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI  // its ERROR macro would clash with the names of the layer
#endif
#include <windows.h>
#include <io.h>
#endif

#include "akm/Protocol.hpp"
#include "akm/harness/FirstContactProbe.hpp"
#include "akm/harness/FrontPanelRemote.hpp"
#include "akm/harness/RealSamplerSuite.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "akm/harness/SessionSmokeTest.hpp"
#include "common/midi/JuceMidiBackend.hpp"

namespace
{
    // Exit codes.
    constexpr int EXIT_OK = 0;
    constexpr int EXIT_USAGE = 1;
    constexpr int EXIT_NO_ANSWER = 2;
    constexpr int EXIT_STEP_FAILED = 3;  // --session, --suite: the sampler answered, but a step or a check did not go as it had to

    constexpr std::uint32_t DEFAULT_DEVICE_ID = 0;  // the sampler's default DeviceID
    constexpr std::uint32_t DEFAULT_OTHER_DEVICE_ID = 5;
    constexpr long long DEFAULT_TIMEOUT_MS = 3000;

    constexpr const char* USAGE =
        "Usage:\n"
        "  xs56k_akm_probe --list\n"
        "  xs56k_akm_probe --in <input port> --out <output port> [--device-id N] [--other-device-id N]\n"
        "                  [--timeout-ms N] [--log <file>] [--yes]\n"
        "  xs56k_akm_probe --session --in <input port> --out <output port> [--device-id N] [--no-lcd]\n"
        "                  [--timeout-ms N] [--log <file>] [--yes]\n"
        "  xs56k_akm_probe --suite --in <input port> --out <output port> [--device-id N] [--no-lcd]\n"
        "                  [--power-cycle] [--slow-operation] [--program-lifecycle] [--sample-lifecycle]\n"
        "                  [--system-setup] [--disk-tools] [--disk-tools-slow OP] [--front-panel] [--sample-name NAME]\n"
        "                  [--timeout-ms N] [--log <file>] [--yes]\n"
        "\n"
        "  --list             list the MIDI input and output ports and exit\n"
        "  --in, --out        the sampler's MIDI input port (what it sends) and output port (what it receives),\n"
        "                     by the names --list shows\n"
        "  --device-id        the DeviceID of the sampler (default 0, the sampler's default)\n"
        "  --other-device-id  another DeviceID, for the step that shows how addressing works (default 5)\n"
        "  --session          run the session smoke test instead of the first-contact probe: a real session\n"
        "                     driven through discovery, the checksum mode, Echo, the OS version and the other\n"
        "                     section 00 settings\n"
        "  --suite            run the real-sampler suite instead: seven checks, each on a session opened with\n"
        "                     Session::open and closed with Session::close (open and close, Echo, 50 timed Echo round\n"
        "                     trips, the OS version, checksums on and off, every setting put back, a check that fails\n"
        "                     half way), then the observations of RQ-AKM-017\n"
        "  --no-lcd           with --session or --suite, do not switch Sync LCD and Auto screen update\n"
        "  --power-cycle      with --suite, an extra check: it asks you to switch the sampler off and on while a\n"
        "                     session is open, to see what survives\n"
        "  --slow-operation   with --suite, an extra check: it sends one command outside sections 00 and 02, \"update the\n"
        "                     list of disks\" (section 10, item 01), with Still Alive on, to see whether F0 F7\n"
        "                     messages reach this computer while the sampler works\n"
        "  --program-lifecycle  with --suite, four extra checks: they create, change, select and delete a\n"
        "                     program under the reserved name \"XS56K_SUITE_TEST\", add keygroups to it and\n"
        "                     round-trip every section 08 item on them and every non-sample section 06 item\n"
        "                     on a zone, including zone 0 (all four) and keygroup 0 + zone 0, and restore the\n"
        "                     program that was current before (RQ-AKM-027, RQ-AKM-030, RQ-AKM-033, RQ-AKM-034,\n"
        "                     RQ-AKM-036) - the only checks that touch a stored program\n"
        "  --sample-lifecycle   with --suite, a fifth extra check, independent of --program-lifecycle (it needs\n"
        "                     no stored program): it selects the sample --sample-name names, renames it and\n"
        "                     back, starts and stops auditioning it, round-trips every settable section 0E\n"
        "                     item on it and confirms the grouped replies &34/&4B agree with the items they\n"
        "                     group (RQ-AKM-045, RQ-AKM-048, RQ-AKM-049, RQ-AKM-051), then restores its name,\n"
        "                     every parameter and the sampler's original current-sample selection. Never\n"
        "                     sends &07 or &08. Skipped, not failed, without --sample-name.\n"
        "  --system-setup     with --suite, two extra checks on the sampler's own settings (RQ-AKM-052 to\n"
        "                     RQ-AKM-055, RQ-AKM-058): they read the model and the memory, then round-trip the\n"
        "                     sampler's name, its four Play Modes (Muted included: it silences the sampler for an\n"
        "                     instant), its front-panel lock (locked for an instant) and its clock, and put each\n"
        "                     back - the lock first, the clock advanced by the time elapsed. Never sends Clear\n"
        "                     Sampler Memory (section 02, item 32). Needs no sample, program or --sample-name.\n"
        "  --disk-tools       with --suite, an extra check on the disk (section 10, RQ-AKM-071): it lists the disks,\n"
        "                     asks you which of the writable disks the sampler reports valid to select (the selection\n"
        "                     stays: no section 10 command clears it), creates a disposable sub-folder XS56K_SUITE_TEST under\n"
        "                     its current folder, creates, renames, enters and leaves a sub-folder inside it, reads\n"
        "                     the folder and file items, and deletes the whole sub-folder again through the confirmed\n"
        "                     &17 guard. It touches nothing that existed before. If a disk check ends early, you are asked\n"
        "                     to look at XS56K_SUITE_TEST on the sampler first: Enter deletes it, skip keeps it.\n"
        "  --disk-tools-files  with --disk-tools, the file items of section 10 (RQ-AKM-068, RQ-AKM-069): one save of the\n"
        "                     test program into the sub-folder (you confirm the file on the sampler), then the file is\n"
        "                     read, renamed, read again and deleted. No audition: the spec allows it for samples only.\n"
        "  --disk-tools-audition  with --disk-tools, the audition of a sample from disk (section 10, items 30 and 31,\n"
        "                     RQ-AKM-068): you confirm that the selected disk holds a .WAV file at its root; the first\n"
        "                     one is played for 3 seconds (it plays a sound), then stopped. A sample shorter than 3\n"
        "                     seconds may already have ended: the stop is then recorded, not failed.\n"
        "  --disk-tools-slow OP  with --disk-tools, one of the six long-running section 10 items, sent inside the\n"
        "                     sub-folder with Still Alive on (RQ-AKM-070). OP is one of: update-list (item 01),\n"
        "                     load-folder (item 15), load-file (items 2C then 2A), load-file-with-dependents (items\n"
        "                     2C then 2B), save-memory-item (item 2C), save-all-memory-items (item 2D). Only one per\n"
        "                     run. These are documented as potentially hanging the sampler, which then needs a\n"
        "                     power cycle by hand; update-list did so on the owner's S5000 with a SCSI2SD disk\n"
        "                     (a warning is printed before it runs). See process/2.architecture/\n"
        "                     OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md, frames F4-F7. The saving items ask you, on\n"
        "                     the sampler, to confirm the file a save has made (between a save and its load, for the\n"
        "                     load items): declining skips the check.\n"
        "  --front-panel      with --suite, you drive the sampler's front panel from the PC keyboard (section 20,\n"
        "                     RQ-AKM-076). You choose the screen the sampler shows and confirm it; the mapping of PC keys\n"
        "                     to sampler keys is then printed and every key you press is sent as the sampler key it stands\n"
        "                     for, nothing else is sent (q ends it). The keys act on whatever the sampler shows: on some\n"
        "                     screens SAVE, ENT/PLAY or the data wheel change or delete your data, so choose the screen\n"
        "                     with care. Every key still held is released at the end. Windows console only: elsewhere the\n"
        "                     check is skipped.\n"
        "  --sample-name      a sample already in the sampler's memory, named for --program-lifecycle (assigns\n"
        "                     it to a zone of the test program by name and reads it back, RQ-AKM-035,\n"
        "                     RQ-AKM-038) and/or --sample-lifecycle (see above). Never creates, changes or\n"
        "                     deletes a sample itself. Needs at least one of --program-lifecycle or\n"
        "                     --sample-lifecycle.\n"
        "  --timeout-ms       how long each step waits for an answer (default 3000)\n"
        "  --log              the log file (default akm-probe-<UTC date and time>.log, or akm-session-... with\n"
        "                     --session, or akm-suite-... with --suite, in this directory)\n"
        "  --yes              do not ask for confirmation before sending\n"
        "\n"
        "The probe switches the sampler's checksum and Still Alive settings on and off and ends with both off.\n"
        "The session smoke test and the suite also switch Notification, Sync LCD and Auto screen update, and end with\n"
        "checksums off, Still Alive off, Notification on, Sync LCD on and Auto screen update off.\n"
        "None of them changes a stored program unless --program-lifecycle is given, and then only the one it\n"
        "creates itself and always deletes again. None of them changes a stored sample's name or parameters\n"
        "beyond a check that puts them back before returning (--program-lifecycle's zone-assignment step never\n"
        "does; --sample-lifecycle's own check does, and restores them), and neither ever deletes a sample.\n";

#ifdef _WIN32
    // The owner's keys for --front-panel, one at a time and without Enter (RQ-AKM-076), read as console key events
    // (`ReadConsoleInputW`) rather than as characters (`_getch`): an event says which key it was apart from what it typed,
    // so a character such as `à` (0xE0) is never taken for the prefix of an extended key, and the number row can be
    // read by position. Function, arrow and page keys become the codes of `pc_key`; a character beyond ASCII, and any
    // other key with no character, a code the mapping gives no meaning to.
    constexpr WORD SCAN_NUMBER_ROW_FIRST = 0x02;  // the key printed 1 on a QWERTY keyboard
    constexpr WORD SCAN_NUMBER_ROW_ZERO = 0x0B;   // the key printed 0, the last of the row
    constexpr int ASCII_LIMIT = 128;
    constexpr int UNMAPPED_EXTENDED_KEY = akm::harness::pc_key::EXTENDED_BASE + 100;

    std::optional<int> readConsoleKey(bool textMode)
    {
        namespace pc_key = akm::harness::pc_key;
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        for (;;)
        {
            INPUT_RECORD record{};
            DWORD read = 0;
            if (!ReadConsoleInputW(input, &record, 1, &read) || read == 0)
                return std::nullopt;
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                continue;
            const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
            const WORD virtualKey = key.wVirtualKeyCode;
            if (virtualKey >= VK_F1 && virtualKey <= VK_F12)
                return pc_key::F1 + (virtualKey - VK_F1);
            switch (virtualKey)
            {
                case VK_UP:
                    return pc_key::UP;
                case VK_DOWN:
                    return pc_key::DOWN;
                case VK_LEFT:
                    return pc_key::LEFT;
                case VK_RIGHT:
                    return pc_key::RIGHT;
                case VK_PRIOR:
                    return pc_key::PAGE_UP;
                case VK_NEXT:
                    return pc_key::PAGE_DOWN;
                default:
                    break;
            }
            // In the normal mode the number row is the digits printed on it, wherever the layout puts them: an AZERTY row
            // types `&é"'(-è_çà` unshifted, which would not be digits. Not with Ctrl, Alt or AltGr, which type symbols; not
            // in the text mode, where what is typed is what is sent; not the numeric pad, whose keys type their digits.
            const bool symbolModifier =
                (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
            const bool numberRow = key.wVirtualScanCode >= SCAN_NUMBER_ROW_FIRST && key.wVirtualScanCode <= SCAN_NUMBER_ROW_ZERO
                                   && (key.dwControlKeyState & ENHANCED_KEY) == 0;
            if (!textMode && numberRow && !symbolModifier)
            {
                if (key.wVirtualScanCode == SCAN_NUMBER_ROW_ZERO)
                    return '0';
                return '1' + (key.wVirtualScanCode - SCAN_NUMBER_ROW_FIRST);
            }
            const int character = key.uChar.UnicodeChar;
            if (character >= ASCII_LIMIT)
                return UNMAPPED_EXTENDED_KEY;
            if (character != 0)
                return character;
            // A modifier alone, a dead key: nothing was typed yet.
        }
    }

    // Whether keys can be read one at a time: only from a console, not from a file or a pipe.
    bool consoleKeysAvailable()
    {
        return _isatty(_fileno(stdin)) != 0;
    }
#endif

    struct Arguments
    {
        bool list = false;
        bool help = false;
        bool yes = false;
        bool session = false;
        bool suite = false;
        bool powerCycle = false;
        bool slowOperation = false;
        bool programLifecycle = false;
        bool sampleLifecycle = false;
        bool systemSetup = false;
        bool diskTools = false;
        bool diskToolsFiles = false;
        bool diskToolsAudition = false;
        bool frontPanel = false;
        bool noLcd = false;
        std::string diskToolsSlow;
        std::string input;
        std::string output;
        std::string logPath;
        std::string sampleName;
        std::uint32_t deviceId = DEFAULT_DEVICE_ID;
        std::uint32_t otherDeviceId = DEFAULT_OTHER_DEVICE_ID;
        long long timeoutMs = DEFAULT_TIMEOUT_MS;
        std::string error;
    };

    // The value of the option at `index`, or empty (with an error) when it has none.
    std::string valueOf(const std::vector<std::string>& args, std::size_t index, Arguments& parsed)
    {
        if (index + 1 >= args.size())
        {
            parsed.error = "the option " + args[index] + " needs a value";
            return {};
        }
        return args[index + 1];
    }

    // The §10 long-running item named on the command line by --disk-tools-slow (RQ-AKM-070), or nothing.
    std::optional<akm::harness::DiskSlowOperation> diskSlowOperationNamed(const std::string& name)
    {
        using akm::harness::DiskSlowOperation;
        if (name == "update-list")
            return DiskSlowOperation::UpdateList;
        if (name == "load-folder")
            return DiskSlowOperation::LoadFolder;
        if (name == "load-file")
            return DiskSlowOperation::LoadFile;
        if (name == "load-file-with-dependents")
            return DiskSlowOperation::LoadFileWithDependents;
        if (name == "save-memory-item")
            return DiskSlowOperation::SaveMemoryItem;
        if (name == "save-all-memory-items")
            return DiskSlowOperation::SaveAllMemoryItems;
        return std::nullopt;
    }

    bool parseNumber(const std::string& text, long long& value)
    {
        try
        {
            std::size_t used = 0;
            value = std::stoll(text, &used);
            return used == text.size();
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    // The text field a value-taking option fills; --sample-name is the last of the four.
    std::string& textOptionTarget(Arguments& parsed, const std::string& option)
    {
        if (option == "--in")
            return parsed.input;
        if (option == "--out")
            return parsed.output;
        if (option == "--log")
            return parsed.logPath;
        return parsed.sampleName;
    }

    Arguments parseArguments(const std::vector<std::string>& args)
    {
        Arguments parsed;
        for (std::size_t index = 1; index < args.size() && parsed.error.empty(); ++index)
        {
            const std::string& option = args[index];
            long long number = 0;
            if (option == "--list")
                parsed.list = true;
            else if (option == "--help" || option == "-h")
                parsed.help = true;
            else if (option == "--yes")
                parsed.yes = true;
            else if (option == "--session")
                parsed.session = true;
            else if (option == "--suite")
                parsed.suite = true;
            else if (option == "--power-cycle")
                parsed.powerCycle = true;
            else if (option == "--slow-operation")
                parsed.slowOperation = true;
            else if (option == "--program-lifecycle")
                parsed.programLifecycle = true;
            else if (option == "--sample-lifecycle")
                parsed.sampleLifecycle = true;
            else if (option == "--system-setup")
                parsed.systemSetup = true;
            else if (option == "--disk-tools")
                parsed.diskTools = true;
            else if (option == "--disk-tools-files")
                parsed.diskToolsFiles = true;
            else if (option == "--disk-tools-audition")
                parsed.diskToolsAudition = true;
            else if (option == "--front-panel")
                parsed.frontPanel = true;
            else if (option == "--disk-tools-slow")
            {
                parsed.diskToolsSlow = valueOf(args, index++, parsed);
                if (!parsed.error.empty())
                    break;
            }
            else if (option == "--no-lcd")
                parsed.noLcd = true;
            else if (option == "--in" || option == "--out" || option == "--log" || option == "--sample-name")
            {
                const std::string value = valueOf(args, index++, parsed);
                textOptionTarget(parsed, option) = value;
            }
            else if (option == "--device-id" || option == "--other-device-id" || option == "--timeout-ms")
            {
                const std::string value = valueOf(args, index++, parsed);
                if (!parsed.error.empty())
                    break;
                if (!parseNumber(value, number) || number < 0)
                {
                    parsed.error = "the option " + option + " needs a non-negative number, not \"" + value + "\"";
                    break;
                }
                if (option == "--timeout-ms")
                    parsed.timeoutMs = number;
                else if (number > akm::DEVICE_ID_MAX)
                {
                    parsed.error = "a DeviceID is at most 31";
                    break;
                }
                else
                    (option == "--device-id" ? parsed.deviceId : parsed.otherDeviceId) = static_cast<std::uint32_t>(number);
            }
            else
                parsed.error = "unknown option " + option;
        }
        if (parsed.error.empty() && parsed.session && parsed.suite)
            parsed.error = "--session and --suite cannot be used together";
        if (parsed.error.empty()
            && !parsed.suite
            && (parsed.powerCycle || parsed.slowOperation || parsed.programLifecycle || parsed.sampleLifecycle || parsed.systemSetup
                || parsed.diskTools || parsed.diskToolsFiles || parsed.diskToolsAudition || !parsed.diskToolsSlow.empty()
                || parsed.frontPanel || !parsed.sampleName.empty()))
            parsed.error = "--power-cycle, --slow-operation, --program-lifecycle, --sample-lifecycle, --system-setup, --disk-tools, "
                           "--disk-tools-files, --disk-tools-audition, --disk-tools-slow, --front-panel and --sample-name need --suite";
        if (parsed.error.empty() && parsed.diskToolsFiles && !parsed.diskTools)
            parsed.error = "--disk-tools-files needs --disk-tools";
        if (parsed.error.empty() && parsed.diskToolsAudition && !parsed.diskTools)
            parsed.error = "--disk-tools-audition needs --disk-tools";
        if (parsed.error.empty() && !parsed.diskToolsSlow.empty() && !parsed.diskTools)
            parsed.error = "--disk-tools-slow needs --disk-tools";
        if (parsed.error.empty() && !parsed.diskToolsSlow.empty() && !diskSlowOperationNamed(parsed.diskToolsSlow))
            parsed.error = "--disk-tools-slow needs one of update-list, load-folder, load-file, load-file-with-dependents, "
                           "save-memory-item, save-all-memory-items, not \"" + parsed.diskToolsSlow + "\"";
        if (parsed.error.empty() && !parsed.sampleName.empty() && !parsed.programLifecycle && !parsed.sampleLifecycle)
            parsed.error = "--sample-name needs --program-lifecycle or --sample-lifecycle";
        return parsed;
    }

    // The current UTC time as "YYYY-MM-DDTHH:MM:SSZ", or with `compact` as "YYYYMMDD-HHMMSS" for a file name.
    std::string utcNow(bool compact)
    {
        const std::time_t now = std::time(nullptr);
        std::tm parts{};
#if defined(_WIN32)
        gmtime_s(&parts, &now);
#else
        gmtime_r(&now, &parts);
#endif
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), compact ? "%Y%m%d-%H%M%S" : "%Y-%m-%dT%H:%M:%SZ", &parts);
        return buffer;
    }

    // Writes to two stream buffers at once: the console and the log file.
    class TeeBuffer final : public std::streambuf
    {
    public:
        TeeBuffer(std::streambuf* first, std::streambuf* second) : _first(first), _second(second) {}

    protected:
        int_type overflow(int_type character) override
        {
            if (traits_type::eq_int_type(character, traits_type::eof()))
                return traits_type::not_eof(character);
            const auto written = traits_type::to_char_type(character);
            const bool firstOk = !traits_type::eq_int_type(_first->sputc(written), traits_type::eof());
            const bool secondOk = !traits_type::eq_int_type(_second->sputc(written), traits_type::eof());
            return firstOk && secondOk ? character : traits_type::eof();
        }

        int sync() override
        {
            const int first = _first->pubsync();
            const int second = _second->pubsync();
            return first == 0 && second == 0 ? 0 : -1;
        }

    private:
        std::streambuf* _first;
        std::streambuf* _second;
    };

    void listPorts(common::midi::MidiBackend& backend)
    {
        std::cout << "MIDI input ports (what the sampler sends to this computer):\n";
        for (const std::string& name : backend.inputDeviceNames())
            std::cout << "  \"" << name << "\"\n";
        std::cout << "MIDI output ports (what this computer sends to the sampler):\n";
        for (const std::string& name : backend.outputDeviceNames())
            std::cout << "  \"" << name << "\"\n";
    }

    // What the real-sampler suite is about to do, for the owner to read before pressing Enter.
    void describeSuite(const Arguments& arguments)
    {
        std::cout << "The real-sampler suite will send SysEx frames to \"" << arguments.output << "\" and listen on \""
                  << arguments.input << "\".\n"
                  << "Each check opens a session and closes it. It switches the sampler's checksum, Notification, Still Alive"
                  << (arguments.noLcd ? "" : ", Sync LCD and Auto screen update") << " settings on and off, and ends with\n"
                  << "checksums off, Still Alive off, Notification on"
                  << (arguments.noLcd ? "" : ", Sync LCD on and Auto screen update off")
                  << (arguments.programLifecycle || arguments.sampleLifecycle || arguments.systemSetup
                          ? ".\n"
                          : ". It changes no stored program or sample.\n");
        if (arguments.slowOperation)
            std::cout << "It also sends one command outside sections 00 and 02: update the list of disks (section 10, item 01).\n";
        if (arguments.powerCycle)
            std::cout << "It will ask you to switch the sampler off and on while a session is open.\n";
        if (arguments.programLifecycle)
        {
            std::cout << "It will also create, change, select and delete a program named \"XS56K_SUITE_TEST\", add\n"
                      << "keygroups to it, round-trip every section 08 item and every non-sample section 06 item on\n"
                      << "them, and restore the program that was current before; no other program, multi or sample\n"
                      << "is touched.\n";
            if (!arguments.sampleName.empty())
                std::cout << "It will also assign the sample \"" << arguments.sampleName
                          << "\" to a zone of that program by name and read it back; the sample itself is never\n"
                          << "created, changed or deleted.\n";
            else
                std::cout << "Sample assignment is skipped: no --sample-name was given.\n";
        }
        if (arguments.systemSetup)
            std::cout << "It will also change the sampler's own settings and put them back: its name, its Play Mode (all four,\n"
                      << "Muted included, which silences it for an instant), its front-panel lock (locked for an instant) and\n"
                      << "its clock (advanced by the time elapsed when put back, to about three seconds). It never sends Clear\n"
                      << "Sampler Memory. Note the sampler's name and time before you start.\n";
        if (arguments.diskTools)
            std::cout << "It will also ask you which writable disk the sampler reports valid to select, create the sub-folder\n"
                      << "XS56K_SUITE_TEST under its current folder, work inside it and delete it again. The selection stays\n"
                      << "on the sampler (no command clears it) and nothing that existed before is touched. Note the disks\n"
                      << "and the current folder on the sampler before you start.\n";
        if (arguments.diskToolsAudition)
            std::cout << "It will also play the first .WAV file at the root of the selected disk for 3 seconds (it plays a\n"
                      << "sound), once you confirm that one is there. Nothing is saved for it.\n";
        if (arguments.diskToolsFiles)
            std::cout << "It will also save the test program into the sub-folder (you confirm the file on the sampler), read,\n"
                      << "rename and delete the file.\n";
        if (arguments.frontPanel)
            std::cout << "It will also let you drive the sampler's front panel from the PC keyboard: you choose the screen the\n"
                      << "sampler shows, then each key you press is sent as the sampler key it stands for (the mapping is printed\n"
                      << "first), nothing else. The keys act on whatever the sampler shows: SAVE, ENT/PLAY or the data wheel can\n"
                      << "change or delete your data on some screens. Put the sampler on a screen where that cannot hurt.\n";
        if (!arguments.diskToolsSlow.empty())
            std::cout << "It will also send one long-running section 10 item, \"" << arguments.diskToolsSlow << "\", inside the\n"
                      << "sub-folder, with Still Alive on. Such an item has hung this sampler before (frames F4-F7 of\n"
                      << "process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md): if it does, the sampler will\n"
                      << "need a power cycle by hand. Be ready to do that.\n";
        if (arguments.diskToolsSlow == "update-list")
            std::cout << "WARNING: update-list hangs the sampler on this rig: the S5000 (OS 2.14) with a SCSI2SD disk (an\n"
                      << "SCSI emulator on an SD card) stopped answering on it, from this check and from the sampler's own\n"
                      << "screen. Expect the check to fail and the sampler to need a power cycle by hand, then the folder\n"
                      << "XS56K_SUITE_TEST to be removed by hand.\n";
        if (arguments.sampleLifecycle)
        {
            if (!arguments.sampleName.empty())
                std::cout << "It will also select the sample \"" << arguments.sampleName
                          << "\", rename it and back, start and stop auditioning it, round-trip every settable\n"
                          << "section 0E item on it, and restore its name, its parameters and the sampler's\n"
                          << "original current-sample selection; it never sends section 0E's Delete ALL or\n"
                          << "Delete current sample item.\n";
            else
                std::cout << "The sample lifecycle check is skipped: no --sample-name was given.\n";
        }
    }
}

int main(int argc, char** argv)
{
    const Arguments arguments = parseArguments(std::vector<std::string>(argv, argv + argc));
    if (!arguments.error.empty())
    {
        std::cerr << "xs56k_akm_probe: " << arguments.error << "\n\n" << USAGE;
        return EXIT_USAGE;
    }
    if (arguments.help)
    {
        std::cout << USAGE;
        return EXIT_OK;
    }

    common::midi::JuceMidiBackend backend;
    if (arguments.list)
    {
        listPorts(backend);
        return EXIT_OK;
    }
    if (arguments.input.empty() || arguments.output.empty())
    {
        std::cerr << "xs56k_akm_probe: --in and --out are required (see --list for the port names)\n\n" << USAGE;
        listPorts(backend);
        return EXIT_USAGE;
    }

    const std::string logPrefix = arguments.suite ? "akm-suite-" : arguments.session ? "akm-session-" : "akm-probe-";
    const std::string logPath = arguments.logPath.empty() ? logPrefix + utcNow(true) + ".log" : arguments.logPath;
    std::ofstream file(logPath);
    if (!file)
    {
        std::cerr << "xs56k_akm_probe: cannot write the log file " << logPath << "\n";
        return EXIT_USAGE;
    }

    if (!arguments.yes)
    {
        if (arguments.suite)
            describeSuite(arguments);
        else if (arguments.session)
            std::cout << "The session smoke test will send SysEx frames to \"" << arguments.output << "\" and listen on \""
                      << arguments.input << "\".\n"
                      << "It switches the sampler's checksum, Notification, Still Alive"
                      << (arguments.noLcd ? "" : ", Sync LCD and Auto screen update") << " settings on and off, and ends with\n"
                      << "checksums off, Still Alive off, Notification on"
                      << (arguments.noLcd ? "" : ", Sync LCD on and Auto screen update off") << ".\n";
        else
            std::cout << "The probe will send fifteen SysEx frames to \"" << arguments.output << "\" and listen on \""
                      << arguments.input << "\".\n"
                      << "It switches the sampler's checksum and Still Alive settings on and off and ends with both off.\n";
        std::cout << "Press Enter to start, Ctrl+C to cancel: " << std::flush;
        std::string ignored;
        std::getline(std::cin, ignored);
    }

    TeeBuffer tee(std::cout.rdbuf(), file.rdbuf());
    std::ostream log(&tee);
    const akm::harness::ScenarioTarget target{arguments.input, arguments.output, arguments.deviceId};

    akm::harness::RealScenarioDriver driver;
    if (arguments.suite)
    {
        akm::harness::RealSuiteOptions options;
        options.target = target;
        options.commandTimeout = std::chrono::milliseconds(arguments.timeoutMs);
        options.touchLcdSettings = !arguments.noLcd;
        options.slowOperation = arguments.slowOperation;
        options.powerCycle = arguments.powerCycle;
        options.programLifecycle = arguments.programLifecycle;
        options.sampleLifecycle = arguments.sampleLifecycle;
        options.systemSetup = arguments.systemSetup;
        options.diskTools = arguments.diskTools;
        options.diskToolsFiles = arguments.diskToolsFiles;
        options.diskToolsAudition = arguments.diskToolsAudition;
        options.frontPanel = arguments.frontPanel;
#ifdef _WIN32
        if (consoleKeysAvailable())
            options.readOwnerKey = readConsoleKey;
#endif
        options.tellOwner = [](const std::string& line) { std::cout << "    " << line << std::endl; };
        if (!arguments.diskToolsSlow.empty())
            options.diskToolsSlow = diskSlowOperationNamed(arguments.diskToolsSlow);
        if (!arguments.sampleName.empty())
            options.sampleName = arguments.sampleName;
        options.startedAt = utcNow(false);
        options.askOwner = [](const std::string& instruction) {
            std::cout << "\n>>> " << instruction << "\n    Press Enter when it is done, or type skip to skip this check: " << std::flush;
            std::string answer;
            std::getline(std::cin, answer);
            return answer != "skip";
        };
        options.askOwnerChoice = [](const std::string& question, const std::vector<std::string>& choices) -> std::optional<std::size_t> {
            std::cout << "\n>>> " << question << '\n';
            for (std::size_t index = 0; index < choices.size(); ++index)
                std::cout << "    " << (index + 1) << ". " << choices[index] << '\n';
            for (;;)
            {
                std::cout << "    Type the number of the disk, or skip to skip this check: " << std::flush;
                std::string answer;
                if (!std::getline(std::cin, answer) || answer == "skip")
                    return std::nullopt;
                try
                {
                    const std::size_t number = std::stoul(answer);
                    if (number >= 1 && number <= choices.size())
                        return number - 1;
                }
                catch (const std::exception&)
                {
                }
                std::cout << "    Not one of the numbers above.\n";
            }
        };

        const akm::harness::RealSuiteResult result = akm::harness::runRealSamplerSuite(backend, driver, options, log);
        log << std::flush;

        std::cout << "\nLog written to " << logPath << "\n";
        if (!result.portsOpened)
            return EXIT_USAGE;
        const auto answered = std::find(result.discoveredDeviceIds.begin(), result.discoveredDeviceIds.end(),
                                        static_cast<std::uint8_t>(arguments.deviceId));
        if (answered == result.discoveredDeviceIds.end())
            return EXIT_NO_ANSWER;
        return result.passed() && result.knownStateRestored ? EXIT_OK : EXIT_STEP_FAILED;
    }
    if (arguments.session)
    {
        akm::harness::SessionSmokeOptions options;
        options.target = target;
        options.stepTimeout = std::chrono::milliseconds(arguments.timeoutMs);
        options.touchLcdSettings = !arguments.noLcd;
        options.startedAt = utcNow(false);

        const akm::harness::SessionSmokeResult result = akm::harness::runSessionSmokeTest(backend, driver, options, log);
        log << std::flush;

        std::cout << "\nLog written to " << logPath << "\n";
        if (!result.portsOpened)
            return EXIT_USAGE;
        if (!result.samplerFound)
            return EXIT_NO_ANSWER;
        return result.failedSteps.empty() && result.knownStateRestored ? EXIT_OK : EXIT_STEP_FAILED;
    }

    akm::harness::ProbeOptions options;
    options.target = target;
    options.otherDeviceId = arguments.otherDeviceId;
    options.answerTimeout = std::chrono::milliseconds(arguments.timeoutMs);
    options.startedAt = utcNow(false);

    const akm::harness::ProbeResult result = akm::harness::runFirstContactProbe(backend, driver, options, log);
    log << std::flush;

    std::cout << "\nLog written to " << logPath << "\n";
    if (!result.portsOpened)
        return EXIT_USAGE;
    return result.anySamplerAnswered ? EXIT_OK : EXIT_NO_ANSWER;
}
