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
// section 00 primitives, logged the same way.
// [TASK-AKM-012, TASK-AKM-013, RQ-AKM-017, RQ-AKM-018, RQ-AKM-044, ADR-AKM-001 (DEC-AKM-007, DEC-AKM-009)]
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <vector>

#include "akm/Protocol.hpp"
#include "akm/harness/FirstContactProbe.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "akm/harness/SessionSmokeTest.hpp"
#include "common/midi/JuceMidiBackend.hpp"

namespace
{
    // Exit codes.
    constexpr int EXIT_OK = 0;
    constexpr int EXIT_USAGE = 1;
    constexpr int EXIT_NO_ANSWER = 2;
    constexpr int EXIT_STEP_FAILED = 3;  // --session: the sampler answered, but a step did not go as it had to

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
        "\n"
        "  --list             list the MIDI input and output ports and exit\n"
        "  --in, --out        the sampler's MIDI input port (what it sends) and output port (what it receives),\n"
        "                     by the names --list shows\n"
        "  --device-id        the DeviceID of the sampler (default 0, the sampler's default)\n"
        "  --other-device-id  another DeviceID, for the step that shows how addressing works (default 5)\n"
        "  --session          run the session smoke test instead of the first-contact probe: a real session\n"
        "                     driven through discovery, the checksum mode, Echo, the OS version and the other\n"
        "                     section 00 settings\n"
        "  --no-lcd           with --session, do not switch Sync LCD and Auto screen update\n"
        "  --timeout-ms       how long each step waits for an answer (default 3000)\n"
        "  --log              the log file (default akm-probe-<UTC date and time>.log, or akm-session-... with\n"
        "                     --session, in this directory)\n"
        "  --yes              do not ask for confirmation before sending\n"
        "\n"
        "The probe switches the sampler's checksum and Still Alive settings on and off and ends with both off.\n"
        "The session smoke test also switches Notification, Sync LCD and Auto screen update, and ends with\n"
        "checksums off, Still Alive off, Notification on, Sync LCD on and Auto screen update off.\n";

    struct Arguments
    {
        bool list = false;
        bool help = false;
        bool yes = false;
        bool session = false;
        bool noLcd = false;
        std::string input;
        std::string output;
        std::string logPath;
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
            else if (option == "--no-lcd")
                parsed.noLcd = true;
            else if (option == "--in" || option == "--out" || option == "--log")
            {
                const std::string value = valueOf(args, index++, parsed);
                (option == "--in" ? parsed.input : option == "--out" ? parsed.output : parsed.logPath) = value;
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

    const std::string logPrefix = arguments.session ? "akm-session-" : "akm-probe-";
    const std::string logPath = arguments.logPath.empty() ? logPrefix + utcNow(true) + ".log" : arguments.logPath;
    std::ofstream file(logPath);
    if (!file)
    {
        std::cerr << "xs56k_akm_probe: cannot write the log file " << logPath << "\n";
        return EXIT_USAGE;
    }

    if (!arguments.yes)
    {
        if (arguments.session)
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
