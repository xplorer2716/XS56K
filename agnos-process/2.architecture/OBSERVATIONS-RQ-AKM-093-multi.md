# Observations — RQ-AKM-093: the multi check on the real S5000

Not an AGNOS artifact (no ID of its own, like the other `OBSERVATIONS-*` files). What the owner's S5000 (OS 2.14, no
disk drive attached) did with the section 0C items, from `xs56k_akm_probe --suite --multi-lifecycle`, run by the
assistant under the owner's standing authorization of 2026-10-04. Traceability: RQ-AKM-087 to RQ-AKM-093,
TASK-AKM-094 (`FTR-AKM-011`, `PLAN-AKM-011`).

## Run 1 (`akm-suite-20261004-161129.log`, not committed)

The sampler held **no multi and no current multi** (`&51` and `&42` answered ERROR 4, `&50` and `&40` an empty list and
0). The check created its test program (`XS56K_SUITE_TEST`) and its test multi (`XS56K_MULTI_TEST`, index 0) and failed
twice, each time on something the spec left open; each time the guards deleted both test items and the sampler held
exactly what it held before (checked by the second check, which passed).

| Finding | Detail |
|---|---|
| `&47` disagreed with the twelve Gets | Mute read 1 on `&21` right after its Set, then 0 on `&47` after the solo had been set: **setting a part's solo clears its mute** (the Set `&12` = 1 after `&11` = 1). The reverse is not known. |

## Run 2 (`akm-suite-20261004-161416.log`, not committed)

With the check made to compare `&47` with the Sets except for the mute: everything passed until the program number was
cleared.

| Finding | Detail |
|---|---|
| `&31` with the flag alone | `0C 31 00` (flag 0, no number byte) answered **ERROR 2 (parameter out of range)**, although the spec says the number is "only required if Data1=1". |

## Run 3 (`akm-suite-20261004-161454.log`, not committed)

`&31` with the flag off now sends `00 00`. All nine checks passed (the seven automatic ones, the multi check and the
check that fails half way), the sampler was left in the known state, and `0 multi(s)` before and after.

| Item | Sent | Sampler's answer |
|---|---|---|
| `&02` Create Multi | `XS56K_MULTI_TEST` | DONE; the multi is current at index 0 |
| `&44` Parts of the current multi | — | REPLY `1F` (31, so 32 parts: the sampler's own setting for new multis) |
| `&40`, `&42`, `&43`, `&51`, `&52`, `&50` | — | count 1, index 0, the name, one name, `1F`, `00 00` (program number off) |
| `&10`-`&1B` / `&20`-`&2B` on part 3 | MIDI channel 5, mute 1, solo 1, level 80, output 9, pan 100, FX channel 3, FX send 40, fine tune 60, transpose 48, low note 36, high note 96 | DONE each; each Get reads the value set, but the mute (see run 1) |
| `&47` on part 3 | — | REPLY of 12 bytes: `05 00 01 50 09 64 03 28 3C 30 24 60` (twelve values, in the order of `&20`-`&2B`) |
| `&48` | — | 32 values; with the mute alone on part 3 reads 1, with the solo alone 2, nothing 0 |
| `&31` | `01 04`, then `00 00` | DONE; `&41` reads 5 (4 on the wire), then off |
| `&33` and `&34` | the test program on part 3 by name, then delete | DONE; `&45` reads the name, then empty (`00`); `&46` returns 32 names |
| `&32` | the program at index 0 on part 4 | DONE; `&45` reads the test program's name |
| `&33` with a program no memory holds | — | ERROR 4 |
| `&30` | rename to `XS56K_MULTI_TEST2` and back | DONE; `&43` follows |
| `&06`, `&05` | an index past the end; a name no multi has | ERROR 4 both |
| `&08` | the test multi, selected again by index | DONE (the guard) |

Not sent, by design: `&07` (Delete ALL Multis) and `&01` (number of parts for new multis).

## What was established

- **Established:** every item of §0C that the check sends is obeyed by the S5000 and reads back as the spec's Table 17
  says, `&44`'s `1F` included. ERROR 4 is the answer for a name or an index that names nothing, for an empty memory on
  the all-multis Gets (`&51` read as an empty list) and for "the current multi" Gets with none current.
- **Two things the spec does not say, observed:** setting a part's solo clears its mute; and `&31` needs its number byte
  even when switching the number off (`00 00`). Both are now in the code and in `ctest` (`MultiEditingTests.cpp`, the
  simulated sampler).
- **Not established:** the effect of `&01` (never sent); mute set after solo; the answers for a part number beyond the
  multi's size; whether the multi's program number given by `&31` is shown on the front panel; behaviour with
  other multis in memory (the sampler held none: selection of an existing multi by index and by name was only
  exercised on the test multi).
