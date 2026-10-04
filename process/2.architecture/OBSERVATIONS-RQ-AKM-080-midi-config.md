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

## What is established and what is not

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

## To settle on the next run (after `TASK-AKM-081`)

Each setting is now changed and put back before the next one is touched, and a "no" no longer stops the check. With
PROGRAM CHANGE back to ON when MULTI SELECT is tried: if the owner then sees PROG CHANGE, the dependence of `&02` on
`&01` is confirmed; if not, `&02` does not do what Table 8 says on this sampler. The owner may also try PROG CHANGE
by hand on the sampler with PROGRAM CHANGE OFF, to see whether the sampler itself refuses it.
