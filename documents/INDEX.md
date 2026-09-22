# Documents index

Reference material for XS56K. Keep this list up to date when a document is added or removed.

## Reference documents

| Document | Format | Version / date | Pages | Content |
|---|---|---|---|---|
| [akai_s5000_s6000_sysex_spec_2.10.pdf](akai_s5000_s6000_sysex_spec_2.10.pdf) | PDF (original) | SysEx spec for OS 2.10 (PDF created 2001-01-30) | 46 (numbered 1–42 plus front matter) | MIDI System Exclusive protocol: message framing, checksums, confirmation and error messages, every Section/Item (system, MIDI, keygroup zone, keygroup, program, multi, sample, disk, multi FX, scenelist, song files, front panel), alternative and blocked sections. |
| [akai_s5000_s6000_sysex_spec_2.10.pdf.md](akai_s5000_s6000_sysex_spec_2.10.pdf.md) | Markdown (converted from the PDF) | same | — | Searchable text of the SysEx spec. Some table layouts are broken; check the PDF (or `_index/sysex_spec.clean.txt`) for table values. |
| [akai_s5000_s6000_user_manual.1.21.pdf](akai_s5000_s6000_user_manual.1.21.pdf) | PDF (original) | Operator's manual, software V1.21 (PDF created 1999-08-24) | 292 | Sampler operation: introduction, structure, load, multi, edit program, edit sample, record, FX, save, utilities, virtual samples, specifications, appendices. |
| [akai_s5000_s6000_user_manual.1.21.pdf.md](akai_s5000_s6000_user_manual.1.21.pdf.md) | Markdown (converted from the PDF) | same | — | Searchable text of the operator's manual. Same caveat about tables. |

## Generated index (`_index/`)

Written for AI agents, generated from the SysEx spec PDF. Don't edit the generated files by hand: re-run the script.

| File | Purpose |
|---|---|
| [_index/sysex_spec.kb.md](_index/sysex_spec.kb.md) | Condensed knowledge base: framing, checksum, errors, sections, encodings, state model, value codes, FX tables, keycodes, spec errata, topic → page / `.md` line map. Hand-written from the spec. |
| [_index/sysex_spec.items.tsv](_index/sysex_spec.items.tsv) | One row per Item (560 rows) from every Control and REPLY table: section, kind, item code, RT flag, data ranges, description, sub-group, table, PDF page, printed page, `.md` line. Table footnotes are at the end (`#FN`). Generated. |
| [_index/sysex_spec.clean.txt](_index/sysex_spec.clean.txt) | Text of the whole spec extracted with its layout kept (table columns separated by ` \| `, page banners with `.md` line ranges). Generated. |
| [_index/build_sysex_index.py](_index/build_sysex_index.py) | Rebuilds the two generated files (`pip install pymupdf`, then `python3 documents/_index/build_sysex_index.py` from the repo root). |

The operator's manual is not indexed in `_index/` yet.
