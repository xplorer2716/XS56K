# KB — AKAI S5000/S6000 SysEx spec v2.10 (agent-oriented, dense)

Source: `documents/akai_s5000_s6000_sysex_spec_2.10.pdf` (46 pdf pages; printed page N = pdf page N+4)
and its conversion `…pdf.md` (tables partly broken — prefer `_index/sysex_spec.clean.txt`).
Ref notation: `pN` = printed page, `mdL` = line in the `.md`, `T#` = spec table number.
Everything below was transcribed from the spec; `[?]` marks my own inference, not stated by the spec.

## Lookup recipe
- Item by section/code/name → `grep -P "^0A\tC\t&21" _index/sysex_spec.items.tsv` or `grep -i "cutoff" …items.tsv`
  cols: sec kind(C=command,R=REPLY format) item dec rt fn d1 d2(+extra data, ";"-sep) desc group tbl pdf doc md
- Table footnotes → `grep "^#FN\t<tbl>" …items.tsv` (fn column of an item = footnote letters)
- Full context/prose → `_index/sysex_spec.clean.txt` (page banners `=== pdf pX | doc pY | md La-b ===`, `## ` = sub-group row)
- Concepts → `grepai search "<english query>"` then Read the md lines.

## Map (topic → p / mdL)
| topic | p | mdL |
|---|---|---|
| intro, ports A/B, RT marker, `&HEX{DEC}` notation | 1 | 86-115 |
| modification history 1.20/1.30/2.00/2.10 | 2 | 117-160 |
| header, DeviceID, T1 user-ref count | 3-4 | 181-225 |
| checksum + example + "all checksums off" tip | 4-5 | 226-253 |
| complete message format | 5 | 254-261 |
| confirmation msgs T2 | 5-6 | 262-300 |
| error numbers T3 | 6-7 | 301-339 |
| sections T4 | 7-8 | 340-387 |
| data formats (ellipsis, word, dword, qword, signed, string) | 8-9 | 388-437 |
| §00 SysEx config T5 | 10 | 441-477 |
| §02 System T6 / REPLY T7 | 10-11 | 478-544 |
| §04 MIDI config T8 | 12 | 547-570 |
| §06 KG zone T9 / T10 | 12-14 | 571-708 |
| §08 Keygroup T11 / T12 | 15-19 | 711-1038 |
| §0A Program T13 / T14 | 20-25 | 1041-1409 |
| Mod sources T15 | 25 | 1410-1418 |
| §0C Multi T16 / T17 | 26-28 | 1421-1583 |
| §0E Sample T18 / T19 | 29-31 | 1586-1726 |
| §10 Disk T20 / T21 | 32-34 | 1729-1902 |
| §12 Multi FX T22 / T23, EB20 fig.2 | 35-36 | 1905-2007 |
| FX module codes T24, FX params T25 | 37-38 | 2010-2090 |
| §14 Scenelist T26 / T27 | 39 | 2093-2121 |
| §16 MIDI song files T28 / T29 | 40 | 2124-2163 |
| §20 Front panel T30, keycodes T31 | 41 | 2166-2211 |
| alt/blocked sections T32 | 42 | 2214-2260 |

## Framing
- Command: `F0 47 5E <dev> <uref×1..4> <section> <item> <data…> [chk] F7`
- Confirmation: `F0 47 5E <dev> <uref×n> <replyID> <section> <item> <data…> [chk] F7` (dev/urefs echoed as sent)
- `dev`: bits0-4 DeviceID 0–31 (since OS 1.30); bits5-6 = number of user-refs − 1 (00→1, 01→2, 10→3, 11→4).
  Sampler DeviceID 0 (default) answers everything; sending DeviceID 0 addresses all samplers.
  Discovery: "Query" §00/&00 with DeviceID 0 → OK + DONE from each sampler.
- user-refs: free values, echoed in every confirmation (tag/match requests). (p3 header line writes `<0..&F7>`: typo, data bytes ≤ &7F.)
- checksum: 8-bit wrapping sum from first user-ref to last data byte, then `& 0x7F`, placed before F7.
  Off by default; toggled only by §00/&04 (per port). When off, a trailing checksum is tolerated/ignored.
  Example: `F0 47 5E 05 10 0C 1B 35 6D 59 F7` (sum 10+0C+1B+35+6D=D9 → 59).
  All-samplers checksum off: `F0 47 5E 00 00 00 04 00 04 F7`.
- All data bytes ≤ &7F. REPLY data use the same encodings as commands.
- replyID: `4F 'O'` OK (received; can be disabled §00/&01) · `44 'D'` DONE · `52 'R'` REPLY+data · `45 'E'` ERROR, err = d1·128 + d2.
  DONE/REPLY/ERROR cannot be disabled → ≥1 answer per message. Flow: OK → DONE|REPLY|ERROR (REPLY then ERROR possible, rare).
- Still-alive (§00/&07 on): `F0 F7` about every second while a long operation is pending.
- Ports A/B decode independently, each answers on its own OUT; §00 settings are per port.
  Input is buffered but can overflow → wait for confirmations. Since OS 2.10: one outstanding msg per port
  synchronised on DONE/REPLY/ERROR guarantees musical MIDI is not disturbed.
- `RT` flag (items.tsv col rt) = item designed for real-time use alongside musical MIDI.
- Section codes 44,45,4F,52 reserved (= the replyID bytes).

## Errors (T3; err = MSB·128+LSB)
00 not supported · 01 invalid format/insufficient data · 02 parameter out of range · 03 unknown error ·
04 requested program/multi/sample/etc. not found · 05 "new" element could not be created · 06 deletion failed ·
&81(129) checksum invalid ·
disk &101(257)… : 101 selected disk invalid · 102 error during load · 103 item not found · 104 unable to create ·
105 folder not empty · 106 unable to delete · 107 unknown · 108 error during save · 109 insufficient space ·
10A write-protected · 10B name not unique · 10C invalid disk handle · 10D disk empty · 10E aborted ·
10F failed on open · 110 read error · 111 not ready · 112 SCSI error ·
&181(385) requested keygroup does not exist in current program.

## Sections (T4)
00 SysEx config (T5) · 02 System setup (T6/T7) · 04 MIDI config (T8) · 06 Keygroup zone (T9/T10) ·
08 Keygroup (T11/T12) · 0A Program (T13/T14, T15) · 0C Multi (T16/T17) · 0E Sample tools (T18/T19) ·
10 Disk tools (T20/T21) · 12 Multi FX (T22/T23, T24, T25) · 14 Scenelist (T26/T27) ·
16 MIDI song files (T28/T29) · 20 Front panel (T30/T31) ·
Alt by index: 2A program · 2C multi · 2E sample · 32 multi FX ·
Blocked (multi-request): 38 KG zone · 3A keygroup · 3C program · 3E multi.

## Data encodings (p8-9)
- `a, b, c` = one discrete value; `a–b` = inclusive range; `…` = more bytes follow.
- word: `MSB LSB` → LSB + 128·MSB. dword: `MSB SB2 SB1 LSB` → LSB + 128·SB1 + 128²·SB2 + 128³·MSB.
  qword `QWORD<8 bytes>`: MSB first, base 128.
- signed: sign byte first (0 = +, 1 = −), then magnitude (byte/word/dword). Most ±params use 2 bytes: `sign abs`.
- string: ASCII, null-terminated (`char1…0`), the 0 is mandatory. Lists = concatenated null-terminated strings.

## State model
- §0A/§0C/§0E/§14/§16 act on the *current* item: select by name (&05) or by index (&06, zero-based word).
  Create (&02) also makes it current. Current item ≠ LCD item unless sync (§00/&03) is on and the mode matches.
  Spec says sync is the default; recommends turning it off except when needed (T5 fn a).
- §08: select keygroup §08/&01 (1–99, 0 = all) inside the current program; Get with KG=0 returns one data set per KG.
- §06: zone number is d1 of every message (1–4, 0 = all four); refers to current KG of current program.
  Get zone 0 → 4 sets; zone 0 + KG 0 → sets for every zone of every KG (KG1 first).
- Multi part edits: part = d1 (0–127). A part's program is edited via §0A (get part program name §0C/&45, select it).
- §2A/2C/2E/32: `<section> <idxMSB> <idxLSB> <item> <data…>` (T32 column order; p42: "index in the first 2 bytes,
  rest identical to the usual section"). Does not change the current item.
- §38/3A: `<section> <progMSB> <progLSB> <KG 1–99>` then requests; §3C/3E: `<section> <idxMSB> <idxLSB>` then requests.
  Each request = `<Item> <Data1> <Data2> <Data3>` (spec wording: "3 data bytes … for each parameter", unused = 0).
  One REPLY = concatenation of normal replies; on error: ERROR without REPLY, or a truncated REPLY.
  Never KG=0 / zone=0 with blocked Gets (unpredictable).
- Multi FX (§12) acts on the current multi; channel/module/param indices zero-based;
  param values always signed word `sign MSB LSB`. Discover layout first (&01, &10, &11).
- Disk (§10): refresh list (&01) → pick handle (14-bit, from &05 list) → select (&02); SysEx disk selection ≠ front panel.
  File/folder indices shift when disk changes.

## Common value codes
- MIDI channel 0–31 = 1A…16B · note 21–127 = A-1…G8 · pan 14–114 = L50…R50, centre 64.
- Output (zone §06/&04): 0 MULTI, 1–8 op1/2…op15/16, 9–24 op1…op16. Output (multi part §0C/&14): 0–7 stereo pairs, 8–23 op1…op16.
- FX send/override: 0 OFF, 1 FX1, 2 FX2, 3 RV3, 4 RV4.
- Program/multi "number": front-panel 1–128 sent as 0–127 (prefixed by an OFF/ON byte).
- Play mode (§02): 0 multi, 1 program, 2 sample, 3 muted.
- Zone playback 0–6: NO LOOPING, ONE SHOT, LOOP IN REL, LOOP UNTIL REL, LIR→RETRIG, PLAY→RETRIG, AS SAMPLE (sample: 0–5, no AS SAMPLE).
- Filter mode 0–25: 2-POLE LP, 4-POLE LP, 2-POLE LP+, 2-POLE BP, 4-POLE BP, 2-POLE BP+, 1-POLE HP, 2-POLE HP, 1-POLE HP+,
  LO<>HI, LO<>BAND, BAND<>HI, NOTCH 1, NOTCH 2, NOTCH 3, WIDE NOTCH, BI-NOTCH, PEAK 1, PEAK 2, PEAK 3, WIDE PEAK, BI-PEAK,
  PHASER 1, PHASER 2, BI-PHASE, VOWELISER. Cutoff 0–100, resonance 0–15, attenuation 0–5 = 0…30 dB (6 dB steps).
- KG level 0–10 = −30…+30 dB (6 dB steps). Semitone tune ±36, fine ±50, as sign+abs.
- LFO wave 0–8: SINE, TRIANGLE, SQUARE, SQUARE+, SQUARE−, SAW BI, SAW UP, SAW DOWN, RANDOM.
  MIDI clock division 0–68: 0 8cy/bt, 1 6cy/bt, 2 4cy/bt, 3 3cy/bt, 4 2cy/bt, 5 1cy/bt, 6 2bt/cy, 7 3bt/cy … 68 64bt/cy.
- Tune template 0–7: USER, EVEN-TEMPERED, ORCHESTRAL, WERKMEISTER, 1/5 MEANTONE, 1/4 MEANTONE, JUST, ARABIAN. Key 0–11 = C…B.
- Mod source (T15) 0–14: NO SOURCE, MODWHEEL, BEND, AFTERTOUCH, EXTERNAL, VELOCITY, KEYBOARD, LFO1, LFO2, AMP ENV,
  FILT ENV, AUX ENV, MODWHEEL, BEND, EXTERNAL (12–14 repeat names; the spec does not explain the difference).
- Multi parts: set 0/1/2 = 32/64/128; replies give 31/63/127 (= n−1). Mute/solo list: 0 none, 1 mute, 2 solo.
- Disk type 0 floppy, 1 HD, 2 CD-ROM, 3 removable. Disk format 0 other, 1 MSDOS, 2 FAT32, 3 ISO9660, 4 S1000, 5 S3000, 6 EMU, 7 ROLAND.
  Memory item type (save) 1 multi, 2 program, 3 sample, 4 SMF, 5 setlist, 6 scenelist. Load file mode 0 normal, 1 RAM, 2 VIRTUAL.
- Sample type 0 RAM, 1 VIRTUAL; channels 1 mono, 2 stereo; length/rate/positions = dword.

## FX (§12) — EB20 board (fig.2, p35)
Channels 0 and 1: module 0 RINGMOD/DIST · 1 EQ · 2 MODULATION (chorus, flange, phase, rotary speakers, fmod/autopan,
pitch shift, pitch+fback) · 3 DELAY (mono left, mono L+R, mono xover, stereo) · 4 REVERB · 5 OUTPUT CONTROL.
Channels 2 and 3: REVERB INPUT · REVERB. Only modules 2 and 3 of channels 0/1 can change type.
Module codes (T24): 00 none, 01 RingMod/Dist, 02 Chorus, 03 Flange, 04 Phase, 05 Rotary, 06 FMod Autopan, 07 PitchShift,
08 PitchShift+Feedback, 09 EQ, 0A Mono Delay, 0B Mono L+R, 0C Mono Xover, 0D Stereo Delay, 0E Reverb, 0F Output Mix, 10 Reverb Input.
Params (T25, index: name range):
- RingMod/Dist: 0 Distortion 0–100 · 1 Output Level 0–100 · 2 RMod Freq 1–5000Hz · 3 RMod Depth 0–100
- Chorus/Flange/Phase: 0 Rate 0–99=0.0–9.9 · 1 Depth 0–100 · 2 Feedback −50…+50
- Rotary: 0 Speed1, 1 Speed2, 2 Acceleration (0–99=0.0–9.9) · 3 Depth/Width 0–100 · 4 Init Speed 0/1 · 5 MIDI Control 1–127 · 6 MIDI Mode 0 level/1 toggle · 7 MIDI Channel 0–31
- PitchShift: 0 L semi, 1 L fine, 2 R semi, 3 R fine (all −50…+50)
- PitchShift+FB: 0 L semi, 1 L fine, 2 L delay 0–275ms, 3 L feedback 0–100, 4 R semi, 5 R fine, 6 R delay 0–275ms, 7 R feedback 0–100
- Mono Delay / L+R / Xover: 0 Delay 0–670ms · 1 Feedback 0–100% · 2 HF damp 0–46=100–20kHz · 3 Ping Pong −50…+50 · 4 FB monitor 0 PRE/1 POST
- Stereo Delay: 0 L delay 0–335ms · 1 L FB · 2 L HF damp · 3 R delay 0–335ms · 4 R FB · 5 R HF damp · 6 FB monitor
- EQ: 0 Low Freq 0–30=16–500Hz · 1 Low Gain −37…+12dB · 2 LowMid Freq 0–44=40–6k3Hz · 3 LowMid Gain · 4 LowMid Width 0–100 ·
  5 HighMid Freq 0–44 · 6 HighMid Gain · 7 HighMid Width · 8 High Freq 0–30=500–16kHz · 9 High Gain ·
  10 LM sweep rate 0–99 · 11 LM sweep depth 0–100 · 12 HM sweep rate · 13 HM sweep depth
- FMod Autopan: 0 FMod rate 0–99 · 1 FMod depth · 2 FMod feedback 0–100 · 3 AMod rate · 4 AMod depth · 5 AMod mode 0–3 = PAN…TREMOLO
- Reverb: 0 Type 0–6 · 1 Pre-delay 0–90 · 2 Decay/Time 0–100 · 3 Diffusion · 4 Near · 5 Level (0–100) · 6 Pan −50…+50 · 7 LF damp 0–40=10–1k0Hz · 8 HF damp 0–25=1k0–20kHz
- Output Mix: 0 Mod/Delay level · 1 Mod/Delay pan · 2 Mod/Delay width · 3 Direct signal 0/1 · 4 Path control −50…+50 · 5 Dist/EQ level · 6 Dist/EQ pan
- Reverb Input: 0 RV Input 0–4

## Front panel keycodes (T31, §20/&01 hold, &02 release; always release after hold)
Mode: FX 40 · RECORD 41 · EDIT SAMPLE 42 · EDIT PROGRAM 43 · MULTI 44 · UTILITIES 45 · SAVE 46 · LOAD 47 ·
F1–F8 48–4F · F9–F16 50–57 · digits 0–9 58–61 · − 62 · + 63 · CURSOR< 64 · CURSOR> 65 · WINDOW 67 · MARK 68 · JUMP 69 · EXIT 6A · ENT/PLAY 6B.
&03 data wheel: d1 0 fwd/1 back, d2 clicks 1–8 · &04 ASCII key. DONE = queued, not executed (T30 fn a).

## Spec errata / inconsistencies (checked against the PDF)
- T11/T12 `&6C{107}` → &6C is 108 (T11 p18, T12 p19).
- T20 `&0E{13}` "Get the name of the specified disk" → &0E is 14 (p32).
- T29 title says §&14{20}: it is §&16{22} (MIDI song files); its intro points to T27 instead of T29 (p40). items.tsv stores sec 16.
- T28 sub-group header "Scenelist Songfile" is a copy/paste leftover (p40).
- §02/&10 Set Play Mode: d1 listed "0, 1, 2" but text defines 3 = Muted (p11).
- T10 `&27{39}` labelled "Set Zone Semitone Tune" in a REPLY table (= Get) (p14).
- T14 `&2C/&2D` "Amp Pan Source/Value" [?] = Pan Mod Source/Value (commands &24/&25/&2C/&2D are Pan Mod) (p24).
- T11 `&64{100}` "Set Aux Env. Velocity→Rate (4 only)" [?] = Off Velocity→Rate (its Get &6C is "Off Velocity→Rate") (p17).
- T12 `&48`, `&5F` reply say "Off Velocity→Rate" while commands say "Off Velocity→Release" (p19).
- §00 has no item &02 (not listed).
