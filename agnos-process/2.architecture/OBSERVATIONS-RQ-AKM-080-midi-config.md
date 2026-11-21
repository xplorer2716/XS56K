# Observations — RQ-AKM-080: the MIDI configuration check on the real S5000

Not an AGNOS artifact (no ID of its own, like the other `OBSERVATIONS-*` files). What the owner's S5000 (OS 2.14,
no disk drive attached) did with the section 04 items, from `xs56k_akm_probe --suite --midi-config`. Traceability:
RQ-AKM-078, RQ-AKM-079, RQ-AKM-080, TASK-AKM-079, TASK-AKM-081 (`FTR-AKM-009`, `PLAN-AKM-009`).

## Run of 2026-10-04 (`akm-suite-20261004-100107.log`, not committed)

The seven automatic checks passed (50 Echo round trips: median 12 ms). The owner declared the sampler's MIDI setup as
PROGRAM CHANGE ON, MULTI SELECT OFF, MULTI SLCT CH 1A, EXT APM CONTROL 0, AFTERTOUCH CHANNEL, and the AFTERTOUCH filter
of channel 1A allowing its messages.

| Item | Sent | Sampler's answer | Owner on the screen |
|---|---|---|---|
| `&01` Program Change Enable | `04 01 00` (OFF) | OK then DONE, 10 ms | **seen**: MIDI SETUP showed PROGRAM CHANGE OFF |
| `&02` Multi Select | `04 02 01` (PROG CHANGE), PROGRAM CHANGE still OFF | OK then DONE, 10 ms | **not seen**: nothing changed on the screen (MULTI SELECT stayed OFF; the owner's own words, after the run — the prompt only offered "No, it shows something else") |
| `&03` to `&07` | — | — | not reached: the check stopped at the first "no" |

Both restores (`&02` back to OFF, `&01` back to ON) were answered DONE. The second check (a failure on purpose after
`&02 = PROG CHANGE`, with PROGRAM CHANGE back to ON) sent the same `&02` and got DONE too, but it did not ask the
owner to look at the screen after the change, only after the restore (OFF shown again, as expected): it says nothing
about whether `&02` took effect.

## What was established after run 1

- **Established:** `&01` is obeyed by the S5000 and the screen follows it at once. For `&02`, the sampler answered
  DONE and **ignored** the command: the screen did not change (it did not show another value either). DONE is no
  proof of effect in section 04. The prompt now tells the two cases apart ("nothing changed" / "another value",
  `TASK-AKM-081`).
- **Not established:** why `&02` was not seen. The owner's hypothesis: MULTI SELECT cannot take PROG CHANGE while
  PROGRAM CHANGE is OFF. The operator's manual (pp. 57-58 and 223) states no such dependence, but does not rule it out.
  Other explanations — the page not redrawn for that field, the item numbering of the S5000 — cannot be excluded
  either.
- **Why the run could not tell:** the first version of the check changed every setting and put all of them back only
  at the end, so MULTI SELECT was tested while PROGRAM CHANGE was still changed — a bias that the plan's "one at a time"
  did not intend (`TASK-AKM-081` corrects it).

## Run 2 of 2026-10-04 (`akm-suite-20261004-101500.log`, not committed)

Same sampler, same declaration (PROGRAM CHANGE ON, MULTI SELECT OFF, MULTI SLCT CH 1A, EXT APM CONTROL 0, AFTERTOUCH
CHANNEL, the AFTERTOUCH filter of channel 1A allowing). With `TASK-AKM-081` each setting was changed and put back
before the next, so MULTI SELECT was tried with PROGRAM CHANGE back to ON. Every item was answered OK then DONE in 10 ms.

| Item | Sent | Owner on the screen |
|---|---|---|
| `&01` Program Change Enable | `04 01 00` (OFF) | **seen** (restored by `04 01 01`) |
| `&02` Multi Select | `04 02 01` (PROG CHANGE), PROGRAM CHANGE on | not seen — screen unchanged |
| `&03` Multi Select Channel | `04 03 01` (2A) | not seen — screen unchanged |
| `&04` External APM Controller | `04 04 01` (1) | not seen — screen unchanged |
| `&05` Aftertouch | `04 05 01` (POLYPHONIC) | not seen — screen unchanged |
| `&07` Ignore filter | `04 07 01 00` (AFTERTOUCH, 1A) | **seen** (restored by `04 06 01 00`) |
| all restored | — | final question "every page shows the declared values again": **no**, without saying which |

The owner's remark that, wherever "no" was answered, the values on the screen had not changed.

## What is established and what is not (after run 2)

- **Refuted:** the dependence of `&02` on PROGRAM CHANGE. It was off in run 1 and on in run 2 and `&02` was not seen either
  time.
- **Established:** the sampler answers DONE to every §04 item, and the screen followed `&01` and `&07` only.
- **Not established — the screen may simply not have been redrawn.** §00/&05 "Enable/Disable automatic screen updating
  when a SysEx message is processed" was off during both runs (the suite leaves it off). Four items "not seen" and the
  final "not back" are what a page that is not redrawn would show; they are also what a sampler that ignores those items
  would show. The log cannot tell the two apart: the owner was never asked to leave the page and open it again.
- **The final "no" is uninformative:** it does not say which page is wrong. After run 2 the real state of the sampler's MIDI
  setup is not known; the owner should look at the pages by hand and put right what differs.

## Run 3 of 2026-10-04 (`akm-suite-20261004-103116.log`, not committed) — with Auto screen update on

After `TASK-AKM-082` the session of the MIDI checks switches Auto screen update (§00/&05) on (`00 05 01` at the open, `00 05 00`
at the close). The owner declared what the sampler held (PROGRAM CHANGE OFF, MULTI SELECT PROG CHANGE, MULTI SLCT CH 1A,
EXT APM CONTROL 0, AFTERTOUCH CHANNEL, the AFTERTOUCH filter of channel 1A allowing). Every item was answered OK then DONE in
10 ms, and **every change was seen on the screen at the first look**; the second look was never needed. All nine checks passed.

| Item | Sent | Owner on the screen |
|---|---|---|
| `&01` Program Change Enable | `04 01 01` (ON) | seen |
| `&02` Multi Select | `04 02 02` (BANK) | seen |
| `&03` Multi Select Channel | `04 03 01` (2A) | seen |
| `&04` External APM Controller | `04 04 01` (1) | seen |
| `&05` Aftertouch | `04 05 01` (POLYPHONIC) | seen |
| `&07` Ignore filter | `04 07 01 00` (AFTERTOUCH, 1A) | seen |
| `&06` Allow filter | `04 06 01 00`, as the restore | seen, indirectly: the final question "every page shows the declared values again" was answered yes, the filter being among them |
| restores of `&01` to `&05` | the declared values | seen, by the same final question |

## What is established (after run 3)

- **All seven §04 items are obeyed by the S5000 (OS 2.14)**, with the values of Table 8 as the spec gives them: `&01` 0-1,
  `&02` 0-2 (OFF, PROG CHANGE, BANK; BANK seen, PROG CHANGE seen as a restore), `&03` channel 2A as code 1, `&04` controller 1,
  `&05` 1 = polyphonic, `&06`/`&07` event type 1 (Aftertouch) on channel code 0 (1A).
- **The sampler does not redraw its MIDI SETUP and MIDI FILTER pages after a SysEx message unless §00/&05 is on.** With it off
  (runs 1 and 2) only `&01` and `&07` showed; with it on all seven did. DONE is no proof of effect in section 04, and
  nor is the screen if §00/&05 is off. An application that changes these settings and wants the sampler's own screen to follow
  must switch §00/&05 on.
- **Corrected:** the first records of runs 1 and 2 above read "not seen" as "the sampler ignores the item", and the
  dependence of `&02` on PROGRAM CHANGE as a possible cause. Neither holds: the sampler had taken the items, the screen had
  not been redrawn.

## Not observed

- The channel code of `&06`/`&07` on the other port: only channel code 0 (1A) was exercised. Whether codes 16 to 31 (1B to
  16B) act on the same channels of port B, or on those of the port the frame arrived on, is not known.
- The other event types (NoteOn, Wheels, Volume) and the effect of a filter on actual MIDI input: only that the setting
  changes on the screen.
- That a Multi Select channel or mode actually selects a multi when MIDI arrives.
