#!/usr/bin/env python3
"""Regenerate the machine index of the AKAI S5000/S6000 SysEx spec.

Reads  : documents/akai_s5000_s6000_sysex_spec_2.10.pdf (+ its .md for line anchors)
Writes : documents/_index/sysex_spec.clean.txt  (layout-faithful text, columns split by " | ")
         documents/_index/sysex_spec.items.tsv  (one row per <Item> of every Control/REPLY table)
Needs  : pip install pymupdf
Run    : python3 documents/_index/build_sysex_index.py   (from the repo root)

Traceability (pdf_md_indexing_guide.md): implements FTR-DIX-001 - RQ-DIX-001, RQ-DIX-003, RQ-DIX-004, RQ-DIX-007,
RQ-DIX-008, RQ-DIX-009, RQ-DIX-011, RQ-DIX-012, RQ-DIX-016, RQ-DIX-020 - after ADR-DIX-001 to ADR-DIX-003 (DEC-DIX-001 to
DEC-DIX-004, DEC-DIX-006, DEC-DIX-008, DEC-DIX-011). RQ-DIX-014 is sysex_spec.kb.md, RQ-DIX-015 documents/INDEX.md and
AGENTS.md. Written before the guide; its known gaps against it:
- RQ-DIX-002: printed pages come from the fixed PDF_TO_DOC_OFFSET, which the run does not verify against the footers.
- RQ-DIX-005, RQ-DIX-006: no headings are marked and no toc.tsv is written (the tables carry the structure).
- RQ-DIX-013, RQ-DIX-017: the summary line gives no anchor count, and md_anchor searches a page range by item code
  instead of the ordered cursor of DEC-DIX-009.
- RQ-DIX-019: write_text() is called without a newline argument, so a run on Windows writes CRLF."""
import re
from pathlib import Path

import pymupdf

# RQ-DIX-001, DEC-DIX-001/DEC-DIX-004
ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "documents"
PDF = DOCS / "akai_s5000_s6000_sysex_spec_2.10.pdf"
MD = DOCS / "akai_s5000_s6000_sysex_spec_2.10.pdf.md"
OUT = DOCS / "_index"
# RQ-DIX-002 (gap: not verified by the run)
PDF_TO_DOC_OFFSET = 4  # printed "Page N/42" == PDF page N+4

ITEM_RE = re.compile(r"^(&[0-9A-F]{2}(?:–&[0-9A-F]{2})?)(\{[0-9–]+\})?((?:RT)?[a-e]{0,2}(?:,[a-e])?)$")
TABLE_RE = re.compile(r"^Table (\d+): (.*)$")
SECTION_RE = re.compile(r"&([0-9A-F]{2}) ?\{(\d+)\}")


# RQ-DIX-003
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


# RQ-DIX-004, DEC-DIX-006
def add_word_to_rows(rows, word):
    """Puts a word on the first visual row whose baseline is within 2 points of its own, or starts a new row."""
    for r in rows:
        if abs(r["yb"] - word[3]) <= 2:
            r["w"].append(word)
            return
    rows.append({"yb": word[3], "w": [word]})


# RQ-DIX-004, DEC-DIX-006
def build_row(words):
    """One visual row from its words: x0,x1,y0,y1 and the text, columns joined by ' | '."""
    ws = sorted(words, key=lambda w: w[0])
    parts, prev = [], None
    for w in ws:
        if prev is not None:
            parts.append(" | " if w[0] - prev > 12 else " ")
        parts.append(w[4])
        prev = w[2]
    return {"x0": ws[0][0], "x1": max(w[2] for w in ws),
            "y0": min(w[1] for w in ws), "y1": max(w[3] for w in ws),
            "text": "".join(parts)}


# RQ-DIX-004, DEC-DIX-006
def page_rows(page):
    """Group words into visual rows; return dicts with x0,x1,y0,y1,text (columns joined by ' | ')."""
    rows = []
    for w in page.get_text("words"):
        add_word_to_rows(rows, w)
    return [build_row(r["w"]) for r in sorted(rows, key=lambda r: r["yb"])]


# DEC-DIX-008: the table rules, which tell the sub-group rows apart
def verticals(page):
    segs = []
    for d in page.get_drawings():
        r = d["rect"]
        if r.width <= 2.5 and r.height > 5:
            segs.append((r.x0, r.y0, r.y1))
    return segs


# RQ-DIX-007, DEC-DIX-008
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


# RQ-DIX-004
def is_page_furniture(text):
    """The running header and footer of a page, which are not part of any table."""
    return text.startswith("AKAI S5000/S6000 MIDI System Exclusive") or bool(
        re.match(r"^(Page \d+/42 \| Version|Version 2·10 \| Page)", text))


# DEC-DIX-008: one pass over the rows, one current table and item
class IndexBuilder:
    """Reads the spec's rows page by page and keeps what it has seen of the current table: the table itself,
    the sub-group, the footnote letters waiting for their item, and the item or footnote being continued."""

    def __init__(self, md_ranges, md_lines):
        self.md_ranges = md_ranges
        self.md_lines = md_lines
        self.clean, self.items = [], []
        self.table = None          # (num, title, section_hex, kind)
        self.group, self.pending_fn, self.cur = "", "", None
        self.footnotes = {}        # table num -> list of footnote texts

    def add_page(self, pi, page):
        docp = pi - PDF_TO_DOC_OFFSET
        rng = self.md_ranges.get(docp)
        if rng:
            doc_label = docp if docp > 0 else "-"
            self.clean.append(f"=== pdf p{pi} | doc p{doc_label} | md L{rng[0]}-{rng[1]} ===")
        else:
            self.clean.append(f"=== pdf p{pi} | front matter ===")
        segs = verticals(page)
        for row in page_rows(page):
            self.process_row(row, segs, (pi, docp, rng))

    def process_row(self, row, segs, where):
        t = row["text"].strip()
        if is_page_furniture(t):
            return
        heading = bool(self.table) and self.table[3] != "-" and row["x0"] >= 150 and is_group_heading(row, segs)
        self.clean.append(t)
        if self.start_table(t):
            return
        if self.table is None or self.table[3] == "-":
            return
        self.process_table_row(t, row, heading, where)

    def process_table_row(self, t, row, heading, where):
        if t.startswith("<Item>"):
            return
        if row["x0"] < 85 and not t.startswith("&"):   # body text => table finished
            self.table, self.cur = None, None
            return
        if self.footnote_line(t, row):
            return
        if heading:
            self.group_heading(t)
            return
        if self.item_start(t, where):
            return
        self.continue_item(t, row)

    # RQ-DIX-007, RQ-DIX-012
    def start_table(self, t):
        """A "Table N: ..." line: becomes the current table; True when the line was one."""
        m = TABLE_RE.match(t)
        if not m:
            return False
        num, title = int(m.group(1)), m.group(2)
        sm = SECTION_RE.search(title)
        if "REPLY" in title:
            kind = "R"
        elif "Control Items" in title:
            kind = "C"
        else:
            kind = "-"
        if self.table is None or self.table[0] != num:
            self.group = ""
        sec = sm.group(1) if sm else ""
        if num == 29:  # RQ-DIX-012, DEC-DIX-011 - spec erratum: Table 29 title says &14{20}, it is the MIDI Song File section &16{22}
            sec = "16"
        self.table = (num, title, sec, kind)
        self.cur = None
        return True

    # RQ-DIX-009
    def footnote_line(self, t, row):
        """A footnote, its continuation, or a bare footnote letter belonging to the next item; True when the line was one."""
        fm = re.match(r"^([a-e])\. (.*)", t)
        if fm and row["x0"] < 125:
            self.footnotes.setdefault(self.table[0], []).append(f"{fm.group(1)}: {fm.group(2)}")
            self.cur = ("fn", self.table[0])
            return True
        if isinstance(self.cur, tuple):                      # footnote continuation
            self.footnotes[self.cur[1]][-1] += " " + t
            return True
        if re.fullmatch(r"[a-e]", t):
            self.pending_fn += t
            return True
        return False

    def group_heading(self, t):
        if not t.startswith("(Note"):
            self.group = t
        self.clean[-1] = "## " + t   # sub-group heading inside a table
        self.cur = None

    # RQ-DIX-011, DEC-DIX-003 (deviates from DEC-DIX-009: search by code, no ordered cursor)
    def md_anchor(self, code, desc, rng):
        """The md line of an item: the first line of the page range holding its code (and one of the first words of
        its description), else the first line holding the code alone, else the whole range."""
        if not rng:
            return ""
        for ln in range(rng[0], rng[1] + 1):
            if code + "{" in self.md_lines[ln - 1] or code + " " in self.md_lines[ln - 1]:
                if any(wd in self.md_lines[ln - 1] for wd in desc.split()[:3] if len(wd) > 3) or not desc:
                    return str(ln)
        return next((str(ln) for ln in range(rng[0], rng[1] + 1)
                     if code + "{" in self.md_lines[ln - 1]), f"{rng[0]}-{rng[1]}")

    # RQ-DIX-007, DEC-DIX-003
    def item_start(self, t, where):
        """An item row: becomes the current item; True when the line was one."""
        pi, docp, rng = where
        cols = t.split(" | ")
        im = ITEM_RE.match(cols[0])
        if not im:
            return False
        code = im.group(1)
        dec = (im.group(2) or "").strip("{}")
        flags = im.group(3) or ""
        rt = "RT" if flags.startswith("RT") else ""
        fn = self.pending_fn + flags.replace("RT", "").replace(",", "")
        self.pending_fn = ""
        data = cols[1:-1]
        desc = cols[-1] if len(cols) > 1 else ""
        self.cur = {"sec": self.table[2], "tbl": self.table[0], "kind": self.table[3], "item": code, "dec": dec,
                    "rt": rt, "fn": fn, "d1": data[0] if len(data) > 0 else "",
                    "d2": " ; ".join(data[1:]), "desc": desc, "group": self.group,
                    "pdf": pi, "doc": docp, "md": self.md_anchor(code, desc, rng)}
        self.items.append(self.cur)
        return True

    # RQ-DIX-008
    def continue_item(self, t, row):
        """A row that goes on with the current item's data columns or description."""
        if not isinstance(self.cur, dict):
            return
        parts = t.split(" | ")
        extra, text = parts[:-1], parts[-1]
        if len(parts) == 1 and re.match(r"^(<Data\d|\(range|\(OFF|\(MSB|\(handle|\{\d|<Data\d…)", t) and row["x0"] < 270:
            extra, text = [t], ""
        if extra:
            self.cur["d2"] = (self.cur["d2"] + " ; " if self.cur["d2"] else "") + " ; ".join(extra)
        if text:
            self.cur["desc"] += " " + text

    # RQ-DIX-001, DEC-DIX-002 (gap RQ-DIX-019: write_text without newline, CRLF on Windows)
    def write_outputs(self):
        OUT.mkdir(exist_ok=True)
        (OUT / "sysex_spec.clean.txt").write_text("\n".join(self.clean) + "\n", encoding="utf-8")
        cols = ["sec", "kind", "item", "dec", "rt", "fn", "d1", "d2", "desc", "group", "tbl", "pdf", "doc", "md"]
        lines = ["\t".join(cols)]
        for it in self.items:
            lines.append("\t".join(str(it[c]).replace("\t", " ") for c in cols))
        lines.append("")
        lines.append("#FOOTNOTES\ttbl\tletter: text")
        for num in sorted(self.footnotes):
            for f in self.footnotes[num]:
                lines.append(f"#FN\t{num}\t{f}")
        (OUT / "sysex_spec.items.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"{len(self.items)} items, {sum(len(v) for v in self.footnotes.values())} footnotes")


def main():
    doc = pymupdf.open(PDF)
    builder = IndexBuilder(md_page_ranges(), MD.read_text(encoding="utf-8").splitlines())
    for pi, page in enumerate(doc, 1):
        builder.add_page(pi, page)
    builder.write_outputs()


if __name__ == "__main__":
    main()
