# KB — AKAI S5000/S6000 Operator's Manual, software V1.21 (agent-oriented, dense)

Source: `documents/akai_s5000_s6000_user_manual.1.21.pdf` (292 pdf pages) and its conversion `…pdf.md` (7313 lines, one
PDF line per .md line, no headings, screens and drawings missing). Prefer the generated files below to both.
Ref notation: `pdf pN` = pdf page, `pN`/`doc` = printed page, `mdL` = line in the `.md`, `cL` = line in `user_manual.clean.txt`.
Everything below was transcribed from the manual; `[?]` marks my own inference.

## Lookup recipe
- Never read the PDF, and never read the `.md` or `clean.txt` whole: find the row first, then Read a range.
- Parameter, value, note, index term → `grep -i "cutoff" _index/user_manual.terms.tsv`
  cols: kind term parent desc section chapter pdf doc md clean
  kind: `P` parameter or field (two-column layout "NAME | description") · `V` one value of the parameter in `parent`
  (e.g. `V SINE WAVEFORM`) · `N` NOTE/TIP box (term NOTE or TIP) · `F` footnote (term = its number; refs appear as
  `^N` in clean.txt) · `I` entry of the manual's alphabetical index, one row per page it cites (desc = all its pages;
  doc/md/clean = the line that names it on that page, or on the page next to it, else the page banner).
  Exact kind: `grep -P "^P\t" …` (needs `LC_ALL=C.UTF-8` on Git Bash) or `awk -F'\t' '$1=="P"' …`.
- Section of the manual → `grep -i "filter" _index/user_manual.toc.tsv`
  cols: lvl title path pdf doc md clean — `pdf`/`doc` = page span, `md`/`clean` = line span of the whole section
  (up to the next heading of the same or a higher level): Read that `md` range, or `clean` range for the layout.
- Full context/prose → `_index/user_manual.clean.txt`: page banners `=== pdf pX | doc pY | md La-b | CHAPTER ===`,
  headings `#`/`##`/`###` (levels of the PDF outline, plus bold headings it lacks), columns joined by ` | `.
- Concepts with no name to grep → `grepai search "<english query>"`, then Read the md lines.
- Screens (LCD captures) are images: neither the `.md` nor the index has them. When a screen's layout matters, look at the pdf
  page the row gives (Read the PDF with `pages`).
- Regenerate after a change of the PDF or .md: `python documents/_index/build_manual_index.py` (`pip install pymupdf`, ~1 s).

## Page numbering
- Printed page N = pdf page N+10 (p1 = pdf 11, INTRODUCTION; p281 = pdf 291, last index page).
- pdf 1-2 cover and notice (no number); pdf 3-10 = i-viii (warnings, warranty, contents pdf 7-10); pdf 292 back cover.
- The manual's own index (p277-281) gives printed pages; it is sometimes one page off (terms.tsv points at the page that
  names the term when the next or previous one does).

## Map (chapter → pdf / printed page / mdL / cL); level 2 in `toc.tsv`
| chapter | pdf | p | mdL | cL |
|---|---|---|---|---|
| INTRODUCTION (panels, connections, key conventions, pop-ups, naming, MARK/JUMP, user keys) | 11-33 | 1-23 | 472-980 | 455-921 |
| STRUCTURE (multi/program/keygroup/zone, voice architecture, APM) | 33-39 | 23-29 | 981-1182 | 922-1100 |
| LOAD (folders, autoload, get info, disks, disk tools) | 39-47 | 29-37 | 1183-1340 | 1101-1242 |
| MULTI (parts, windows, multi tools, MIDI remote select, part tools) | 47-90 | 37-80 | 1341-2153 | 1243-1971 |
| EDIT PROGRAM (output, MIDI/tune, pitchbend, LFOs, keygroups, zones, filter, envelopes, prog tools) | 90-151 | 80-141 | 2154-3549 | 1972-3245 |
| EDIT SAMPLE (trim, chop, loop, join, mix, timestretch, pitch shift, BPM, re-sample, EQ, sample tools) | 151-188 | 141-178 | 3550-4394 | 3246-4018 |
| RECORD | 188-196 | 178-186 | 4395-4586 | 4019-4195 |
| FX (ring mod, EQ, modulation, delay, reverb, output mix, copying) | 196-226 | 186-216 | 4587-5273 | 4196-4823 |
| SAVE | 226-229 | 216-219 | 5274-5334 | 4824-4878 |
| UTILITIES (system, clock, prefs, MIDI setup, FX in/outs, disk utils, O/S) | 229-252 | 219-242 | 5335-5771 | 4879-5271 |
| VIRTUAL SAMPLES | 252-260 | 242-250 | 5772-6015 | 5272-5500 |
| SPECIFICATIONS | 260-261 | 250-251 | 6016-6077 | 5501-5548 |
| APPENDIX A MIDI controller list, MIDI implementation chart | 261-263 | 251-253 | 6078-6176 | 5550-5639 |
| APPENDIX B SCSI / hard disks · C PC/Mac transfer · D sampler to Mac/PC via SCSI | 263-275 | 253-265 | 6177-6492 | 5640-5934 |
| APPENDIX E sound library compatibility · F memory · G what is sampling | 275-287 | 265-277 | 6493-6926 | 5935-6342 |
| INDEX | 287-291 | 277-281 | 6927-7314 | 6343-6723 |

## Facts worth knowing (with their source)
- The manual is written for OS V1.20/V1.21 and for the S6000 ("All references will be to the S6000 except where
  specified", p3); the sampler here runs a later OS (2.14) and the SysEx spec is for 2.10, so a feature or range missing
  here may still exist: the SysEx spec (`sysex_spec.kb.md`) wins on codes and ranges, the manual on meaning.
- Structure (p23-25): a program has up to 99 keygroups (p23); each keygroup has four zones (p24); up to 128 multis in
  memory (p25).
- A multi has 32, 64 or 128 parts (chosen at creation, p42; EXPAND MULTI, p56), but only 32 MIDI channels (1-16A, 1-16B;
  footnote 9, p77).
- Polyphony: 128 voices on the S6000, 64 (expandable to 128) on the S5000 (p1, footnote 1); 64 LFOs on an unexpanded
  S5000 (footnote 10, p90). A keygroup with a virtual sample has a polyphony of 1 voice (NOTE, p98).
- EB20 multi-effects board: optional on the S5000 (footnote 7, p37).
- Filter: 26 types (p108), described p114-119 as the 26 `V` rows of `FILTER MODE`, in the order of the SysEx filter mode
  codes 0-25 (`awk -F'\t' '$1=="V" && $3=="FILTER MODE"' _index/user_manual.terms.tsv`).
- `UNDO` has no function in V1.20 (p6).
