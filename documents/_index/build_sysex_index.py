#!/usr/bin/env python3
"""Regenerate the machine index of the AKAI S5000/S6000 SysEx spec.

Reads  : documents/akai_s5000_s6000_sysex_spec_2.10.pdf (+ its .md for line anchors)
Writes : documents/_index/sysex_spec.clean.txt  (layout-faithful text, columns split by " | ")
         documents/_index/sysex_spec.items.tsv  (one row per <Item> of every Control/REPLY table)
Needs  : pip install pymupdf
Run    : python3 documents/_index/build_sysex_index.py   (from the repo root)
"""
import re
from pathlib import Path

import pymupdf

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "documents"
PDF = DOCS / "akai_s5000_s6000_sysex_spec_2.10.pdf"
MD = DOCS / "akai_s5000_s6000_sysex_spec_2.10.pdf.md"
OUT = DOCS / "_index"
PDF_TO_DOC_OFFSET = 4  # printed "Page N/42" == PDF page N+4

ITEM_RE = re.compile(r"^(&[0-9A-F]{2}(?:–&[0-9A-F]{2})?)(\{[0-9–]+\})?((?:RT)?[a-e]{0,2}(?:,[a-e])?)$")
TABLE_RE = re.compile(r"^Table (\d+): (.*)$")
SECTION_RE = re.compile(r"&([0-9A-F]{2}) ?\{(\d+)\}")


def md_page_ranges():
    """doc page -> (first md line, last md line), 1-based, from the 'Page N/42' footers."""
    ends = {}
    for i, line in enumerate(MD.read_text(encoding="utf-8").splitlines(), 1):
        m = re.search(r"Page (\d+)/42", line)
        if m:
            ends[int(m.group(1))] = i
    ranges, prev = {}, 84  # front matter (cover, TOC, list of tables) ends at md line 84
    for n in sorted(ends):
        ranges[n] = (prev + 1, ends[n])
        prev = ends[n]
    return ranges


def page_rows(page):
    """Group words into visual rows; return dicts with x0,x1,y0,y1,text (columns joined by ' | ')."""
    rows = []
    for w in page.get_text("words"):
        for r in rows:
            if abs(r["yb"] - w[3]) <= 2:
                r["w"].append(w)
                break
        else:
            rows.append({"yb": w[3], "w": [w]})
    out = []
    for r in sorted(rows, key=lambda r: r["yb"]):
        ws = sorted(r["w"], key=lambda w: w[0])
        parts, prev = [], None
        for w in ws:
            if prev is not None:
                parts.append(" | " if w[0] - prev > 12 else " ")
            parts.append(w[4])
            prev = w[2]
        out.append({"x0": ws[0][0], "x1": max(w[2] for w in ws),
                    "y0": min(w[1] for w in ws), "y1": max(w[3] for w in ws),
                    "text": "".join(parts)})
    return out


def verticals(page):
    segs = []
    for d in page.get_drawings():
        r = d["rect"]
        if r.width <= 2.5 and r.height > 5:
            segs.append((r.x0, r.y0, r.y1))
    return segs


def is_group_heading(row, segs):
    """Sub-group rows inside a table interrupt the inner column rules."""
    if " | " in row["text"] or row["text"].startswith("&"):
        return False
    ym = (row["y0"] + row["y1"]) / 2
    near = [s for s in segs if s[1] - 40 < ym < s[2] + 40]
    if not near:
        return False
    xs = sorted({round(s[0]) for s in near})
    inner = [x for x in xs if xs[0] + 20 < x < xs[-1] - 20]
    if not inner:
        return False
    # heading row: at least one inner column rule is interrupted at this height
    return any(not any(s[1] <= ym <= s[2] for s in near if round(s[0]) == x) for x in inner)


def main():
    doc = pymupdf.open(PDF)
    md_ranges = md_page_ranges()
    md_lines = MD.read_text(encoding="utf-8").splitlines()

    clean, items = [], []
    table = None          # (num, title, section_hex, kind)
    group, pending_fn, cur = "", "", None
    footnotes = {}        # table num -> list of footnote texts

    for pi, page in enumerate(doc, 1):
        docp = pi - PDF_TO_DOC_OFFSET
        rng = md_ranges.get(docp)
        clean.append(f"=== pdf p{pi} | doc p{docp if docp > 0 else '-'} | md L{rng[0]}-{rng[1]} ===" if rng
                     else f"=== pdf p{pi} | front matter ===")
        segs = verticals(page)
        for row in page_rows(page):
            t = row["text"].strip()
            if t.startswith("AKAI S5000/S6000 MIDI System Exclusive") or re.match(r"^(Page \d+/42 \| Version|Version 2·10 \| Page)", t):
                continue
            heading = bool(table) and table[3] != "-" and row["x0"] >= 150 and is_group_heading(row, segs)
            clean.append(t)

            m = TABLE_RE.match(t)
            if m:
                num, title = int(m.group(1)), m.group(2)
                sm = SECTION_RE.search(title)
                kind = "R" if "REPLY" in title else ("C" if "Control Items" in title else "-")
                if table is None or table[0] != num:
                    group = ""
                sec = sm.group(1) if sm else ""
                if num == 29:  # spec erratum: Table 29 title says &14{20}, it is the MIDI Song File section &16{22}
                    sec = "16"
                table = (num, title, sec, kind)
                cur = None
                continue
            if table is None or table[3] == "-":
                continue
            if t.startswith("<Item>"):
                continue
            if row["x0"] < 85 and not t.startswith("&"):   # body text => table finished
                table, cur = None, None
                continue
            fm = re.match(r"^([a-e])\. (.*)", t)
            if fm and row["x0"] < 125:
                footnotes.setdefault(table[0], []).append(f"{fm.group(1)}: {fm.group(2)}")
                cur = ("fn", table[0])
                continue
            if isinstance(cur, tuple):                      # footnote continuation
                footnotes[cur[1]][-1] += " " + t
                continue
            if re.fullmatch(r"[a-e]", t):
                pending_fn += t
                continue
            if heading:
                if not t.startswith("(Note"):
                    group = t
                clean[-1] = "## " + t   # sub-group heading inside a table
                cur = None
                continue
            cols = t.split(" | ")
            im = ITEM_RE.match(cols[0])
            if im:
                code = im.group(1)
                dec = (im.group(2) or "").strip("{}")
                flags = im.group(3) or ""
                rt = "RT" if flags.startswith("RT") else ""
                fn = pending_fn + flags.replace("RT", "").replace(",", "")
                pending_fn = ""
                data = cols[1:-1]
                desc = cols[-1] if len(cols) > 1 else ""
                # md anchor: first line in the page range containing the item code
                md_at = ""
                if rng:
                    for ln in range(rng[0], rng[1] + 1):
                        if code + "{" in md_lines[ln - 1] or code + " " in md_lines[ln - 1]:
                            if any(wd in md_lines[ln - 1] for wd in desc.split()[:3] if len(wd) > 3) or not desc:
                                md_at = str(ln)
                                break
                    if not md_at:  # fallback: item code alone
                        md_at = next((str(ln) for ln in range(rng[0], rng[1] + 1)
                                      if code + "{" in md_lines[ln - 1]), f"{rng[0]}-{rng[1]}")
                cur = {"sec": table[2], "tbl": table[0], "kind": table[3], "item": code, "dec": dec,
                       "rt": rt, "fn": fn, "d1": data[0] if len(data) > 0 else "",
                       "d2": " ; ".join(data[1:]), "desc": desc, "group": group,
                       "pdf": pi, "doc": docp, "md": md_at}
                items.append(cur)
                continue
            if isinstance(cur, dict):                       # continuation of current item
                parts = t.split(" | ")
                extra, text = parts[:-1], parts[-1]
                if len(parts) == 1 and re.match(r"^(<Data\d|\(range|\(OFF|\(MSB|\(handle|\{\d|<Data\d…)", t) and row["x0"] < 270:
                    extra, text = [t], ""
                if extra:
                    cur["d2"] = (cur["d2"] + " ; " if cur["d2"] else "") + " ; ".join(extra)
                if text:
                    cur["desc"] += " " + text

    OUT.mkdir(exist_ok=True)
    (OUT / "sysex_spec.clean.txt").write_text("\n".join(clean) + "\n", encoding="utf-8")
    cols = ["sec", "kind", "item", "dec", "rt", "fn", "d1", "d2", "desc", "group", "tbl", "pdf", "doc", "md"]
    lines = ["\t".join(cols)]
    for it in items:
        lines.append("\t".join(str(it[c]).replace("\t", " ") for c in cols))
    lines.append("")
    lines.append("#FOOTNOTES\ttbl\tletter: text")
    for num in sorted(footnotes):
        for f in footnotes[num]:
            lines.append(f"#FN\t{num}\t{f}")
    (OUT / "sysex_spec.items.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{len(items)} items, {sum(len(v) for v in footnotes.values())} footnotes")


if __name__ == "__main__":
    main()
