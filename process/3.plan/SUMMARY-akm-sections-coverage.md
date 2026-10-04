# SysEx Sections Coverage — Summary

> Reference document, not an AGNOS artifact (no ID, ignored by the index). Generated 2026-09-30, updated
> 2026-10-03 from `documents/_index/sysex_spec.items.tsv`, `sysex_spec.kb.md` (Table 4) and
> `generate_akm_items.py --coverage`.
> "Spec lines" = Control + REPLY lines from `items.tsv` for the section (560 total, entire protocol).
> Mermaid has no built-in 100%-stacked bar chart, so each section's bar below is drawn as 20 Unicode
> blocks (`█` done, `░` not done) in a monospace table column — one bar per section, each reaching 100%.

## Coverage, one bar per section

| Section | Name | Spec lines | Done | Bar (20 chars = 100%) |
|---|---|---:|---:|---|
| §00 | SysEx config | 7 | 100 % | `████████████████████` |
| §02 | System setup | 27 | 100 % | `████████████████████` |
| §04 | MIDI config | 7 | 100 % | `████████████████████` |
| §06 | Keygroup zone | 42 | 100 % | `████████████████████` |
| §08 | Keygroup | 120 | 100 % | `████████████████████` |
| §0A | Program | 141 | 100 % | `████████████████████` |
| §0C | Multi | 60 | 100 % | `████████████████████` |
| §0E | Sample tools | 53 | 100 % | `████████████████████` |
| §10 | Disk tools | 51 | 100 % | `████████████████████` |
| §12 | Multi FX | 18 | 0 % | `░░░░░░░░░░░░░░░░░░░░` |
| §14 | Scenelist | 12 | 100 % | `████████████████████` |
| §16 | MIDI song files | 18 | 100 % | `████████████████████` |
| §20 | Front panel | 4 | 100 % | `████████████████████` |

542 of 560 spec lines covered overall (97 %); every implemented section's commands are fully catalogued
(no section is partially done right now — §02 was the last partial one before §10, closed by
`TASK-AKM-053`/`TASK-AKM-055`). Coverage confirmed by `generate_akm_items.py --coverage`
(`unaccounted: none`) for §00/§02/§06/§08/§0A/§0E/§10 — §10's own `35 of 35 spec rows covered` reached by
`TASK-AKM-066` (`PLAN-AKM-007`, 2026-10-03), the largest single-section lot so far (51 rows, six items
flagged by the spec as potentially long-running, three guarded destructive ones, two new catalogue
decisions — `qword`, DEC-AKM-017, and a second `String` argument, DEC-AKM-018).
§02's own `complete` flag in `items.json` was flipped to `true` by `TASK-AKM-054` (`PLAN-AKM-006`,
2026-10-02), which re-ran `--coverage` (16/16, no unaccounted item) and recorded the Play Mode range
erratum as resolved. §02 was confirmed on a real S5000, 2026-10-01 (`akm-suite-20261001-223720.log`).
§10's own `complete` flag was flipped to `true` by `TASK-AKM-068` (`PLAN-AKM-007`, 2026-10-03), which
re-ran `--coverage` (35/35, no unaccounted item) — the &0D/&0E decimal erratum was already resolved by
`TASK-AKM-059`, so this closure needed no further erratum work, the same way §02's `TASK-AKM-054`
needed none beyond confirming `TASK-AKM-053`'s own resolution.

§20's own `complete` flag was flipped to `true` by `TASK-AKM-074` (`PLAN-AKM-008`, 2026-10-03), which re-ran
`--coverage` (4/4, no unaccounted item): four command rows, no REPLY row, no erratum (Table 31 lists 43 of the 44
keycodes of `&40`-`&6B`, `&66` unlisted, which the primitives refuse). §20 was run on the owner's real sampler with
the owner-driven `--front-panel` check, twice on 2026-10-04 (8 checks of 8 each, 250 and 343 commands, all answered OK then
DONE; `process/2.architecture/OBSERVATIONS-RQ-AKM-076-front-panel.md`), and the owner reports the shortcuts worked. Not pressed
yet: Escape, `-`/`+`, most digits, four mode keys and any character in the text mode (the ASCII item `&04`). The owner's own
eyes are the read-back, the section having no Get.

§04's own `complete` flag was flipped to `true` by `TASK-AKM-080` (`PLAN-AKM-009`, 2026-10-04), which re-ran `--coverage`
(7/7, no unaccounted item): seven command rows, no REPLY row, no erratum. §04 has not been run on a real sampler yet.
Unlike §20 it sets stored configuration (UTILITIES > MIDI SETUP and MIDI FILTER) that cannot be read back, so the
owner-guided `--midi-config` check asks the owner for the values to put back (`RQ-AKM-080`); the owner's eyes on the
sampler's screen are its read-back.

§04 was run on the owner's S5000 (OS 2.14) on 2026-10-04 (`akm-suite-20261004-103116.log`, third run,
`process/2.architecture/OBSERVATIONS-RQ-AKM-080-midi-config.md`): **all seven items are obeyed** and every change was seen on
the sampler's screen. The sampler redraws those pages after a SysEx message only while §00/&05 (automatic screen updating)
is on; with it off, as in the first two runs, only `&01` and `&07` showed. Not observed: the channel code of `&06`/`&07`
for port B (codes 16-31), the three other filter event types, and the effect on real MIDI input.

§16's own `complete` flag was flipped to `true` by `TASK-AKM-087` (`PLAN-AKM-010`, 2026-10-04), which re-ran `--coverage`
(12/12, no unaccounted item): twelve command rows (the eight of the song files, the four of the set lists) and six REPLY
rows, no erratum beyond the two cosmetic ones of the kb. The real S5000 (OS 2.14) answered §16 on 2026-10-04 but held no
song file and no set list: `--song-files` could read the counts (both 0) and see ERROR 4 for every item naming something,
nothing more (`process/2.architecture/OBSERVATIONS-RQ-AKM-085-song-files.md`). Renaming, selection and the REPLYs of the
Gets for items that exist have not been run on a real sampler yet.

§0C's own `complete` flag was flipped to `true` by `TASK-AKM-095` (`PLAN-AKM-011`, 2026-10-04), which re-ran `--coverage`
(47/47, no unaccounted item): the multi lifecycle and its guarded "Delete ALL", the twelve part parameters (Set and Get),
the five Sets of general information and the ten Gets, no erratum. The real S5000 (OS 2.14) ran `--multi-lifecycle` on
2026-10-04 (nine checks of nine, third run; `process/2.architecture/OBSERVATIONS-RQ-AKM-093-multi.md`): every item the check
sends was obeyed and read back, and two behaviours the spec leaves open were found and are now in the code — setting a
part's solo clears its mute, and the program number's "off" needs its number byte (`00 00`). Not run on hardware: `&01`
and `&07`, by design.

§14's own `complete` flag was flipped to `true` by `TASK-AKM-099` (`PLAN-AKM-012`, 2026-10-04), which re-ran `--coverage`
(8/8, no unaccounted item): eight command rows (the same shape as the song file half of §16) and four REPLY rows, no
erratum. The real S5000 (OS 2.14) supports the section but held no scenelist: `--scenelists` read the count (0) and saw
ERROR 4 for every item naming something, nothing more (`process/2.architecture/OBSERVATIONS-RQ-AKM-097-scenelist.md`).
Renaming, selection and the REPLYs of the Gets for a scenelist that exists have not been run on a real sampler yet.
Section still at 0 %: §12 Multi FX (18 lines).

### §10 on the real S5000 (2026-10-03)

The table above measures the catalogue against the spec. This is separate: what the owner's S5000 (OS 2.14, disk
handle 129 "S5K", hard disk) actually answered, from the `--disk-tools`, `--disk-tools-files` and
`--disk-tools-slow` runs of the day (`akm-suite-20261003-*.log` and the named logs in
`juce/build/tests/probe/Debug`).

- **33 of 35 items answered correctly**: `&02 &03 &04 &05 &06 &07 &08 &09 &0A &0B &0E` (safe check);
  `&10 &11 &12 &13 &14 &16 &17 &18 &20 &24` (safe check, folders and file count); `&15 &2A &2B &2C &2D` (slow items);
  `&21 &22 &23 &28 &29` (file check: names, size, rename, delete); `&30 &31` (audition check: the first .WAV at the root, played
  for 3 seconds with position 2 in the root's list, then stopped).
- **`&01` update the list of disks: no answer.** Timeout after 3 s with no `F0 F7`, even with Still Alive on; the
  sampler then needed a power cycle by hand. The owner's SCSI2SD disk is suspected, not verified.
- **`&0D` eject disk: not run**, by design (destructive, outside the suite).

Open observations: `&24` (get index of a file by name) gave answers that do not match the `&22` list positions: for
`S1.WAV` it answered 1 while `&30` plays it with 2, and `&24` finds a sample by its name without the extension only
(`S1` found, `S1.WAV` refused), while a program is found with its extension (`X01.AKP` found, `X01` refused). `&0B` free
space answered 0 bytes on a disk with about 10 GB free. `&28` rename: the name is given
without its extension, and the sampler appends the renamed file's own extension (`XS56K_RENAMED` → `XS56K_RENAMED.AKP`;
giving `XS56K_RENAMED.AKP` produced `XS56K_RENAMED.AKP.AKP`). Observed on a program file; not yet on a sample (`.WAV`).

Not in the table above — 0 of their own spec lines, so no bar applies:

| Section | Name | Spec lines |
|---|---|---:|
| §2A/2C/2E/32 | Alt by index (program / multi / sample / multi FX) | 0 — reuses items from target sections, no own items |
| §38/3A/3C/3E | Blocked = batch request (zone / keygroup / program / multi) | 0 — same |
