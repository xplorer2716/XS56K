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
#include "StandardParameters.hpp"

#include <utility>

namespace mcp
{
    namespace
    {
        constexpr const char* GROUP_FILTER = "filter";
        constexpr const char* GROUP_AMP_ENVELOPE = "amplitude envelope";
        constexpr const char* GROUP_FILTER_ENVELOPE = "filter envelope";
        constexpr const char* GROUP_LFO_1 = "lfo 1";
        constexpr const char* GROUP_LFO_2 = "lfo 2";

        // The sampler's own ranges (spec Tables 11 and 13), as the item catalogue gives them.
        constexpr std::int64_t LEVEL_MAX = 100;
        constexpr std::int64_t FILTER_RESONANCE_MAX = 15;
        constexpr std::int64_t FILTER_KEYBOARD_TRACK_MAX = 36;
        constexpr std::int64_t FILTER_ATTENUATION_STEP_DB = 6;
        constexpr std::int64_t FILTER_ATTENUATION_MAX_DB = 30;
        constexpr const char* UNIT_DB = "dB";
        constexpr std::int64_t FIRST_LFO = 1;
        constexpr std::int64_t SECOND_LFO = 2;

        // The 26 filter types in code order, as the sampler's screen names them (user manual, EDIT PROGRAM, FILTER
        // MODE; spec Table 11, item &20).
        const std::vector<std::string>& filterTypeLabels()
        {
            static const std::vector<std::string> labels{
                "2-POLE LP",  "4-POLE LP",  "2-POLE LP+", "2-POLE BP", "4-POLE BP",  "2-POLE BP+", "1-POLE HP",
                "2-POLE HP",  "1-POLE HP+", "LO<>HI",     "LO<>BAND",  "BAND<>HI",   "NOTCH 1",    "NOTCH 2",
                "NOTCH 3",    "WIDE NOTCH", "BI-NOTCH",   "PEAK 1",    "PEAK 2",     "PEAK 3",     "WIDE PEAK",
                "BI-PEAK",    "PHASER 1",   "PHASER 2",   "BI-PHASE",  "VOWELISER"};
            return labels;
        }

        // The nine LFO waveforms in code order (spec Table 13, item &53; user manual, EDIT PROGRAM, LFO).
        const std::vector<std::string>& lfoWaveformLabels()
        {
            static const std::vector<std::string> labels{"SINE", "TRIANGLE", "SQUARE",   "SQUARE+", "SQUARE-",
                                                         "SAW BI", "SAW UP",  "SAW DOWN", "RANDOM"};
            return labels;
        }

        const std::vector<std::string>& switchLabels()
        {
            static const std::vector<std::string> labels{"off", "on"};
            return labels;
        }

        ParameterDefinition number(std::string name, std::vector<std::string> aliases, const char* group, std::string description,
                                   ParameterScope scope, akm::ItemId setItem, akm::ItemId getItem, std::int64_t max,
                                   std::vector<std::int64_t> leadingArguments = {})
        {
            ParameterDefinition row;
            row.name = std::move(name);
            row.aliases = std::move(aliases);
            row.group = group;
            row.description = std::move(description);
            row.scope = scope;
            row.kind = ParameterKind::Number;
            row.setItem = setItem;
            row.getItem = getItem;
            row.leadingArguments = std::move(leadingArguments);
            row.min = 0;
            row.max = max;
            return row;
        }

        ParameterDefinition signedNumber(std::string name, std::vector<std::string> aliases, const char* group,
                                         std::string description, ParameterScope scope, akm::ItemId setItem,
                                         akm::ItemId getItem, std::int64_t magnitudeMax)
        {
            ParameterDefinition row = number(std::move(name), std::move(aliases), group, std::move(description), scope,
                                             setItem, getItem, magnitudeMax);
            row.kind = ParameterKind::Signed;
            row.min = -magnitudeMax;
            return row;
        }

        ParameterDefinition choice(std::string name, std::vector<std::string> aliases, const char* group, std::string description,
                                   ParameterScope scope, akm::ItemId setItem, akm::ItemId getItem,
                                   const std::vector<std::string>& labels, std::vector<std::int64_t> leadingArguments = {})
        {
            ParameterDefinition row = number(std::move(name), std::move(aliases), group, std::move(description), scope,
                                             setItem, getItem, static_cast<std::int64_t>(labels.size()) - 1,
                                             std::move(leadingArguments));
            row.kind = ParameterKind::Choice;
            row.labels = labels;
            return row;
        }

        ParameterDefinition onOff(std::string name, std::vector<std::string> aliases, const char* group, std::string description,
                                  ParameterScope scope, akm::ItemId setItem, akm::ItemId getItem,
                                  std::vector<std::int64_t> leadingArguments)
        {
            ParameterDefinition row = choice(std::move(name), std::move(aliases), group, std::move(description), scope,
                                             setItem, getItem, switchLabels(), std::move(leadingArguments));
            row.kind = ParameterKind::Switch;
            return row;
        }

        std::string lfoName(std::int64_t lfo, const char* what)
        {
            return std::string("lfo ") + std::to_string(lfo) + " " + what;
        }

        std::string lfoAlias(std::int64_t lfo, const char* what)
        {
            return std::string("lfo") + std::to_string(lfo) + " " + what;
        }

        const char* lfoGroup(std::int64_t lfo)
        {
            return lfo == FIRST_LFO ? GROUP_LFO_1 : GROUP_LFO_2;
        }

        // One envelope stage of a keygroup: the same four rows (attack, decay, sustain, release) for the amplitude and
        // for the filter envelope.
        void addEnvelopeStages(std::vector<ParameterDefinition>& rows, const char* group, const std::string& prefix,
                               const std::vector<std::string>& prefixAliases, akm::ItemId setAttack, akm::ItemId getAttack,
                               akm::ItemId setDecay, akm::ItemId getDecay, akm::ItemId setSustain, akm::ItemId getSustain,
                               akm::ItemId setRelease, akm::ItemId getRelease)
        {
            const auto aliasesFor = [&prefixAliases](const char* stage) {
                std::vector<std::string> aliases;
                for (const std::string& alias : prefixAliases)
                    aliases.push_back(alias + " " + stage);
                return aliases;
            };
            rows.push_back(number(prefix + " attack", aliasesFor("attack"), group,
                                  "Attack time of the " + prefix + ": how long it takes to rise to its peak once a key is pressed.",
                                  ParameterScope::Keygroup, setAttack, getAttack, LEVEL_MAX));
            rows.push_back(number(prefix + " decay", aliasesFor("decay"), group,
                                  "Decay time of the " + prefix + ": how long it takes to fall from its peak to the sustain level.",
                                  ParameterScope::Keygroup, setDecay, getDecay, LEVEL_MAX));
            rows.push_back(number(prefix + " sustain", aliasesFor("sustain"), group,
                                  "Sustain level of the " + prefix + ": the level it holds while the key stays down.",
                                  ParameterScope::Keygroup, setSustain, getSustain, LEVEL_MAX));
            rows.push_back(number(prefix + " release", aliasesFor("release"), group,
                                  "Release time of the " + prefix + ": how long it takes to fall to nothing once the key is released.",
                                  ParameterScope::Keygroup, setRelease, getRelease, LEVEL_MAX));
        }
    }

    std::vector<GroupDefinition> standardGroups()
    {
        return {
            {GROUP_FILTER, {}, "The keygroup's filter: its type, cutoff, resonance, keyboard tracking and attenuation."},
            {GROUP_AMP_ENVELOPE,
             {"amp envelope", "amp env", "amplitude env", "amplifier envelope"},
             "The keygroup's amplitude envelope: how the volume of a note rises, falls and dies away."},
            {GROUP_FILTER_ENVELOPE,
             {"filter env"},
             "The keygroup's filter envelope: how the filter's cutoff moves during a note, and how deeply."},
            {GROUP_LFO_1, {"lfo"}, "The program's first low-frequency oscillator."},
            {GROUP_LFO_2, {"lfo"}, "The program's second low-frequency oscillator."},
        };
    }

    std::vector<ParameterDefinition> standardParameters()
    {
        std::vector<ParameterDefinition> rows;

        // Filter (§08, Table 11).
        rows.push_back(choice("filter type", {"filter mode", "type of filter"}, GROUP_FILTER,
                              "The filter's type, one of the sampler's 26 resonant filter types (low-pass, band-pass, "
                              "high-pass, notch, peak, phaser and others).",
                              ParameterScope::Keygroup, akm::ItemId::KeygroupSetFilterMode, akm::ItemId::KeygroupGetFilterMode,
                              filterTypeLabels()));
        rows.push_back(number("filter cutoff", {"cutoff", "filter frequency", "cutoff frequency", "filter cutoff frequency"},
                              GROUP_FILTER, "The filter's cutoff frequency.", ParameterScope::Keygroup,
                              akm::ItemId::KeygroupSetFilterCutoff, akm::ItemId::KeygroupGetFilterCutoff, LEVEL_MAX));
        rows.push_back(number("filter resonance", {"resonance"}, GROUP_FILTER,
                              "The filter's resonance: the emphasis around the cutoff frequency.", ParameterScope::Keygroup,
                              akm::ItemId::KeygroupSetFilterResonance, akm::ItemId::KeygroupGetFilterResonance,
                              FILTER_RESONANCE_MAX));
        rows.push_back(signedNumber("filter keyboard tracking", {"filter keyboard track", "filter key track", "keyboard tracking"},
                                    GROUP_FILTER, "How the filter's cutoff follows the keyboard, in the sampler's units.",
                                    ParameterScope::Keygroup, akm::ItemId::KeygroupSetFilterKeyboardTrack,
                                    akm::ItemId::KeygroupGetFilterKeyboardTrack, FILTER_KEYBOARD_TRACK_MAX));
        ParameterDefinition attenuation =
            number("filter attenuation", {"attenuation"}, GROUP_FILTER,
                   "The attenuation of the signal at the filter's input, in dB, in steps of 6 dB.", ParameterScope::Keygroup,
                   akm::ItemId::KeygroupSetFilterAttenuation, akm::ItemId::KeygroupGetFilterAttenuation,
                   FILTER_ATTENUATION_MAX_DB);
        attenuation.step = FILTER_ATTENUATION_STEP_DB;
        attenuation.unit = UNIT_DB;
        rows.push_back(std::move(attenuation));

        // Amplitude envelope and filter envelope (§08, Table 11).
        addEnvelopeStages(rows, GROUP_AMP_ENVELOPE, "amplitude envelope", {"amp envelope", "amp env", "amp"},
                          akm::ItemId::KeygroupSetAmpEnvAttack, akm::ItemId::KeygroupGetAmpEnvAttack,
                          akm::ItemId::KeygroupSetAmpEnvDecay, akm::ItemId::KeygroupGetAmpEnvDecay,
                          akm::ItemId::KeygroupSetAmpEnvSustain, akm::ItemId::KeygroupGetAmpEnvSustain,
                          akm::ItemId::KeygroupSetAmpEnvRelease, akm::ItemId::KeygroupGetAmpEnvRelease);
        addEnvelopeStages(rows, GROUP_FILTER_ENVELOPE, "filter envelope", {"filter env"},
                          akm::ItemId::KeygroupSetFilterEnvAttack, akm::ItemId::KeygroupGetFilterEnvAttack,
                          akm::ItemId::KeygroupSetFilterEnvDecay, akm::ItemId::KeygroupGetFilterEnvDecay,
                          akm::ItemId::KeygroupSetFilterEnvSustain, akm::ItemId::KeygroupGetFilterEnvSustain,
                          akm::ItemId::KeygroupSetFilterEnvRelease, akm::ItemId::KeygroupGetFilterEnvRelease);
        rows.push_back(signedNumber("filter envelope depth", {"filter env depth", "filter envelope amount"},
                                    GROUP_FILTER_ENVELOPE,
                                    "How far the filter envelope moves the cutoff; negative values move it the other way.",
                                    ParameterScope::Keygroup, akm::ItemId::KeygroupSetFilterEnvDepth,
                                    akm::ItemId::KeygroupGetFilterEnvDepth, LEVEL_MAX));

        // The two LFOs (§0A, Table 13): the same five rows for each, the first LFO syncing where the second re-triggers.
        for (const std::int64_t lfo : {FIRST_LFO, SECOND_LFO})
        {
            const char* group = lfoGroup(lfo);
            const std::string which = std::to_string(lfo);
            rows.push_back(number(lfoName(lfo, "rate"), {lfoAlias(lfo, "speed")}, group,
                                  "The speed of LFO " + which + ".", ParameterScope::Program, akm::ItemId::ProgramSetLfoRate,
                                  akm::ItemId::ProgramGetLfoRate, LEVEL_MAX, {lfo}));
            rows.push_back(number(lfoName(lfo, "delay"), {}, group,
                                  "The time LFO " + which + "'s modulation takes to fade in.", ParameterScope::Program,
                                  akm::ItemId::ProgramSetLfoDelay, akm::ItemId::ProgramGetLfoDelay, LEVEL_MAX, {lfo}));
            rows.push_back(number(lfoName(lfo, "depth"), {}, group,
                                  "The initial depth of LFO " + which + "'s modulation.", ParameterScope::Program,
                                  akm::ItemId::ProgramSetLfoDepth, akm::ItemId::ProgramGetLfoDepth, LEVEL_MAX, {lfo}));
            rows.push_back(choice(lfoName(lfo, "waveform"), {lfoAlias(lfo, "shape")}, group,
                                  "The waveform of LFO " + which + ", one of nine.", ParameterScope::Program,
                                  akm::ItemId::ProgramSetLfoWaveform, akm::ItemId::ProgramGetLfoWaveform, lfoWaveformLabels(),
                                  {lfo}));
        }
        rows.push_back(onOff(lfoName(FIRST_LFO, "sync"), {}, GROUP_LFO_1,
                             "Whether LFO 1 is synchronised (the sampler's LFO sync setting, LFO 1 only).",
                             ParameterScope::Program, akm::ItemId::ProgramSetLfoSync, akm::ItemId::ProgramGetLfoSync,
                             {FIRST_LFO}));
        rows.push_back(onOff(lfoName(SECOND_LFO, "retrigger"), {}, GROUP_LFO_2,
                             "Whether LFO 2 starts its cycle again on each new note (re-trigger, LFO 2 only).",
                             ParameterScope::Program, akm::ItemId::ProgramSetLfoRetrigger,
                             akm::ItemId::ProgramGetLfoRetrigger, {SECOND_LFO}));
        return rows;
    }
}
