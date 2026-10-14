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

// The connection of the editor application and its MIDI setup, over the gateway and the simulated sampler: the ports listed, the DeviceID
// checked, the setup kept between runs, connecting and why it failed, the state always known, the ports changed, the session closed before
// the ports are released, and a lost port. [TASK-APP-012, RQ-APP-015 to RQ-APP-022, ADR-APP-001 (DEC-APP-008, DEC-APP-009)]
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "AppTestSupport.hpp"

using app::ConnectionState;
using app::MidiSetup;
using app::test::CoreRig;
using app::test::setupA;

namespace
{
    bool contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }

    /// Connects on the first pair of ports and waits for the answer.
    void connectA(CoreRig& rig)
    {
        rig.controller.connect(setupA());
        REQUIRE(rig.settle());
        REQUIRE(rig.model.connection().state == ConnectionState::Connected);
    }

    /// The number of "send <port>" events from `from` on.
    std::size_t sendsFrom(const std::vector<std::string>& events, std::size_t from, const std::string& port)
    {
        std::size_t count = 0;
        for (std::size_t i = from; i < events.size(); ++i)
        {
            if (events[i] == "send " + port)
                ++count;
        }
        return count;
    }
}

TEST_CASE("Given a computer with the inputs IN A and IN B and the outputs OUT A and OUT B, When the MIDI setup lists the ports, Then both "
          "lists are given; When the DeviceID 40 is given, Then it is refused and the range 0 to 31 is said [RQ-APP-015]",
          "[app][connection]")
{
    CoreRig rig;

    const app::MidiPortLists ports = app::listMidiPorts(rig.recording);

    CHECK(ports.inputs == std::vector<std::string>{"IN A", "IN B"});
    CHECK(ports.outputs == std::vector<std::string>{"OUT A", "OUT B"});
    const auto refused = app::deviceIdProblem(40);
    REQUIRE(refused.has_value());
    CHECK(contains(*refused, "0 to 31"));
    CHECK_FALSE(app::deviceIdProblem(0).has_value());
    CHECK_FALSE(app::deviceIdProblem(31).has_value());
    CHECK(app::deviceIdProblem(-1).has_value());
    CHECK(rig.recording.events().empty());  // listing the ports opens none and sends nothing
}

TEST_CASE("Given IN B, OUT B and DeviceID 3 validated, When the application starts again with the same settings, Then the MIDI setup gives "
          "IN B, OUT B and 3 [RQ-APP-016]",
          "[app][connection]")
{
    CoreRig rig;
    CHECK_FALSE(app::loadMidiSetup(rig.settings).has_value());

    rig.controller.applyMidiSetup(MidiSetup{"IN B", "OUT B", 3});

    const auto stored = app::loadMidiSetup(rig.settings);
    REQUIRE(stored.has_value());
    CHECK(stored->inputPort == "IN B");
    CHECK(stored->outputPort == "OUT B");
    CHECK(stored->deviceId == 3);
    CHECK(rig.model.connection().state == ConnectionState::Disconnected);  // validating while disconnected does not connect
}

TEST_CASE("Given the simulated sampler at DeviceID 0 and the setup at 0, When the person connects, Then the state goes from connecting to "
          "connected with the model, the name, the OS version and the DeviceID [RQ-APP-017, RQ-APP-019]",
          "[app][connection]")
{
    CoreRig rig;

    rig.controller.connect(setupA());
    CHECK(rig.model.connection().state == ConnectionState::Connecting);
    REQUIRE(rig.settle());

    const app::ConnectionStatus& status = rig.model.connection();
    REQUIRE(status.state == ConnectionState::Connected);
    REQUIRE(status.sampler.has_value());
    CHECK(status.sampler->model == "AKAI S5000");
    CHECK_FALSE(status.sampler->name.empty());
    CHECK(status.sampler->osVersion == "2.10");
    CHECK(status.sampler->deviceId == 0);
    CHECK_FALSE(rig.controller.canConnect());
}

TEST_CASE("Given the setup at DeviceID 2 and the simulated sampler answering at DeviceID 1 only, When the person connects, Then the state is "
          "failed and says no sampler answered at 2 and one answered at 1 [RQ-APP-018]",
          "[app][connection]")
{
    CoreRig rig(app::test::TEST_COMMAND_TIMEOUT, akm::harness::SamplerConfig{1, {2, 10}});

    rig.controller.connect(setupA(2));
    REQUIRE(rig.settle());

    const app::ConnectionStatus& status = rig.model.connection();
    CHECK(status.state == ConnectionState::Failed);
    CHECK(contains(status.reason, "No sampler answered at DeviceID 2"));
    CHECK(contains(status.reason, "DeviceID 1"));
    CHECK(rig.controller.canConnect());
}

TEST_CASE("Given a stored input port absent from the computer, When the person connects, Then the state is failed and names the port and "
          "the ports available [RQ-APP-018]",
          "[app][connection]")
{
    CoreRig rig;

    rig.controller.connect(MidiSetup{"IN GONE", "OUT A", 0});
    REQUIRE(rig.settle());

    const app::ConnectionStatus& status = rig.model.connection();
    CHECK(status.state == ConnectionState::Failed);
    CHECK(contains(status.reason, "IN GONE"));
    CHECK(contains(status.reason, "IN A"));
}

TEST_CASE("Given an invalid DeviceID, When the person connects, Then nothing is opened and the state says why [RQ-APP-015, RQ-APP-018]",
          "[app][connection]")
{
    CoreRig rig;

    rig.controller.connect(setupA(40));
    REQUIRE(rig.settle());

    CHECK(rig.model.connection().state == ConnectionState::Failed);
    CHECK(contains(rig.model.connection().reason, "0 to 31"));
    CHECK(rig.recording.events().empty());
}

TEST_CASE("Given the application disconnected, When an action needs the sampler, Then it is refused and nothing is queued; When it is "
          "connected, Then it is queued [RQ-APP-019]",
          "[app][connection]")
{
    CoreRig rig;
    bool ran = false;
    const auto readStatus = [](mcp::SamplerGateway& gateway) { return gateway.status(); };
    const auto record = [&ran](const mcp::SamplerStatus&) { ran = true; };

    CHECK(rig.model.connection().state == ConnectionState::Disconnected);
    CHECK_FALSE(rig.controller.runOnSampler<mcp::SamplerStatus>("Reading the status", readStatus, record));
    CHECK_FALSE(rig.model.busy().has_value());
    REQUIRE(rig.settle());
    CHECK_FALSE(ran);

    connectA(rig);
    CHECK(rig.controller.runOnSampler<mcp::SamplerStatus>("Reading the status", readStatus, record));
    REQUIRE(rig.settle());
    CHECK(ran);
}

TEST_CASE("Given a session open on IN A and OUT A, When the person validates IN B and OUT B, Then the session is closed and IN A and OUT A "
          "are released before IN B and OUT B are opened, and it connects again [RQ-APP-020]",
          "[app][connection]")
{
    CoreRig rig;
    connectA(rig);

    rig.controller.applyMidiSetup(MidiSetup{"IN B", "OUT B", 0});
    REQUIRE(rig.settle());

    CHECK(rig.model.connection().state == ConnectionState::Connected);
    const auto releasedIn = rig.recording.find("release IN A");
    const auto releasedOut = rig.recording.find("release OUT A");
    const auto openedIn = rig.recording.find("open IN B");
    const auto openedOut = rig.recording.find("open OUT B");
    REQUIRE(releasedIn.has_value());
    REQUIRE(releasedOut.has_value());
    REQUIRE(openedIn.has_value());
    REQUIRE(openedOut.has_value());
    CHECK(*releasedIn < *openedIn);
    CHECK(*releasedOut < *openedIn);
    CHECK(*releasedIn < *openedOut);
    CHECK(*releasedOut < *openedOut);
    CHECK(app::loadMidiSetup(rig.settings)->inputPort == "IN B");
}

TEST_CASE("Given a session open, When the person disconnects, Then the session closing is sent before the ports are released, and the state "
          "is disconnected [RQ-APP-021]",
          "[app][connection]")
{
    CoreRig rig;
    connectA(rig);
    const std::size_t before = rig.recording.events().size();

    rig.controller.disconnect();
    REQUIRE(rig.settle());

    CHECK(rig.model.connection().state == ConnectionState::Disconnected);
    CHECK(rig.controller.canConnect());
    const auto events = rig.recording.events();
    const auto releasedOut = rig.recording.find("release OUT A", before);
    REQUIRE(releasedOut.has_value());
    CHECK(sendsFrom(events, before, "OUT A") > 0);
    CHECK(sendsFrom(events, *releasedOut, "OUT A") == 0);
}

TEST_CASE("Given a session open, When the person quits, Then the session closing is sent before the ports are released, and the application "
          "is told it may quit only then [RQ-APP-021]",
          "[app][connection]")
{
    CoreRig rig;
    connectA(rig);
    const std::size_t before = rig.recording.events().size();
    std::optional<std::size_t> eventsWhenQuitting;

    rig.controller.quit([&rig, &eventsWhenQuitting] { eventsWhenQuitting = rig.recording.events().size(); });
    CHECK_FALSE(eventsWhenQuitting.has_value());  // the UI thread is not blocked: the quit comes later
    REQUIRE(rig.settle());

    REQUIRE(eventsWhenQuitting.has_value());
    const auto releasedIn = rig.recording.find("release IN A", before);
    const auto releasedOut = rig.recording.find("release OUT A", before);
    REQUIRE(releasedIn.has_value());
    REQUIRE(releasedOut.has_value());
    CHECK(*releasedIn < *eventsWhenQuitting);
    CHECK(*releasedOut < *eventsWhenQuitting);
    CHECK(sendsFrom(rig.recording.events(), before, "OUT A") > 0);
}

TEST_CASE("Given the application disconnected, When the person quits, Then the application is told at once that it may quit [RQ-APP-021]",
          "[app][connection]")
{
    CoreRig rig;
    bool mayQuit = false;

    rig.controller.quit([&mayQuit] { mayQuit = true; });
    REQUIRE(rig.settle());

    CHECK(mayQuit);
    CHECK(rig.recording.events().empty());
}

TEST_CASE("Given a session open, When the input port reports an error, Then the state is failed with the port's name, actions are refused "
          "and the person can connect again [RQ-APP-022]",
          "[app][connection]")
{
    CoreRig rig;
    connectA(rig);

    REQUIRE(rig.recording.raiseInputError("device removed"));
    REQUIRE(rig.settle());

    const app::ConnectionStatus& status = rig.model.connection();
    CHECK(status.state == ConnectionState::Failed);
    CHECK(contains(status.reason, "IN A"));
    CHECK(contains(status.reason, "device removed"));
    CHECK(rig.recording.find("release IN A").has_value());
    CHECK_FALSE(rig.controller.runOnSampler<mcp::SamplerStatus>(
        "Reading the status", [](mcp::SamplerGateway& gateway) { return gateway.status(); }, [](const mcp::SamplerStatus&) {}));
    CHECK(rig.controller.canConnect());

    rig.controller.connect(setupA());
    REQUIRE(rig.settle());
    CHECK(rig.model.connection().state == ConnectionState::Connected);
}
