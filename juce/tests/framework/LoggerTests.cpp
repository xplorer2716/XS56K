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

// What the framework's Logger does today, pinned before its state is reorganised: filtering by level and by
// domain, the line it writes, the sink it writes to, and its use from several threads. The Logger is process-wide
// state, so every test starts and ends from the same known state. [TASK-QLT-006, RQ-QLT-005, RQ-QLT-004,
// ADR-QLT-001 (DEC-QLT-004), RQ-FMW-070, RQ-FMW-073]
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "midiapp/service/Logger.hpp"

using midiapp::service::ILogSink;
using midiapp::service::LogDomain;
using midiapp::service::Logger;
using midiapp::service::TraceLevel;

namespace
{
    constexpr int THREAD_COUNT = 4;
    constexpr int LINES_PER_THREAD = 100;
    constexpr int SOME_LINE = 42;

    // The lines a test's sink received. Shared, because Logger::configure takes the sink away.
    struct Lines
    {
        std::mutex mutex;
        std::vector<std::string> received;

        [[nodiscard]] std::vector<std::string> snapshot()
        {
            const std::lock_guard lock(mutex);
            return received;
        }
    };

    class RecordingSink final : public ILogSink
    {
    public:
        explicit RecordingSink(std::shared_ptr<Lines> lines) : _lines(std::move(lines)) {}

        void write(const std::string& line) override
        {
            const std::lock_guard lock(_lines->mutex);
            _lines->received.push_back(line);
        }

    private:
        std::shared_ptr<Lines> _lines;
    };

    // The Logger is process-wide: a test leaves it as it found it, silent, with every domain on and no sink.
    void resetLogger()
    {
        Logger::shutdown();
        Logger::setLevel(TraceLevel::Off);
        Logger::setDomainEnabled(LogDomain::Midi, true);
        Logger::setDomainEnabled(LogDomain::ControllerCalls, true);
        Logger::setDomainEnabled(LogDomain::UiEvents, true);
    }

    class LoggerScope
    {
    public:
        LoggerScope() { resetLogger(); }
        ~LoggerScope() { resetLogger(); }
        LoggerScope(const LoggerScope&) = delete;
        LoggerScope& operator=(const LoggerScope&) = delete;
    };

    std::shared_ptr<Lines> installRecordingSink()
    {
        auto lines = std::make_shared<Lines>();
        Logger::configure(std::make_unique<RecordingSink>(lines));
        return lines;
    }
}

TEST_CASE("Given a sink and the level Info, When a Warning line is logged under an enabled domain, Then the sink gets one line with the level, the domain, the file's base name, the line and the message [RQ-FMW-070, RQ-FMW-073]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Info);

    Logger::writeLine(LogDomain::Midi, TraceLevel::Warning, "C:\\some\\dir\\File.cpp", SOME_LINE, "the message");

    const auto received = lines->snapshot();
    REQUIRE(received.size() == 1);
    CHECK_THAT(received[0], Catch::Matchers::ContainsSubstring("[WARNING] [MIDI] File.cpp:42: the message"));
}

TEST_CASE("Given a sink, When a line is logged, Then it starts with a UTC timestamp to the millisecond [RQ-FMW-070]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);

    Logger::writeLine(LogDomain::UiEvents, TraceLevel::Error, "File.cpp", SOME_LINE, "m");

    const auto received = lines->snapshot();
    REQUIRE(received.size() == 1);
    CHECK(std::regex_search(received[0], std::regex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} \[ERROR\] \[UI\] )")));
}

TEST_CASE("Given the three domains, When a line is logged under each, Then each is named MIDI, CTL or UI [RQ-FMW-073]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);

    Logger::writeLine(LogDomain::Midi, TraceLevel::Info, "F.cpp", 1, "a");
    Logger::writeLine(LogDomain::ControllerCalls, TraceLevel::Info, "F.cpp", 1, "b");
    Logger::writeLine(LogDomain::UiEvents, TraceLevel::Info, "F.cpp", 1, "c");

    const auto received = lines->snapshot();
    REQUIRE(received.size() == 3);
    CHECK_THAT(received[0], Catch::Matchers::ContainsSubstring("[INFO] [MIDI] "));
    CHECK_THAT(received[1], Catch::Matchers::ContainsSubstring("[INFO] [CTL] "));
    CHECK_THAT(received[2], Catch::Matchers::ContainsSubstring("[INFO] [UI] "));
}

TEST_CASE("Given the four levels, When a line is logged at each, Then each is named ERROR, WARNING, INFO or VERBOSE [RQ-FMW-070]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);

    Logger::writeLine(LogDomain::Midi, TraceLevel::Error, "F.cpp", 1, "m");
    Logger::writeLine(LogDomain::Midi, TraceLevel::Warning, "F.cpp", 1, "m");
    Logger::writeLine(LogDomain::Midi, TraceLevel::Info, "F.cpp", 1, "m");
    Logger::writeLine(LogDomain::Midi, TraceLevel::Verbose, "F.cpp", 1, "m");

    const auto received = lines->snapshot();
    REQUIRE(received.size() == 4);
    CHECK_THAT(received[0], Catch::Matchers::ContainsSubstring("[ERROR] "));
    CHECK_THAT(received[1], Catch::Matchers::ContainsSubstring("[WARNING] "));
    CHECK_THAT(received[2], Catch::Matchers::ContainsSubstring("[INFO] "));
    CHECK_THAT(received[3], Catch::Matchers::ContainsSubstring("[VERBOSE] "));
}

TEST_CASE("Given the level Warning, When an Info line and a Verbose line are logged, Then neither is written and an Error is [RQ-FMW-070]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Warning);

    Logger::writeLine(LogDomain::Midi, TraceLevel::Info, "F.cpp", 1, "info");
    Logger::writeLine(LogDomain::Midi, TraceLevel::Verbose, "F.cpp", 1, "verbose");
    Logger::writeLine(LogDomain::Midi, TraceLevel::Error, "F.cpp", 1, "error");

    const auto received = lines->snapshot();
    REQUIRE(received.size() == 1);
    CHECK_THAT(received[0], Catch::Matchers::ContainsSubstring("error"));
}

TEST_CASE("Given the level Off, When a line is logged at any level, Then nothing is written [RQ-FMW-070]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Off);

    Logger::writeLine(LogDomain::Midi, TraceLevel::Error, "F.cpp", 1, "m");
    Logger::writeLine(LogDomain::Midi, TraceLevel::Verbose, "F.cpp", 1, "m");

    CHECK(lines->snapshot().empty());
    CHECK(Logger::level() == TraceLevel::Off);
}

TEST_CASE("Given a level set, When it is read, Then the Logger answers it [RQ-FMW-070]", "[framework][logger]")
{
    const LoggerScope scope;

    Logger::setLevel(TraceLevel::Info);

    CHECK(Logger::level() == TraceLevel::Info);
}

TEST_CASE("Given one domain disabled, When a line is logged under it and under another, Then only the other is written [RQ-FMW-073]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);
    Logger::setDomainEnabled(LogDomain::Midi, false);

    Logger::writeLine(LogDomain::Midi, TraceLevel::Error, "F.cpp", 1, "midi");
    Logger::writeLine(LogDomain::UiEvents, TraceLevel::Error, "F.cpp", 1, "ui");

    const auto received = lines->snapshot();
    REQUIRE(received.size() == 1);
    CHECK_THAT(received[0], Catch::Matchers::ContainsSubstring("ui"));
    CHECK_FALSE(Logger::isDomainEnabled(LogDomain::Midi));
    CHECK(Logger::isDomainEnabled(LogDomain::ControllerCalls));
    CHECK(Logger::isDomainEnabled(LogDomain::UiEvents));
}

TEST_CASE("Given a domain disabled and enabled again, When a line is logged under it, Then it is written [RQ-FMW-073]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);
    Logger::setDomainEnabled(LogDomain::ControllerCalls, false);
    Logger::setDomainEnabled(LogDomain::ControllerCalls, true);

    Logger::writeLine(LogDomain::ControllerCalls, TraceLevel::Error, "F.cpp", 1, "m");

    CHECK(lines->snapshot().size() == 1);
}

TEST_CASE("Given a path with directories of either separator, When a line is logged, Then only the file's base name is written [RQ-FMW-070]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);

    Logger::writeLine(LogDomain::Midi, TraceLevel::Info, "a/b/Slash.cpp", 1, "m");
    Logger::writeLine(LogDomain::Midi, TraceLevel::Info, "a\\b\\Backslash.cpp", 2, "m");
    Logger::writeLine(LogDomain::Midi, TraceLevel::Info, "Bare.cpp", 3, "m");

    const auto received = lines->snapshot();
    REQUIRE(received.size() == 3);
    CHECK_THAT(received[0], Catch::Matchers::ContainsSubstring(" Slash.cpp:1: "));
    CHECK_THAT(received[1], Catch::Matchers::ContainsSubstring(" Backslash.cpp:2: "));
    CHECK_THAT(received[2], Catch::Matchers::ContainsSubstring(" Bare.cpp:3: "));
}

TEST_CASE("Given no sink, When a line is logged, Then nothing happens and nothing throws [RQ-FMW-070]",
          "[framework][logger]")
{
    const LoggerScope scope;
    Logger::setLevel(TraceLevel::Verbose);

    CHECK_NOTHROW(Logger::writeLine(LogDomain::Midi, TraceLevel::Error, "F.cpp", 1, "m"));
}

TEST_CASE("Given a sink replaced by another, When a line is logged, Then only the new sink gets it [RQ-FMW-070, ADR-FMW-001 (DEC-FMW-001)]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto first = installRecordingSink();
    const auto second = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);

    Logger::writeLine(LogDomain::Midi, TraceLevel::Error, "F.cpp", 1, "m");

    CHECK(first->snapshot().empty());
    CHECK(second->snapshot().size() == 1);
}

TEST_CASE("Given a sink shut down, When a line is logged, Then it is not written [RQ-FMW-070]", "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);
    Logger::shutdown();

    Logger::writeLine(LogDomain::Midi, TraceLevel::Error, "F.cpp", 1, "m");

    CHECK(lines->snapshot().empty());
}

TEST_CASE("Given a log file path, When the Logger is configured with it and a line is logged, Then the file holds the line and the call answered true [ADR-FMW-001 (DEC-FMW-001)]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto path = std::filesystem::temp_directory_path() / "xs56k_framework_logger_test.log";
    std::filesystem::remove(path);
    Logger::setLevel(TraceLevel::Verbose);

    const bool opened = Logger::configure(path.string());
    Logger::writeLine(LogDomain::Midi, TraceLevel::Error, "File.cpp", SOME_LINE, "to the file");
    Logger::shutdown(); // releases the handle: on Windows an open file cannot be deleted

    CHECK(opened);
    std::ifstream stream(path);
    std::stringstream content;
    content << stream.rdbuf();
    CHECK_THAT(content.str(), Catch::Matchers::ContainsSubstring("[ERROR] [MIDI] File.cpp:42: to the file"));
    stream.close();
    std::filesystem::remove(path);
}

TEST_CASE("Given a log file path in a folder that does not exist, When the Logger is configured with it, Then the call answers false [ADR-FMW-001 (DEC-FMW-001)]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto path = std::filesystem::temp_directory_path() / "xs56k_no_such_folder" / "x.log";

    CHECK_FALSE(Logger::configure(path.string()));
}

TEST_CASE("Given several threads logging at once, When they have finished, Then every line arrived whole [RQ-NFR-008]",
          "[framework][logger]")
{
    const LoggerScope scope;
    const auto lines = installRecordingSink();
    Logger::setLevel(TraceLevel::Verbose);

    std::vector<std::thread> threads;
    for (int thread = 0; thread < THREAD_COUNT; ++thread)
    {
        threads.emplace_back([thread] {
            for (int count = 0; count < LINES_PER_THREAD; ++count)
                Logger::writeLine(LogDomain::Midi, TraceLevel::Info, "Thread.cpp", thread,
                                  "line " + std::to_string(count) + " end");
        });
    }
    for (auto& thread : threads)
        thread.join();

    const auto received = lines->snapshot();
    CHECK(received.size() == static_cast<std::size_t>(THREAD_COUNT * LINES_PER_THREAD));
    for (const auto& line : received)
    {
        INFO(line);
        REQUIRE(std::regex_search(line, std::regex(R"( \[INFO\] \[MIDI\] Thread\.cpp:\d: line \d+ end$)")));
    }
}
