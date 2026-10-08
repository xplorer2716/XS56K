#pragma once

// A simulated sampler loaded with programs, as a real one is before the MCP server starts: raw §0A and §08 frames from a
// host of its own, checksums off, the program at `current` selected at the end. Shared by the tests of the gateway and of
// the tools and by the simulated server (`xs56k_mcp_server_simulated`). [TASK-MCP-004, TASK-MCP-007, RQ-MCP-012,
// ADR-MCP-001 (DEC-MCP-009)]
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "HostProbe.hpp"
#include "TestBytes.hpp"
#include "akm/Command.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"

namespace mcp::test
{
    struct SeededProgram
    {
        std::string name;
        int keygroups = 1;
        std::vector<int> cutoffs{};  ///< the filter cutoff of each keygroup, when it is to start with one
    };

    // The sampler keeps its programs in alphabetical order, whatever the order they are created in (observed on a real
    // S5000, OBSERVATIONS-RQ-MCP-012-real-sampler.md): `current` is the position in the order of `programs`, as given, and
    // is selected by name. [TASK-MCP-012]
    inline void seedSimulatedPrograms(akm::harness::SimulatedMidiBackend& backend, const std::vector<SeededProgram>& programs,
                                      std::size_t current)
    {
        constexpr std::uint8_t FIRST_USER_REF = 1;
        constexpr std::uint8_t LAST_USER_REF = 100;

        akm::test::HostProbe host(backend, backend.inputName(), backend.outputName());
        std::uint8_t userRef = FIRST_USER_REF;
        const auto send = [&](const akm::CommandRequest& request) {
            const akm::EncodeResult frame =
                akm::encodeCommand(0, akm::test::Bytes{userRef}, request.command, akm::ChecksumMode::Off);
            host.send(frame.bytes);
            userRef = userRef >= LAST_USER_REF ? FIRST_USER_REF : static_cast<std::uint8_t>(userRef + 1);
        };
        for (const SeededProgram& program : programs)
        {
            send(akm::makeStringRequest(akm::ItemId::ProgramCreate, program.name));
            if (program.keygroups > 1)
                send(akm::makeRequest(akm::ItemId::ProgramAddKeygroups, {program.keygroups - 1}));
            for (std::size_t i = 0; i < program.cutoffs.size(); ++i)
            {
                send(akm::makeRequest(akm::ItemId::KeygroupSelect, {static_cast<std::int64_t>(i) + 1}));
                send(akm::makeRequest(akm::ItemId::KeygroupSetFilterCutoff, {program.cutoffs[i]}));
            }
        }
        send(akm::makeStringRequest(akm::ItemId::ProgramSelectByName, programs[current].name));
    }

    /// PAD (1 keygroup), BASS (3, cutoffs 30, 60, 90) and LEAD (2, cutoffs 50, 70); BASS is current.
    inline void seedThreePrograms(akm::harness::SimulatedMidiBackend& backend)
    {
        constexpr std::size_t BASS = 1;
        seedSimulatedPrograms(backend, {{"PAD", 1, {10}}, {"BASS", 3, {30, 60, 90}}, {"LEAD", 2, {50, 70}}}, BASS);
    }
}
