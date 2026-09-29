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
#include "akm/SamplePrimitives.hpp"

#include <cstdint>
#include <span>
#include <utility>

#include "akm/ItemRequest.hpp"
#include "akm/SamplerError.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
        // A zero-based index is split into two 7-bit data bytes, most significant first (spec pp. 8-9's
        // compound word, read back byte by byte since the catalogue lists them as two separate Byte
        // values, not a Word, matching the spec's own Data1/Data2 columns) — the same convention
        // ProgramPrimitives.cpp uses for &06/&12.
        constexpr std::int64_t DATA_BYTE_BASE = 128;

        void submitSampleRequest(Session& session, CommandRequest request, CommandCompletion completion)
        {
            session.submit(std::move(request), std::move(completion));
        }
    }

    void selectSampleByName(Session& session, std::string_view name, CommandCompletion completion)
    {
        submitSampleRequest(session, makeStringRequest(ItemId::SampleSelectByName, name), std::move(completion));
    }

    void selectSampleByIndex(Session& session, int index, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        submitSampleRequest(session, makeRequest(ItemId::SampleSelectByIndex, {msb, lsb}), std::move(completion));
    }

    void deleteCurrentSample(Session& session, CommandCompletion completion)
    {
        submitSampleRequest(session, makeRequest(ItemId::SampleDeleteCurrent, NO_VALUES), std::move(completion));
    }

    void renameCurrentSample(Session& session, std::string_view name, CommandCompletion completion)
    {
        submitSampleRequest(session, makeStringRequest(ItemId::SampleRenameCurrent, name), std::move(completion));
    }

    void startSampleAudition(Session& session, CommandCompletion completion)
    {
        submitSampleRequest(session, makeRequest(ItemId::SampleStartAudition, NO_VALUES), std::move(completion));
    }

    void stopSampleAudition(Session& session, CommandCompletion completion)
    {
        submitSampleRequest(session, makeRequest(ItemId::SampleStopAudition, NO_VALUES), std::move(completion));
    }

    void getCurrentSampleIndex(Session& session, SampleIndexCompletion completion)
    {
        session.submit(makeRequest(ItemId::SampleGetCurrentIndex, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           SampleIndexResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::SampleGetCurrentIndex, rep->data);
                               if (values && values->size() == 2)
                                   result.index = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentSampleName(Session& session, SampleNameCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::SampleGetCurrentName, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           SampleNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::SampleGetCurrentName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }
}
