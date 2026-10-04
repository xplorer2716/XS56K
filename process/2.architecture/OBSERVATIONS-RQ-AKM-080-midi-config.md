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

## To settle on the next run (after `TASK-AKM-082`)

The session of the MIDI checks now opens with Auto screen update on (§00/&05, put back off by the close), and a "no" is
followed by a second look after the owner leaves the page and opens it again. Three outcomes per item: seen at once (the
item works and the screen redraws); seen only after re-opening the page (the item works, the screen did not redraw
by itself); still not seen after re-opening (the sampler ignores the item). Only the last one is a finding about the item.
