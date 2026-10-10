/*
 * XS56K - Editor for AKAI S5000/S6000 samplers
 * Copyright (C) 2026 xplorer2716
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// The effects unit of the sampler gateway: the Multi FX section (12), whose items act on the current multi. Verified on the simulated sampler only:
// the owner has no effects board. [TASK-MCP-052, RQ-MCP-053, ADR-MCP-005 (DEC-MCP-033)]
#include <functional>
#include <utility>

#include "GatewayDetail.hpp"
#include "akm/MultiFxPrimitives.hpp"
#include "akm/SamplerError.hpp"
#include "mcp/SamplerGateway.hpp"

namespace mcp
{
    using detail::await;
    using detail::explain;
    using detail::numberText;

    namespace
    {
        constexpr const char* SESSION_TIMED_OUT = "The sampler session did not complete the command in time.";
        // The effects belong to the current multi: that is what a sampler with none selected is reminded of.
        constexpr const char* CURRENT_MULTI = "multi";
    }

    Outcome<FxLayoutInfo> SamplerGateway::readFxLayout()
    {
        if (const auto problem = connect())
            return Outcome<FxLayoutInfo>::failure(*problem);
        const auto card = await<akm::FxCardResult>(waitFor(1), [&](std::function<void(const akm::FxCardResult&)> done) {
            akm::getFxCard(session(), std::move(done));
        });
        if (!card)
            return Outcome<FxLayoutInfo>::failure(SESSION_TIMED_OUT);
        if (!card->card)
            return Outcome<FxLayoutInfo>::failure(akm::succeeded(card->outcome) ? std::string("The sampler reported an effects card this server does not know.")
                                                                                 : explain(card->outcome, "reading the effects card", _config, false));
        FxLayoutInfo layout;
        if (*card->card == akm::FxCard::None)
            return Outcome<FxLayoutInfo>::success(std::move(layout));
        layout.card = FxCardKind::Eb20;

        const auto channels = await<akm::FxCountResult>(waitFor(1), [&](std::function<void(const akm::FxCountResult&)> done) {
            akm::getFxChannelCount(session(), std::move(done));
        });
        if (!channels)
            return Outcome<FxLayoutInfo>::failure(SESSION_TIMED_OUT);
        if (!channels->count)
            return Outcome<FxLayoutInfo>::failure(explain(channels->outcome, "counting the effects channels", _config, false));
        for (int channel = 0; channel < *channels->count; ++channel)
        {
            const auto modules = await<akm::FxCountResult>(waitFor(1), [&](std::function<void(const akm::FxCountResult&)> done) {
                akm::getFxModuleCount(session(), channel, std::move(done));
            });
            if (!modules)
                return Outcome<FxLayoutInfo>::failure(SESSION_TIMED_OUT);
            if (!modules->count)
                return Outcome<FxLayoutInfo>::failure(
                    explain(modules->outcome, "counting the modules of the effects channel " + numberText(channel), _config, false));
            layout.moduleCounts.push_back(*modules->count);
        }
        return Outcome<FxLayoutInfo>::success(std::move(layout));
    }

    Outcome<FxModuleState> SamplerGateway::readFxModule(int channel, int module)
    {
        const std::string where = "the module " + numberText(module) + " of the effects channel " + numberText(channel);
        const auto type = await<akm::FxModuleTypeResult>(waitFor(1), [&](std::function<void(const akm::FxModuleTypeResult&)> done) {
            akm::getFxModuleType(session(), channel, module, std::move(done));
        });
        if (!type)
            return Outcome<FxModuleState>::failure(SESSION_TIMED_OUT);
        if (!type->type)
            return Outcome<FxModuleState>::failure(explain(type->outcome, "reading the type of " + where, _config, true, CURRENT_MULTI));
        const auto enabled = await<akm::FxEnabledResult>(waitFor(1), [&](std::function<void(const akm::FxEnabledResult&)> done) {
            akm::getFxModuleEnabled(session(), channel, module, std::move(done));
        });
        if (!enabled)
            return Outcome<FxModuleState>::failure(SESSION_TIMED_OUT);
        if (!enabled->enabled)
            return Outcome<FxModuleState>::failure(explain(enabled->outcome, "reading whether " + where + " is enabled", _config, true, CURRENT_MULTI));
        return Outcome<FxModuleState>::success(FxModuleState{static_cast<int>(*type->type), *enabled->enabled});
    }

    Outcome<FxBoardState> SamplerGateway::readFxBoard()
    {
        FxBoardState board;
        const auto layout = readFxLayout();
        if (!layout.ok())
            return Outcome<FxBoardState>::failure(layout.problem);
        board.layout = *layout.value;
        for (std::size_t channel = 0; channel < board.layout.moduleCounts.size(); ++channel)
        {
            FxChannelState state;
            const int index = static_cast<int>(channel);
            const auto muted = await<akm::FxMuteResult>(waitFor(1), [&](std::function<void(const akm::FxMuteResult&)> done) {
                akm::getFxChannelMute(session(), index, std::move(done));
            });
            if (!muted)
                return Outcome<FxBoardState>::failure(SESSION_TIMED_OUT);
            if (!muted->muted)
                return Outcome<FxBoardState>::failure(
                    explain(muted->outcome, "reading the mute of the effects channel " + numberText(index), _config, true, CURRENT_MULTI));
            state.muted = *muted->muted;
            for (int module = 0; module < board.layout.moduleCounts[channel]; ++module)
            {
                const auto read = readFxModule(index, module);
                if (!read.ok())
                    return Outcome<FxBoardState>::failure(read.problem);
                state.modules.push_back(*read.value);
            }
            board.channels.push_back(std::move(state));
        }
        return Outcome<FxBoardState>::success(std::move(board));
    }

    Outcome<int> SamplerGateway::readFxParameter(int channel, int module, int parameter)
    {
        const auto read = await<akm::FxParameterResult>(waitFor(1), [&](std::function<void(const akm::FxParameterResult&)> done) {
            akm::getFxParameter(session(), channel, module, parameter, std::move(done));
        });
        if (!read)
            return Outcome<int>::failure(SESSION_TIMED_OUT);
        if (!read->value)
            return Outcome<int>::failure(explain(read->outcome, "reading the parameter " + numberText(parameter) + " of the module " + numberText(module) +
                                                                    " of the effects channel " + numberText(channel),
                                                 _config, true, CURRENT_MULTI));
        return Outcome<int>::success(*read->value);
    }

    Outcome<bool> SamplerGateway::setFxChannelMute(int channel, bool muted)
    {
        if (const auto problem = connect())
            return Outcome<bool>::failure(*problem);
        const auto set = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::setFxChannelMute(session(), channel, muted, std::move(done));
        });
        if (!set)
            return Outcome<bool>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*set))
            return Outcome<bool>::failure(explain(*set, "muting the effects channel " + numberText(channel), _config, true, CURRENT_MULTI));
        const auto read = await<akm::FxMuteResult>(waitFor(1), [&](std::function<void(const akm::FxMuteResult&)> done) {
            akm::getFxChannelMute(session(), channel, std::move(done));
        });
        if (!read)
            return Outcome<bool>::failure(SESSION_TIMED_OUT);
        if (!read->muted)
            return Outcome<bool>::failure(
                explain(read->outcome, "reading the mute of the effects channel " + numberText(channel), _config, true, CURRENT_MULTI));
        return Outcome<bool>::success(*read->muted);
    }

    Outcome<FxModuleState> SamplerGateway::setFxModule(int channel, int module, std::optional<int> type, std::optional<bool> enabled)
    {
        if (const auto problem = connect())
            return Outcome<FxModuleState>::failure(*problem);
        const std::string where = "the module " + numberText(module) + " of the effects channel " + numberText(channel);
        if (type)
        {
            const auto set = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
                akm::setFxModuleType(session(), channel, module, static_cast<akm::FxModuleType>(*type), std::move(done));
            });
            if (!set)
                return Outcome<FxModuleState>::failure(SESSION_TIMED_OUT);
            if (!akm::succeeded(*set))
                return Outcome<FxModuleState>::failure(explain(*set, "setting the type of " + where, _config, true, CURRENT_MULTI));
        }
        if (enabled)
        {
            const auto set = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
                akm::setFxModuleEnabled(session(), channel, module, *enabled, std::move(done));
            });
            if (!set)
                return Outcome<FxModuleState>::failure(SESSION_TIMED_OUT);
            if (!akm::succeeded(*set))
                return Outcome<FxModuleState>::failure(explain(*set, "enabling or bypassing " + where, _config, true, CURRENT_MULTI));
        }
        return readFxModule(channel, module);
    }

    Outcome<int> SamplerGateway::setFxParameter(int channel, int module, int parameter, int value)
    {
        if (const auto problem = connect())
            return Outcome<int>::failure(*problem);
        const auto set = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::setFxParameter(session(), channel, module, parameter, value, std::move(done));
        });
        if (!set)
            return Outcome<int>::failure(SESSION_TIMED_OUT);
        if (!akm::succeeded(*set))
            return Outcome<int>::failure(explain(*set, "setting the parameter " + numberText(parameter) + " of the module " + numberText(module) +
                                                           " of the effects channel " + numberText(channel),
                                                 _config, true, CURRENT_MULTI));
        return readFxParameter(channel, module, parameter);
    }
}
