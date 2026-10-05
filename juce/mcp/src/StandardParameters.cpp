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
        constexpr const char* GROUP_KEYGROUP = "keygroup";
        constexpr const char* GROUP_PITCH_AMP = "pitch and amplitude";
        constexpr const char* GROUP_AUX_ENVELOPE = "aux envelope";
        constexpr const char* GROUP_OUTPUT = "output";
        constexpr const char* GROUP_TUNING = "tuning";
        constexpr const char* GROUP_PITCH_BEND = "pitch bend";
        constexpr const char* GROUP_ZONE = "zone";

        // The sampler's own ranges (spec Tables 11 and 13), as the item catalogue gives them.
        constexpr std::int64_t LEVEL_MAX = 100;
        constexpr std::int64_t FILTER_RESONANCE_MAX = 15;
        constexpr std::int64_t FILTER_KEYBOARD_TRACK_MAX = 36;
        constexpr std::int64_t FILTER_ATTENUATION_STEP_DB = 6;
        constexpr std::int64_t FILTER_ATTENUATION_MAX_DB = 30;
        constexpr const char* UNIT_DB = "dB";
        constexpr std::int64_t FIRST_LFO = 1;
        constexpr std::int64_t SECOND_LFO = 2;


        // Zone ranges (spec Table 9). The pan is a code 14 to 114 with 64 at the centre.
        constexpr std::int64_t ZONE_PAN_MAX = 50;
        constexpr std::int64_t ZONE_PAN_CENTRE_CODE = 64;
        constexpr std::int64_t VELOCITY_MAX = 127;
        constexpr std::int64_t VELOCITY_TO_START_MAX = 9999;
        constexpr std::int64_t ZONE_FILTER_MAX = 100;

        // Table 9, item &04: 0 MULTI, 1 to 8 the stereo pairs op1/2 to op15/16, 9 to 24 the outputs op1 to op16.
        const std::vector<std::string>& zoneOutputLabels()
        {
            static const std::vector<std::string> labels = [] {
                constexpr int OUTPUT_PAIRS = 8;
                constexpr int OUTPUTS = 16;
                std::vector<std::string> made{"MULTI"};
                for (int pair = 0; pair < OUTPUT_PAIRS; ++pair)
                    made.push_back("OP" + std::to_string(2 * pair + 1) + "/" + std::to_string(2 * pair + 2));
                for (int output = 1; output <= OUTPUTS; ++output)
                    made.push_back("OP" + std::to_string(output));
                return made;
            }();
            return labels;
        }

        // Table 9, item &09.
        const std::vector<std::string>& zonePlaybackLabels()
        {
            static const std::vector<std::string> labels{"NO LOOPING",   "ONE SHOT",     "LOOP IN REL", "LOOP UNTIL REL",
                                                         "LIR->RETRIG", "PLAY->RETRIG", "AS SAMPLE"};
            return labels;
        }

        // Lot 3 ranges (spec Tables 11 and 13).
        constexpr std::int64_t FIRST_NOTE = 21;  // A-1
        constexpr std::int64_t LAST_NOTE = 127;  // G8
        constexpr std::int64_t MUTE_GROUP_MAX = 32;
        constexpr std::int64_t SEMITONE_TUNE_MAX = 36;
        constexpr std::int64_t FINE_TUNE_MAX = 50;
        constexpr std::int64_t KEYGROUP_LEVEL_MIN_DB = -30;
        constexpr std::int64_t KEYGROUP_LEVEL_MAX_DB = 30;
        constexpr std::int64_t KEYGROUP_LEVEL_STEP_DB = 6;
        constexpr std::int64_t PITCH_BEND_MAX_SEMITONES = 24;
        constexpr std::int64_t AFTERTOUCH_PITCH_MAX_SEMITONES = 12;
        constexpr std::int64_t AUX_STAGES = 4;
        constexpr std::int64_t LAST_AMP_MOD = 2;
        constexpr std::int64_t LAST_PAN_MOD = 3;
        constexpr std::int64_t LAST_PITCH_MOD = 2;
        constexpr std::int64_t FIRST_AUX_RATE_WITH_VELOCITY = 1;
        constexpr std::int64_t LAST_AUX_RATE_WITH_VELOCITY = 4;
        constexpr std::int64_t AUX_OFF_VELOCITY_RATE = 4;

        const std::vector<std::string>& fxOverrideLabels()
        {
            static const std::vector<std::string> labels{"OFF", "FX1", "FX2", "RV3", "RV4"};
            return labels;
        }

        // Table 13, item &32.
        const std::vector<std::string>& tuneTemplateLabels()
        {
            static const std::vector<std::string> labels{"USER",         "EVEN-TEMPERED", "ORCHESTRAL", "WERKMEISTER",
                                                         "1/5 MEANTONE", "1/4 MEANTONE",  "JUST",       "ARABIAN"};
            return labels;
        }

        // Table 13, item &34.
        const std::vector<std::string>& tuneKeyLabels()
        {
            static const std::vector<std::string> labels{"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
            return labels;
        }

        const std::vector<std::string>& bendModeLabels()
        {
            static const std::vector<std::string> labels{"NORMAL", "HELD"};
            return labels;
        }

        const std::vector<std::string>& portamentoModeLabels()
        {
            static const std::vector<std::string> labels{"TIME", "RATE"};
            return labels;
        }

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

    namespace
    {
        // A MIDI note limit of a keygroup: the item carries the note number as it is.
        ParameterDefinition keygroupNote(const char* name, const char* description, akm::ItemId setItem, akm::ItemId getItem)
        {
            ParameterDefinition row =
                number(name, {}, GROUP_KEYGROUP, description, ParameterScope::Keygroup, setItem, getItem, LAST_NOTE);
            row.min = FIRST_NOTE;
            return row;
        }

        // The tuning of a keygroup or of a program: a signed number of semitones and a signed number of cents.
        void addTuning(std::vector<ParameterDefinition>& rows, const char* group, const std::string& owner, ParameterScope scope,
                       akm::ItemId setSemitone, akm::ItemId getSemitone, akm::ItemId setFine, akm::ItemId getFine)
        {
            rows.push_back(signedNumber(owner + " semitone tune", {}, group, "How far the " + owner + " is tuned, in semitones, up or down.",
                                        scope, setSemitone, getSemitone, SEMITONE_TUNE_MAX));
            rows.push_back(signedNumber(owner + " fine tune", {}, group,
                                        "A finer tuning of the " + owner + ", up or down (cents of a semitone).", scope, setFine, getFine,
                                        FINE_TUNE_MAX));
        }

        // A source and an amount of an input that modulates the program or the keygroup.
        void addModulationInput(std::vector<ParameterDefinition>& rows, const char* group, const std::string& name,
                                const std::string& what, akm::ItemId setSource, akm::ItemId getSource, std::int64_t input,
                                ParameterScope amountScope, akm::ItemId setAmount, akm::ItemId getAmount)
        {
            rows.push_back(sourceChoice(name + " source", {}, group, "The source routed to the " + what + ".", setSource, getSource, input));
            rows.push_back(signedNumber(name + " amount", {}, group, "How much the source of the " + what + " moves it, as a signed amount.",
                                        amountScope, setAmount, getAmount, LEVEL_MAX, {input}));
        }

        // Section 06: the parameters of a zone of the current keygroup. The zone is not a leading argument of the row:
        // it is said at each call (1 to 4, or all), and goes first in the item's arguments.
        void addZoneRows(std::vector<ParameterDefinition>& rows)
        {
            rows.push_back(signedNumber("zone level", {}, GROUP_ZONE, "The zone's level; negative values lower it.", ParameterScope::Zone,
                                        akm::ItemId::ZoneSetLevel, akm::ItemId::ZoneGetLevel, LEVEL_MAX));
            ParameterDefinition pan = number("zone pan", {"zone balance", "zone pan balance"}, GROUP_ZONE,
                                             "The zone's pan, or balance for a stereo sample: -50 is fully left, 0 the centre, 50 fully right.",
                                             ParameterScope::Zone, akm::ItemId::ZoneSetPanBalance, akm::ItemId::ZoneGetPanBalance, ZONE_PAN_MAX);
            pan.min = -ZONE_PAN_MAX;
            pan.offset = -ZONE_PAN_CENTRE_CODE;
            rows.push_back(std::move(pan));
            rows.push_back(choice("zone output", {}, GROUP_ZONE,
                                  "Where the zone is sent: MULTI, a stereo pair of outputs (OP1/2 to OP15/16) or one output (OP1 to OP16).",
                                  ParameterScope::Zone, akm::ItemId::ZoneSetOutput, akm::ItemId::ZoneGetOutput, zoneOutputLabels()));
            rows.push_back(signedNumber("zone filter", {}, GROUP_ZONE, "How much the zone moves the filter's cutoff, as a signed amount.",
                                        ParameterScope::Zone, akm::ItemId::ZoneSetFilter, akm::ItemId::ZoneGetFilter, ZONE_FILTER_MAX));
            rows.push_back(signedNumber("zone fine tune", {}, GROUP_ZONE, "A finer tuning of the zone, up or down (cents of a semitone).",
                                        ParameterScope::Zone, akm::ItemId::ZoneSetFineTune, akm::ItemId::ZoneGetFineTune, FINE_TUNE_MAX));
            rows.push_back(signedNumber("zone semitone tune", {}, GROUP_ZONE, "How far the zone is tuned, in semitones, up or down.",
                                        ParameterScope::Zone, akm::ItemId::ZoneSetSemitoneTune, akm::ItemId::ZoneGetSemitoneTune,
                                        SEMITONE_TUNE_MAX));
            rows.push_back(onOff("zone keyboard tracking", {"zone keyboard track"}, GROUP_ZONE,
                                 "Whether the zone's pitch follows the keyboard.", ParameterScope::Zone, akm::ItemId::ZoneSetKeyboardTrack,
                                 akm::ItemId::ZoneGetKeyboardTrack, {}));
            rows.push_back(choice("zone playback", {"zone loop mode"}, GROUP_ZONE,
                                  "How the zone's sample plays: NO LOOPING, ONE SHOT, LOOP IN REL, LOOP UNTIL REL, LIR->RETRIG, "
                                  "PLAY->RETRIG or AS SAMPLE (the sample's own setting).",
                                  ParameterScope::Zone, akm::ItemId::ZoneSetPlayback, akm::ItemId::ZoneGetPlayback, zonePlaybackLabels()));
            ParameterDefinition start = signedNumber("zone velocity to start", {}, GROUP_ZONE,
                                                     "How the note-on velocity moves the start of the zone's sample, as a signed amount "
                                                     "up to 9999.",
                                                     ParameterScope::Zone, akm::ItemId::ZoneSetVelocityToStart,
                                                     akm::ItemId::ZoneGetVelocityToStart, VELOCITY_TO_START_MAX);
            start.magnitudeBytes = 2;
            rows.push_back(std::move(start));
            rows.push_back(number("zone high velocity", {}, GROUP_ZONE, "The highest note-on velocity the zone plays.", ParameterScope::Zone,
                                  akm::ItemId::ZoneSetHighVelocity, akm::ItemId::ZoneGetHighVelocity, VELOCITY_MAX));
            rows.push_back(number("zone low velocity", {}, GROUP_ZONE, "The lowest note-on velocity the zone plays.", ParameterScope::Zone,
                                  akm::ItemId::ZoneSetLowVelocity, akm::ItemId::ZoneGetLowVelocity, VELOCITY_MAX));
            rows.push_back(onOff("zone mute", {}, GROUP_ZONE, "Whether the zone is muted.", ParameterScope::Zone, akm::ItemId::ZoneSetMute,
                                 akm::ItemId::ZoneGetMute, {}));
            rows.push_back(onOff("zone solo", {}, GROUP_ZONE, "Whether the zone is soloed.", ParameterScope::Zone, akm::ItemId::ZoneSetSolo,
                                 akm::ItemId::ZoneGetSolo, {}));
        }

        // Lot 3: the rest of the keygroup (section 08, Table 11) and of the program (section 0A, Table 13).
        void addLot3(std::vector<ParameterDefinition>& rows)
        {
            // Section 08 general options.
            rows.push_back(keygroupNote("keygroup low note",
                                        "The lowest note the keygroup plays, as a MIDI note number from 21 (A-1) to 127 (G8); 60 is C3.",
                                        akm::ItemId::KeygroupSetLowNote, akm::ItemId::KeygroupGetLowNote));
            rows.push_back(keygroupNote("keygroup high note",
                                        "The highest note the keygroup plays, as a MIDI note number from 21 (A-1) to 127 (G8); 60 is C3.",
                                        akm::ItemId::KeygroupSetHighNote, akm::ItemId::KeygroupGetHighNote));
            rows.push_back(number("keygroup mute group", {"mute group"}, GROUP_KEYGROUP,
                                  "The keygroup's mute group, 1 to 32; 0 is no mute group.", ParameterScope::Keygroup,
                                  akm::ItemId::KeygroupSetMuteGroup, akm::ItemId::KeygroupGetMuteGroup, MUTE_GROUP_MAX));
            rows.push_back(choice("keygroup fx override", {"fx override"}, GROUP_KEYGROUP,
                                  "The effects bus the keygroup is sent to instead of the program's: OFF, FX1, FX2, RV3 or RV4.",
                                  ParameterScope::Keygroup, akm::ItemId::KeygroupSetFxOverride, akm::ItemId::KeygroupGetFxOverride,
                                  fxOverrideLabels()));
            rows.push_back(number("keygroup fx send level", {"fx send level"}, GROUP_KEYGROUP,
                                  "How much of the keygroup is sent to the effects.", ParameterScope::Keygroup,
                                  akm::ItemId::KeygroupSetFxSendLevel, akm::ItemId::KeygroupGetFxSendLevel, LEVEL_MAX));
            rows.push_back(onOff("keygroup zone crossfade", {"zone crossfade"}, GROUP_KEYGROUP, "Whether the zones of the keygroup crossfade.",
                                 ParameterScope::Keygroup, akm::ItemId::KeygroupSetZoneCrossfade,
                                 akm::ItemId::KeygroupGetZoneCrossfade, {}));
            rows.push_back(onOff("keygroup crossfade", {"program crossfade"}, GROUP_KEYGROUP,
                                 "Whether the keygroups of the program crossfade where they overlap (a program value).",
                                 ParameterScope::Program, akm::ItemId::ProgramSetCrossfade, akm::ItemId::ProgramGetCrossfade, {}));

            // Section 08 pitch and amplitude of the keygroup, and the inputs that modulate them.
            addTuning(rows, GROUP_PITCH_AMP, "keygroup", ParameterScope::Keygroup, akm::ItemId::KeygroupSetSemitoneTune,
                      akm::ItemId::KeygroupGetSemitoneTune, akm::ItemId::KeygroupSetFineTune, akm::ItemId::KeygroupGetFineTune);
            ParameterDefinition level = number("keygroup level", {}, GROUP_PITCH_AMP, "The keygroup's level, in dB, in steps of 6 dB.",
                                               ParameterScope::Keygroup, akm::ItemId::KeygroupSetLevel, akm::ItemId::KeygroupGetLevel,
                                               KEYGROUP_LEVEL_MAX_DB);
            level.min = KEYGROUP_LEVEL_MIN_DB;
            level.offset = KEYGROUP_LEVEL_MIN_DB;
            level.step = KEYGROUP_LEVEL_STEP_DB;
            level.unit = UNIT_DB;
            rows.push_back(std::move(level));
            for (std::int64_t input = 1; input <= LAST_PITCH_MOD; ++input)
            {
                const std::string which = std::to_string(input);
                addModulationInput(rows, GROUP_PITCH_AMP, "pitch modulation " + which, "pitch modulation input " + which,
                                   akm::ItemId::ProgramSetPitchModSource, akm::ItemId::ProgramGetPitchModSource, input,
                                   ParameterScope::Keygroup, akm::ItemId::KeygroupSetPitchModValue, akm::ItemId::KeygroupGetPitchModValue);
            }
            addModulationInput(rows, GROUP_PITCH_AMP, "keygroup amp modulation", "keygroup's amplitude modulation input",
                               akm::ItemId::ProgramSetKeygroupAmpModSource, akm::ItemId::ProgramGetKeygroupAmpModSource, 1,
                               ParameterScope::Keygroup, akm::ItemId::KeygroupSetAmpModValue, akm::ItemId::KeygroupGetAmpModValue);

            // Section 08 auxiliary envelope: four rates and four levels.
            for (std::int64_t stage = 1; stage <= AUX_STAGES; ++stage)
            {
                const std::string which = std::to_string(stage);
                rows.push_back(number("aux envelope rate " + which, {"aux env rate " + which}, GROUP_AUX_ENVELOPE,
                                      "Rate " + which + " (R" + which + ") of the auxiliary envelope: how fast it moves to level " + which + ".",
                                      ParameterScope::Keygroup, akm::ItemId::KeygroupSetAuxEnvRate, akm::ItemId::KeygroupGetAuxEnvRate,
                                      LEVEL_MAX, {stage}));
            }
            for (std::int64_t stage = 1; stage <= AUX_STAGES; ++stage)
            {
                const std::string which = std::to_string(stage);
                rows.push_back(number("aux envelope level " + which, {"aux env level " + which}, GROUP_AUX_ENVELOPE,
                                      "Level " + which + " (L" + which + ") of the auxiliary envelope.", ParameterScope::Keygroup,
                                      akm::ItemId::KeygroupSetAuxEnvLevel, akm::ItemId::KeygroupGetAuxEnvLevel, LEVEL_MAX, {stage}));
            }
            for (const std::int64_t rate : {FIRST_AUX_RATE_WITH_VELOCITY, LAST_AUX_RATE_WITH_VELOCITY})
            {
                const std::string which = std::to_string(rate);
                rows.push_back(signedNumber("aux envelope velocity to rate " + which, {"aux env velocity to rate " + which},
                                            GROUP_AUX_ENVELOPE,
                                            "How the note-on velocity changes rate " + which + " of the auxiliary envelope, as a signed amount.",
                                            ParameterScope::Keygroup, akm::ItemId::KeygroupSetAuxEnvVelocityToRate,
                                            akm::ItemId::KeygroupGetAuxEnvVelocityToRate, LEVEL_MAX, {rate}));
            }
            rows.push_back(signedNumber("aux envelope keyboard to rate 2 and 4", {"aux env keyboard to rate"}, GROUP_AUX_ENVELOPE,
                                        "How the note played changes rates 2 and 4 of the auxiliary envelope, as a signed amount.",
                                        ParameterScope::Keygroup, akm::ItemId::KeygroupSetAuxEnvKeyboardToR2R4,
                                        akm::ItemId::KeygroupGetAuxEnvKeyboardToR2R4, LEVEL_MAX));
            rows.push_back(signedNumber("aux envelope off-velocity to rate 4", {"aux env off-velocity to rate 4"}, GROUP_AUX_ENVELOPE,
                                        "How the note-off velocity changes rate 4 of the auxiliary envelope, as a signed amount.",
                                        ParameterScope::Keygroup, akm::ItemId::KeygroupSetAuxEnvOffVelocityToRate,
                                        akm::ItemId::KeygroupGetAuxEnvOffVelocityToRate, LEVEL_MAX, {AUX_OFF_VELOCITY_RATE}));

            // Section 0A output: loudness, velocity sensitivity and the amplitude and pan modulation inputs.
            rows.push_back(number("program loudness", {"loudness"}, GROUP_OUTPUT, "The program's loudness.", ParameterScope::Program,
                                  akm::ItemId::ProgramSetLoudness, akm::ItemId::ProgramGetLoudness, LEVEL_MAX));
            rows.push_back(signedNumber("program velocity sensitivity", {"velocity sensitivity"}, GROUP_OUTPUT,
                                        "How much the note-on velocity changes the program's loudness; negative values reverse it.",
                                        ParameterScope::Program, akm::ItemId::ProgramSetVelocitySensitivity,
                                        akm::ItemId::ProgramGetVelocitySensitivity, LEVEL_MAX));
            for (std::int64_t input = 1; input <= LAST_AMP_MOD; ++input)
            {
                const std::string which = std::to_string(input);
                addModulationInput(rows, GROUP_OUTPUT, "program amp modulation " + which, "program's amplitude modulation input " + which,
                                   akm::ItemId::ProgramSetAmpModSource, akm::ItemId::ProgramGetAmpModSource, input,
                                   ParameterScope::Program, akm::ItemId::ProgramSetAmpModValue, akm::ItemId::ProgramGetAmpModValue);
            }
            for (std::int64_t input = 1; input <= LAST_PAN_MOD; ++input)
            {
                const std::string which = std::to_string(input);
                addModulationInput(rows, GROUP_OUTPUT, "pan modulation " + which, "pan modulation input " + which,
                                   akm::ItemId::ProgramSetPanModSource, akm::ItemId::ProgramGetPanModSource, input,
                                   ParameterScope::Program, akm::ItemId::ProgramSetPanModValue, akm::ItemId::ProgramGetPanModValue);
            }

            // Section 0A tuning.
            addTuning(rows, GROUP_TUNING, "program", ParameterScope::Program, akm::ItemId::ProgramSetSemitoneTune,
                      akm::ItemId::ProgramGetSemitoneTune, akm::ItemId::ProgramSetFineTune, akm::ItemId::ProgramGetFineTune);
            rows.push_back(choice("tune template", {"tuning template", "temperament"}, GROUP_TUNING,
                                  "The tuning the program plays in: USER, EVEN-TEMPERED, ORCHESTRAL, WERKMEISTER, 1/5 MEANTONE, "
                                  "1/4 MEANTONE, JUST or ARABIAN.",
                                  ParameterScope::Program, akm::ItemId::ProgramSetTuneTemplate, akm::ItemId::ProgramGetTuneTemplate,
                                  tuneTemplateLabels()));
            rows.push_back(choice("tune key", {"tuning key"}, GROUP_TUNING, "The key the tuning template is rooted on, C to B.",
                                  ParameterScope::Program, akm::ItemId::ProgramSetKey, akm::ItemId::ProgramGetKey, tuneKeyLabels()));

            // Section 0A pitch bend, aftertouch, legato and portamento.
            rows.push_back(number("pitch bend up", {"bend up", "bend range up"}, GROUP_PITCH_BEND,
                                  "How far the pitch bend wheel bends the pitch up, in semitones.", ParameterScope::Program,
                                  akm::ItemId::ProgramSetPitchBendUp, akm::ItemId::ProgramGetPitchBendUp, PITCH_BEND_MAX_SEMITONES));
            rows.push_back(number("pitch bend down", {"bend down", "bend range down"}, GROUP_PITCH_BEND,
                                  "How far the pitch bend wheel bends the pitch down, in semitones.", ParameterScope::Program,
                                  akm::ItemId::ProgramSetPitchBendDown, akm::ItemId::ProgramGetPitchBendDown, PITCH_BEND_MAX_SEMITONES));
            rows.push_back(choice("pitch bend mode", {"bend mode"}, GROUP_PITCH_BEND,
                                  "The pitch bend mode, NORMAL or HELD (the sampler's own names).", ParameterScope::Program,
                                  akm::ItemId::ProgramSetBendMode, akm::ItemId::ProgramGetBendMode, bendModeLabels()));
            rows.push_back(signedNumber("aftertouch pitch", {"aftertouch value"}, GROUP_PITCH_BEND,
                                        "How far aftertouch bends the pitch, in semitones, up or down.", ParameterScope::Program,
                                        akm::ItemId::ProgramSetAftertouchValue, akm::ItemId::ProgramGetAftertouchValue,
                                        AFTERTOUCH_PITCH_MAX_SEMITONES));
            rows.push_back(onOff("legato", {}, GROUP_PITCH_BEND, "The program's legato setting, on or off.", ParameterScope::Program,
                                 akm::ItemId::ProgramSetLegato, akm::ItemId::ProgramGetLegato, {}));
            rows.push_back(onOff("portamento", {"glide"}, GROUP_PITCH_BEND, "Whether the pitch glides from one note to the next.",
                                 ParameterScope::Program, akm::ItemId::ProgramSetPortamentoEnable,
                                 akm::ItemId::ProgramGetPortamentoEnable, {}));
            rows.push_back(choice("portamento mode", {"glide mode"}, GROUP_PITCH_BEND, "Whether the portamento is set by a TIME or by a RATE.",
                                  ParameterScope::Program, akm::ItemId::ProgramSetPortamentoMode, akm::ItemId::ProgramGetPortamentoMode,
                                  portamentoModeLabels()));
            rows.push_back(number("portamento time", {"glide time"}, GROUP_PITCH_BEND, "The portamento's time (or rate, by its mode).",
                                  ParameterScope::Program, akm::ItemId::ProgramSetPortamentoTime, akm::ItemId::ProgramGetPortamentoTime,
                                  LEVEL_MAX));
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
            {GROUP_KEYGROUP, {}, "The keygroup's note range, mute group, effects routing and crossfades."},
            {GROUP_PITCH_AMP, {"pitch", "amplitude"}, "The keygroup's tuning and level and the inputs that modulate its pitch and amplitude."},
            {GROUP_AUX_ENVELOPE, {"aux env", "auxiliary envelope"}, "The keygroup's auxiliary envelope: four rates and four levels."},
            {GROUP_OUTPUT, {}, "The program's loudness, velocity sensitivity and the inputs that modulate its amplitude and pan."},
            {GROUP_TUNING, {"tune"}, "The program's tuning: semitones, cents, tuning template and key."},
            {GROUP_PITCH_BEND, {"bend", "portamento"}, "The program's pitch bend, aftertouch, legato and portamento."},
            {GROUP_ZONE, {"zones"}, "The keygroup's zones: level, pan, output, tuning, playback and velocity range, per zone."},
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

        addLot3(rows);
        addZoneRows(rows);
        return rows;
    }

    namespace
    {
        constexpr const char* GROUP_SAMPLE_PLAYBACK = "playback";
        constexpr const char* GROUP_SAMPLE_PITCH = "pitch";
        constexpr const char* GROUP_SAMPLE_INFO = "info";

        // A position in the sample, or its length, is a compound double word of four 7-bit bytes (spec, data encodings).
        constexpr std::int64_t SAMPLE_POSITION_BYTES = 4;
        constexpr std::int64_t SAMPLE_POSITION_MAX = (1LL << (7 * SAMPLE_POSITION_BYTES)) - 1;
        constexpr std::int64_t SAMPLE_CHANNELS_MIN = 1;
        constexpr std::int64_t SAMPLE_CHANNELS_MAX = 2;
        constexpr const char* UNIT_HZ = "Hz";

        // Table 18, item &28: the sample's own modes (the zone adds AS SAMPLE).
        const std::vector<std::string>& samplePlaybackLabels()
        {
            static const std::vector<std::string> labels{"NO LOOPING", "ONE SHOT", "LOOP IN REL", "LOOP UNTIL REL", "LIR->RETRIG", "PLAY->RETRIG"};
            return labels;
        }

        // Table 18, item &30.
        const std::vector<std::string>& sampleTypeLabels()
        {
            static const std::vector<std::string> labels{"RAM", "VIRTUAL"};
            return labels;
        }

        ParameterDefinition samplePosition(const char* name, const char* description, akm::ItemId setItem, akm::ItemId getItem)
        {
            ParameterDefinition row = number(name, {}, GROUP_SAMPLE_PLAYBACK, description, ParameterScope::Sample, setItem, getItem,
                                             SAMPLE_POSITION_MAX);
            row.magnitudeBytes = SAMPLE_POSITION_BYTES;
            row.unit = "sample points";
            return row;
        }

        ParameterDefinition readOnlyRow(ParameterDefinition row)
        {
            row.readOnly = true;
            return row;
        }
    }

    std::vector<GroupDefinition> sampleGroups()
    {
        return {
            {GROUP_SAMPLE_PLAYBACK, {"loop", "positions"}, "Where the sample starts and ends, where its loop starts and ends, and how it plays."},
            {GROUP_SAMPLE_PITCH, {"tuning"}, "The sample's original pitch and its tuning."},
            {GROUP_SAMPLE_INFO, {"information", "attributes"}, "What the sampler reports about the sample and that cannot be changed: its type, channels, length and rate."},
        };
    }

    std::vector<ParameterDefinition> sampleParameters()
    {
        std::vector<ParameterDefinition> rows;
        rows.push_back(samplePosition("sample start position", "Where the sample starts playing, in sample points from its beginning.",
                                      akm::ItemId::SampleSetStartPosition, akm::ItemId::SampleGetStartPosition));
        rows.push_back(samplePosition("sample end position", "Where the sample stops playing, in sample points from its beginning.",
                                      akm::ItemId::SampleSetEndPosition, akm::ItemId::SampleGetEndPosition));
        rows.push_back(samplePosition("sample loop start",
                                      "Where the sample's loop starts, in sample points. Setting the loop end can move the loop start (seen on "
                                      "the S5000): read both back.",
                                      akm::ItemId::SampleSetLoopStart, akm::ItemId::SampleGetLoopStart));
        rows.push_back(samplePosition("sample loop end", "Where the sample's loop ends, in sample points. Set it before the loop start.",
                                      akm::ItemId::SampleSetLoopEnd, akm::ItemId::SampleGetLoopEnd));
        rows.push_back(choice("sample playback mode", {"sample loop mode", "playback mode"}, GROUP_SAMPLE_PLAYBACK,
                              "How the sample plays: NO LOOPING, ONE SHOT, LOOP IN REL, LOOP UNTIL REL, LIR->RETRIG or PLAY->RETRIG.",
                              ParameterScope::Sample, akm::ItemId::SampleSetPlaybackMode, akm::ItemId::SampleGetPlaybackMode,
                              samplePlaybackLabels()));

        ParameterDefinition pitch = number("sample original pitch", {"original pitch", "sample root note"}, GROUP_SAMPLE_PITCH,
                                           "The note the sample was recorded at, as a MIDI note number from 21 (A-1) to 127 (G8); 60 is C3.",
                                           ParameterScope::Sample, akm::ItemId::SampleSetOriginalPitch, akm::ItemId::SampleGetOriginalPitch,
                                           LAST_NOTE);
        pitch.min = FIRST_NOTE;
        rows.push_back(std::move(pitch));
        rows.push_back(signedNumber("sample semitone tune", {}, GROUP_SAMPLE_PITCH, "How far the sample is tuned, in semitones, up or down.",
                                    ParameterScope::Sample, akm::ItemId::SampleSetSemitoneTune, akm::ItemId::SampleGetSemitoneTune,
                                    SEMITONE_TUNE_MAX));
        rows.push_back(signedNumber("sample fine tune", {}, GROUP_SAMPLE_PITCH, "A finer tuning of the sample, up or down (cents of a semitone).",
                                    ParameterScope::Sample, akm::ItemId::SampleSetFineTune, akm::ItemId::SampleGetFineTune, FINE_TUNE_MAX));

        rows.push_back(readOnlyRow(choice("sample type", {}, GROUP_SAMPLE_INFO, "Whether the sample is RAM or VIRTUAL. Read-only.",
                                          ParameterScope::Sample, akm::ItemId{}, akm::ItemId::SampleGetType, sampleTypeLabels())));
        ParameterDefinition channels = number("sample channels", {}, GROUP_SAMPLE_INFO, "How many channels the sample has: 1 is mono, 2 is stereo. Read-only.",
                                              ParameterScope::Sample, akm::ItemId{}, akm::ItemId::SampleGetChannels, SAMPLE_CHANNELS_MAX);
        channels.min = SAMPLE_CHANNELS_MIN;
        rows.push_back(readOnlyRow(std::move(channels)));
        ParameterDefinition length = number("sample length", {}, GROUP_SAMPLE_INFO, "The sample's length, in sample points. Read-only.",
                                            ParameterScope::Sample, akm::ItemId{}, akm::ItemId::SampleGetLength, SAMPLE_POSITION_MAX);
        length.magnitudeBytes = SAMPLE_POSITION_BYTES;
        length.unit = "sample points";
        rows.push_back(readOnlyRow(std::move(length)));
        ParameterDefinition rate = number("sample rate", {"sampling rate"}, GROUP_SAMPLE_INFO, "The sample's sampling rate, in Hz. Read-only.",
                                          ParameterScope::Sample, akm::ItemId{}, akm::ItemId::SampleGetRate, SAMPLE_POSITION_MAX);
        rate.magnitudeBytes = SAMPLE_POSITION_BYTES;
        rate.unit = UNIT_HZ;
        rows.push_back(readOnlyRow(std::move(rate)));
        return rows;
    }

    namespace
    {
        constexpr const char* GROUP_MULTI_MIX = "mix";
        constexpr const char* GROUP_MULTI_SETUP = "setup";

        constexpr int MIDI_CHANNELS_PER_PORT = 16;
        constexpr int MULTI_OUTPUT_PAIRS = 8;
        constexpr int MULTI_OUTPUTS = 16;
        constexpr std::int64_t PART_TRANSPOSE_MAX = 36;
        constexpr std::int64_t PART_FINE_TUNE_MAX = 50;
        constexpr std::int64_t PART_PAN_MAX = 50;
        constexpr std::int64_t PART_PAN_CENTRE_CODE = 64;

        // Table 16, item &10: 1A to 16A are codes 0 to 15, 1B to 16B codes 16 to 31 (the sampler's two MIDI ports).
        const std::vector<std::string>& partMidiChannelLabels()
        {
            static const std::vector<std::string> labels = [] {
                std::vector<std::string> made;
                for (const char port : {'A', 'B'})
                {
                    for (int channel = 1; channel <= MIDI_CHANNELS_PER_PORT; ++channel)
                        made.push_back(std::to_string(channel) + port);
                }
                return made;
            }();
            return labels;
        }

        // Table 16, item &14: 0 to 7 the stereo pairs op1/2 to op15/16, 8 to 23 the outputs op1 to op16.
        const std::vector<std::string>& partOutputLabels()
        {
            static const std::vector<std::string> labels = [] {
                std::vector<std::string> made;
                for (int pair = 0; pair < MULTI_OUTPUT_PAIRS; ++pair)
                    made.push_back("OP" + std::to_string(2 * pair + 1) + "/" + std::to_string(2 * pair + 2));
                for (int output = 1; output <= MULTI_OUTPUTS; ++output)
                    made.push_back("OP" + std::to_string(output));
                return made;
            }();
            return labels;
        }

        // A part value that is a centred code: -max to max over the codes 0 to 2 * max (the pan's codes 14 to 114).
        ParameterDefinition centred(ParameterDefinition row, std::int64_t max, std::int64_t offset)
        {
            row.min = -max;
            row.offset = offset;
            return row;
        }
    }

    std::vector<GroupDefinition> multiGroups()
    {
        return {
            {GROUP_MULTI_MIX, {"levels"}, "How a part sounds in the mix: mute, solo, level, output, pan, effects channel and FX send level."},
            {GROUP_MULTI_SETUP, {"range"}, "A part's MIDI channel, tuning and key range."},
        };
    }

    std::vector<ParameterDefinition> multiParameters()
    {
        const ParameterScope part = ParameterScope::MultiPart;
        std::vector<ParameterDefinition> rows;
        rows.push_back(onOff("part mute", {}, GROUP_MULTI_MIX, "Whether the part is muted.", part, akm::ItemId::MultiSetMute,
                             akm::ItemId::MultiGetMute, {}));
        rows.push_back(onOff("part solo", {}, GROUP_MULTI_MIX, "Whether the part is soloed.", part, akm::ItemId::MultiSetSolo,
                             akm::ItemId::MultiGetSolo, {}));
        rows.push_back(number("part level", {}, GROUP_MULTI_MIX, "The part's level.", part, akm::ItemId::MultiSetLevel,
                              akm::ItemId::MultiGetLevel, LEVEL_MAX));
        rows.push_back(choice("part output", {}, GROUP_MULTI_MIX,
                              "Where the part is sent: a stereo pair of outputs (OP1/2 to OP15/16) or one output (OP1 to OP16).", part,
                              akm::ItemId::MultiSetOutput, akm::ItemId::MultiGetOutput, partOutputLabels()));
        rows.push_back(centred(number("part pan", {"part balance", "part pan balance"}, GROUP_MULTI_MIX,
                                      "The part's pan, or balance for a stereo part: -50 is fully left, 0 the centre, 50 fully right.",
                                      part, akm::ItemId::MultiSetPanBalance, akm::ItemId::MultiGetPanBalance, PART_PAN_MAX),
                               PART_PAN_MAX, -PART_PAN_CENTRE_CODE));
        rows.push_back(choice("part effects channel", {"part fx channel"}, GROUP_MULTI_MIX,
                              "The effects bus the part is sent to: OFF, FX1, FX2, RV3 or RV4.", part, akm::ItemId::MultiSetEffectsChannel,
                              akm::ItemId::MultiGetEffectsChannel, fxOverrideLabels()));
        rows.push_back(number("part fx send level", {"part fx send"}, GROUP_MULTI_MIX, "How much of the part is sent to the effects.", part,
                              akm::ItemId::MultiSetFxSendLevel, akm::ItemId::MultiGetFxSendLevel, LEVEL_MAX));

        rows.push_back(choice("part midi channel", {"part channel"}, GROUP_MULTI_SETUP,
                              "The MIDI channel the part listens on: 1A to 16A (the first MIDI port) or 1B to 16B (the second).", part,
                              akm::ItemId::MultiSetMidiChannel, akm::ItemId::MultiGetMidiChannel, partMidiChannelLabels()));
        rows.push_back(centred(number("part fine tune", {}, GROUP_MULTI_SETUP, "A fine tuning of the part, in cents: -50 to 50.", part,
                                      akm::ItemId::MultiSetFineTune, akm::ItemId::MultiGetFineTune, PART_FINE_TUNE_MAX),
                               PART_FINE_TUNE_MAX, -PART_FINE_TUNE_MAX));
        rows.push_back(centred(number("part transpose", {}, GROUP_MULTI_SETUP, "How far the part is transposed, in semitones: -36 to 36.",
                                      part, akm::ItemId::MultiSetTranspose, akm::ItemId::MultiGetTranspose, PART_TRANSPOSE_MAX),
                               PART_TRANSPOSE_MAX, -PART_TRANSPOSE_MAX));
        ParameterDefinition low = number("part low note", {}, GROUP_MULTI_SETUP,
                                         "The lowest note the part plays, as a MIDI note number from 21 (A-1) to 127 (G8); 60 is C3.", part,
                                         akm::ItemId::MultiSetLowNote, akm::ItemId::MultiGetLowNote, LAST_NOTE);
        low.min = FIRST_NOTE;
        rows.push_back(std::move(low));
        ParameterDefinition high = number("part high note", {}, GROUP_MULTI_SETUP,
                                          "The highest note the part plays, as a MIDI note number from 21 (A-1) to 127 (G8); 60 is C3.", part,
                                          akm::ItemId::MultiSetHighNote, akm::ItemId::MultiGetHighNote, LAST_NOTE);
        high.min = FIRST_NOTE;
        rows.push_back(std::move(high));
        return rows;
    }
}
