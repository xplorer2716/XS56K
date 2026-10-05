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

        // The modulation sources of Table 15, in code order. Codes 12 to 14 repeat MODWHEEL, BEND and EXTERNAL in the
        // spec's table; what sets them apart from codes 1, 2 and 4 is not documented, so they are named 2 here and the
        // difference is left to be observed on the sampler.
        const std::vector<std::string>& modulationSourceLabels()
        {
            static const std::vector<std::string> labels{"NO SOURCE", "MODWHEEL", "BEND",       "AFTERTOUCH", "EXTERNAL",
                                                         "VELOCITY",  "KEYBOARD", "LFO1",       "LFO2",       "AMP ENV",
                                                         "FILT ENV",  "AUX ENV",  "MODWHEEL 2", "BEND 2",     "EXTERNAL 2"};
            return labels;
        }

        constexpr const char* MODULATION_SOURCE_NOTE =
            " A source of the sampler's modulation matrix (spec Table 15). Codes 12 to 14 repeat MODWHEEL, BEND and EXTERNAL; "
            "what sets them apart from the first three is not documented.";

        // Other ways to say a source: the manual's words for the ones the spec abbreviates.
        std::vector<std::pair<std::string, std::int64_t>> modulationSourceAliases()
        {
            return {{"none", 0},          {"off", 0},           {"pitch bend", 2},      {"pitchbend", 2},
                    {"external midi", 4}, {"amp envelope", 9},  {"amplitude envelope", 9}, {"filter envelope", 10},
                    {"filter env", 10},   {"aux envelope", 11}};
        }

        // The MIDI clock divisions of LFO 2 (spec Table 13, item &5F): codes 0 to 5 are 8, 6, 4, 3, 2 and 1 cycles per
        // beat, codes 6 to 68 are 2 to 64 beats per cycle.
        const std::vector<std::string>& clockDivisionLabels()
        {
            static const std::vector<std::string> labels = [] {
                constexpr std::int64_t FIRST_BEATS_PER_CYCLE_CODE = 6;
                constexpr std::int64_t LAST_CODE = 68;
                constexpr std::int64_t BEATS_PER_CYCLE_OFFSET = 4;  // code 6 is 2 beats per cycle
                std::vector<std::string> made;
                for (const std::int64_t cycles : {8, 6, 4, 3, 2, 1})
                    made.push_back(std::to_string(cycles) + (cycles == 1 ? " cycle per beat" : " cycles per beat"));
                for (std::int64_t code = FIRST_BEATS_PER_CYCLE_CODE; code <= LAST_CODE; ++code)
                    made.push_back(std::to_string(code - BEATS_PER_CYCLE_OFFSET) + " beats per cycle");
                return made;
            }();
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
                                         akm::ItemId getItem, std::int64_t magnitudeMax,
                                         std::vector<std::int64_t> leadingArguments = {})
        {
            ParameterDefinition row = number(std::move(name), std::move(aliases), group, std::move(description), scope,
                                             setItem, getItem, magnitudeMax, std::move(leadingArguments));
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

        ParameterDefinition sourceChoice(std::string name, std::vector<std::string> aliases, const char* group,
                                         std::string description, akm::ItemId setItem, akm::ItemId getItem,
                                         std::int64_t leadingArgument)
        {
            ParameterDefinition row = choice(std::move(name), std::move(aliases), group,
                                             std::move(description) + MODULATION_SOURCE_NOTE, ParameterScope::Program, setItem,
                                             getItem, modulationSourceLabels(), {leadingArgument});
            row.choiceAliases = modulationSourceAliases();
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

    namespace
    {
        // The four signed items every envelope of a keygroup has besides its stages: what the note-on velocity does to
        // the attack and to the release, what the note-off velocity does to the release, and the key scaling.
        void addEnvelopeExtras(std::vector<ParameterDefinition>& rows, const char* group, const std::string& prefix,
                               const std::vector<std::string>& prefixAliases, akm::ItemId setVelocityToAttack,
                               akm::ItemId getVelocityToAttack, akm::ItemId setOnVelocityToRelease,
                               akm::ItemId getOnVelocityToRelease, akm::ItemId setOffVelocityToRelease,
                               akm::ItemId getOffVelocityToRelease, akm::ItemId setKeyscale, akm::ItemId getKeyscale)
        {
            const auto aliasesFor = [&prefixAliases](const char* extra) {
                std::vector<std::string> aliases;
                for (const std::string& alias : prefixAliases)
                    aliases.push_back(alias + " " + extra);
                return aliases;
            };
            rows.push_back(signedNumber(prefix + " velocity to attack", aliasesFor("velocity to attack"), group,
                                        "How the note-on velocity changes the attack time of the " + prefix + ", as a signed amount.",
                                        ParameterScope::Keygroup, setVelocityToAttack, getVelocityToAttack, LEVEL_MAX));
            rows.push_back(signedNumber(prefix + " on-velocity to release", aliasesFor("on-velocity to release"), group,
                                        "How the note-on velocity changes the release time of the " + prefix + ", as a signed amount.",
                                        ParameterScope::Keygroup, setOnVelocityToRelease, getOnVelocityToRelease, LEVEL_MAX));
            rows.push_back(signedNumber(prefix + " off-velocity to release", aliasesFor("off-velocity to release"), group,
                                        "How the note-off velocity changes the release time of the " + prefix + ", as a signed amount.",
                                        ParameterScope::Keygroup, setOffVelocityToRelease, getOffVelocityToRelease, LEVEL_MAX));
            rows.push_back(signedNumber(prefix + " key scale", aliasesFor("key scale"), group,
                                        "How the note played changes the times of the " + prefix + ", as a signed amount.",
                                        ParameterScope::Keygroup, setKeyscale, getKeyscale, LEVEL_MAX));
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

        // Lot 2: the rest of the four groups. The filter's three modulation inputs: the source is a program value, the
        // amount a keygroup value.
        for (const std::int64_t input : {1, 2, 3})
        {
            const std::string which = std::to_string(input);
            rows.push_back(sourceChoice("filter modulation " + which + " source", {"filter mod " + which + " source"}, GROUP_FILTER,
                                        "The source routed to the filter's modulation input " + which + ".",
                                        akm::ItemId::ProgramSetFilterModInputSource, akm::ItemId::ProgramGetFilterModInputSource,
                                        input));
            rows.push_back(signedNumber("filter modulation " + which + " amount", {"filter mod " + which + " amount"}, GROUP_FILTER,
                                        "How much the source of the filter's modulation input " + which +
                                            " moves the cutoff, as a signed amount.",
                                        ParameterScope::Keygroup, akm::ItemId::KeygroupSetFilterModInputValue,
                                        akm::ItemId::KeygroupGetFilterModInputValue, LEVEL_MAX, {input}));
        }

        addEnvelopeExtras(rows, GROUP_AMP_ENVELOPE, "amplitude envelope", {"amp envelope", "amp env", "amp"},
                          akm::ItemId::KeygroupSetAmpEnvVelocityToAttack, akm::ItemId::KeygroupGetAmpEnvVelocityToAttack,
                          akm::ItemId::KeygroupSetAmpEnvOnVelocityToRelease, akm::ItemId::KeygroupGetAmpEnvOnVelocityToRelease,
                          akm::ItemId::KeygroupSetAmpEnvOffVelocityToRelease, akm::ItemId::KeygroupGetAmpEnvOffVelocityToRelease,
                          akm::ItemId::KeygroupSetAmpEnvKeyscale, akm::ItemId::KeygroupGetAmpEnvKeyscale);
        addEnvelopeExtras(rows, GROUP_FILTER_ENVELOPE, "filter envelope", {"filter env"},
                          akm::ItemId::KeygroupSetFilterEnvVelocityToAttack, akm::ItemId::KeygroupGetFilterEnvVelocityToAttack,
                          akm::ItemId::KeygroupSetFilterEnvOnVelocityToRelease, akm::ItemId::KeygroupGetFilterEnvOnVelocityToRelease,
                          akm::ItemId::KeygroupSetFilterEnvOffVelocityToRelease,
                          akm::ItemId::KeygroupGetFilterEnvOffVelocityToRelease, akm::ItemId::KeygroupSetFilterEnvKeyscale,
                          akm::ItemId::KeygroupGetFilterEnvKeyscale);

        // Each LFO's rate, delay and depth can be moved by a source, by an amount of it.
        struct LfoModulation
        {
            const char* what;
            akm::ItemId setSource;
            akm::ItemId getSource;
            akm::ItemId setValue;
            akm::ItemId getValue;
        };
        const LfoModulation lfoModulations[]{
            {"rate", akm::ItemId::ProgramSetLfoRateModSource, akm::ItemId::ProgramGetLfoRateModSource,
             akm::ItemId::ProgramSetLfoRateModValue, akm::ItemId::ProgramGetLfoRateModValue},
            {"delay", akm::ItemId::ProgramSetLfoDelayModSource, akm::ItemId::ProgramGetLfoDelayModSource,
             akm::ItemId::ProgramSetLfoDelayModValue, akm::ItemId::ProgramGetLfoDelayModValue},
            {"depth", akm::ItemId::ProgramSetLfoDepthModSource, akm::ItemId::ProgramGetLfoDepthModSource,
             akm::ItemId::ProgramSetLfoDepthModValue, akm::ItemId::ProgramGetLfoDepthModValue}};
        for (const std::int64_t lfo : {FIRST_LFO, SECOND_LFO})
        {
            for (const LfoModulation& modulation : lfoModulations)
            {
                const std::string prefix = lfoName(lfo, modulation.what) + " modulation ";
                rows.push_back(sourceChoice(prefix + "source", {}, lfoGroup(lfo),
                                            std::string("The source that moves the ") + modulation.what + " of LFO " +
                                                std::to_string(lfo) + ".",
                                            modulation.setSource, modulation.getSource, lfo));
                rows.push_back(signedNumber(prefix + "amount", {}, lfoGroup(lfo),
                                            std::string("How much the source moves the ") + modulation.what + " of LFO " +
                                                std::to_string(lfo) + ", as a signed amount.",
                                            ParameterScope::Program, modulation.setValue, modulation.getValue, LEVEL_MAX, {lfo}));
            }
        }
        rows.push_back(number("lfo 1 modwheel", {}, GROUP_LFO_1, "How much the modwheel adds to LFO 1's modulation.",
                              ParameterScope::Program, akm::ItemId::ProgramSetLfoModwheel, akm::ItemId::ProgramGetLfoModwheel,
                              LEVEL_MAX, {FIRST_LFO}));
        rows.push_back(number("lfo 1 aftertouch", {}, GROUP_LFO_1, "How much aftertouch adds to LFO 1's modulation.",
                              ParameterScope::Program, akm::ItemId::ProgramSetLfoAftertouch,
                              akm::ItemId::ProgramGetLfoAftertouch, LEVEL_MAX, {FIRST_LFO}));
        rows.push_back(onOff("lfo 2 clock sync", {"lfo 2 midi clock sync"}, GROUP_LFO_2,
                             "Whether LFO 2 follows the MIDI clock (LFO 2 only).", ParameterScope::Program,
                             akm::ItemId::ProgramSetLfoMidiClockSyncEnable, akm::ItemId::ProgramGetLfoMidiClockSyncEnable,
                             {SECOND_LFO}));
        rows.push_back(choice("lfo 2 clock division", {"lfo 2 midi clock division"}, GROUP_LFO_2,
                              "How LFO 2's cycle divides the MIDI clock when it follows it: from 8 cycles per beat to 64 beats "
                              "per cycle (LFO 2 only).",
                              ParameterScope::Program, akm::ItemId::ProgramSetLfoMidiClockSyncDivision,
                              akm::ItemId::ProgramGetLfoMidiClockSyncDivision, clockDivisionLabels(), {SECOND_LFO}));
        return rows;
    }
}
