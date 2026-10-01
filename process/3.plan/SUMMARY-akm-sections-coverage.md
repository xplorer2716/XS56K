# SysEx Sections Coverage — Summary

> Reference document, not an AGNOS artifact (no ID, ignored by the index). Generated 2026-09-30, updated
> 2026-10-02 from `documents/_index/sysex_spec.items.tsv`, `sysex_spec.kb.md` (Table 4) and
> `generate_akm_items.py --coverage`.
> "Spec lines" = Control + REPLY lines from `items.tsv` for the section (560 total, entire protocol).
> Mermaid has no built-in 100%-stacked bar chart, so each section's bar below is drawn as 20 Unicode
> blocks (`█` done, `░` not done) in a monospace table column — one bar per section, each reaching 100%.

## Coverage, one bar per section

| Section | Name | Spec lines | Done | Bar (20 chars = 100%) |
|---|---|---:|---:|---|
| §00 | SysEx config | 7 | 100 % | `████████████████████` |
| §02 | System setup | 27 | 100 % | `████████████████████` |
| §04 | MIDI config | 7 | 0 % | `░░░░░░░░░░░░░░░░░░░░` |
| §06 | Keygroup zone | 42 | 100 % | `████████████████████` |
| §08 | Keygroup | 120 | 100 % | `████████████████████` |
| §0A | Program | 141 | 100 % | `████████████████████` |
| §0C | Multi | 60 | 0 % | `░░░░░░░░░░░░░░░░░░░░` |
| §0E | Sample tools | 53 | 100 % | `████████████████████` |
| §10 | Disk tools | 51 | 0 % | `░░░░░░░░░░░░░░░░░░░░` |
| §12 | Multi FX | 18 | 0 % | `░░░░░░░░░░░░░░░░░░░░` |
| §14 | Scenelist | 12 | 0 % | `░░░░░░░░░░░░░░░░░░░░` |
| §16 | MIDI song files | 18 | 0 % | `░░░░░░░░░░░░░░░░░░░░` |
| §20 | Front panel | 4 | 0 % | `░░░░░░░░░░░░░░░░░░░░` |

390 of 560 spec lines covered overall (70 %); every implemented section is complete (no section is
partially done right now — §02 was the last partial one, closed by `TASK-AKM-053`/`TASK-AKM-055`).
Coverage confirmed by `generate_akm_items.py --coverage` (`unaccounted: none`) for §00/§02/§06/§08/§0A/§0E.
§02's own `complete: true` flag in `items.json` is flipped by `TASK-AKM-054` (`PLAN-AKM-006`), which also
records the section's errata resolution; the table above reflects the catalogue's actual content, not
that flag. §02 was confirmed on a real S5000, 2026-10-01 (`akm-suite-20261001-223720.log`).

Not in the table above — 0 of their own spec lines, so no bar applies:

| Section | Name | Spec lines |
|---|---|---:|
| §2A/2C/2E/32 | Alt by index (program / multi / sample / multi FX) | 0 — reuses items from target sections, no own items |
| §38/3A/3C/3E | Blocked = batch request (zone / keygroup / program / multi) | 0 — same |
