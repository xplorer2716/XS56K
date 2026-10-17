#!/usr/bin/env python3
"""Regenerate the machine index of the AKAI S5000/S6000 operator's manual (software V1.21).

Reads  : documents/akai_s5000_s6000_user_manual.1.21.pdf (+ its .md for line anchors)
Writes : documents/_index/user_manual.clean.txt  (layout-faithful text, columns split by " | ", headings as #/##/###,
                                                  footnote references as ^N)
         documents/_index/user_manual.toc.tsv    (one row per heading: level, path, page span, md and clean line span)
         documents/_index/user_manual.terms.tsv  (one row per parameter, parameter value, NOTE/TIP, footnote and
                                                  back-of-book index entry, each with its section, pages and lines)
Needs  : pip install pymupdf
Run    : python3 documents/_index/build_manual_index.py   (from the repo root)

Traceability (pdf_md_indexing_guide.md): implements FTR-DIX-001 - RQ-DIX-001 to RQ-DIX-011, RQ-DIX-013, RQ-DIX-016 to
RQ-DIX-021 - after ADR-DIX-001 to ADR-DIX-003 (DEC-DIX-001 to DEC-DIX-010). RQ-DIX-014 is user_manual.kb.md, RQ-DIX-015
documents/INDEX.md and AGENTS.md. RQ-DIX-012: no erratum of the manual is known, so no exception is coded.
"""
import bisect
import re
from pathlib import Path

import pymupdf

# RQ-DIX-001, DEC-DIX-001/DEC-DIX-004: the PDF gives the structure, the .md the line addresses; outputs in _index/
ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "documents"
PDF = DOCS / "akai_s5000_s6000_user_manual.1.21.pdf"
MD = DOCS / "akai_s5000_s6000_user_manual.1.21.pdf.md"
OUT = DOCS / "_index"
# DEC-DIX-005: thresholds measured on this manual's profile (10 pt body text), not defaults
FIRST_BODY_PDF = 11      # printed page 1 (INTRODUCTION); before it: cover, warnings, contents
HEADER_Y, FOOTER_Y = 60, 780
TERM_X = (80, 125)       # x of a parameter name in the two-column layout; its description starts at x >= DESC_MIN_X
DESC_MIN_X = 150
FOOTNOTE_X = (120, 160)  # x of a footnote's text, after its number (at TERM_X)
FOOTNOTE_MIN_Y = 450     # footnotes sit in the lower part of a page
COL_GAP = 12             # a horizontal gap wider than this between two spans is a column break
NOTE_RE = re.compile(r"^(NOTE|TIP)S?\b:?")
ROMAN_RE = re.compile(r"^[ivx]+$")
PAGE_NUM_RE = re.compile(r"^\d+,?$")
INDEX_NOISE = {"page", "pages", "window", "windows", "menu", "prompt", "key", "keys"}  # words an index entry adds


# DEC-DIX-009: the anchoring key (Latin script only: see the guide's Domain of validity)
def norm(text):
    """Lower-case letters and digits only: the key used to match a PDF line with an .md line or a title."""
    return re.sub(r"[^0-9a-z]", "", text.lower())


# RQ-DIX-007, DEC-DIX-008
def is_term(text):
    """A parameter or value name: short, mostly upper case, not a sentence."""
    letters = [c for c in text if c.isalpha()]
    if not letters or len(text) > 40 or text.endswith((".", ":", ",")):
        return False
    return sum(c.isupper() for c in letters) / len(letters) >= 0.8


# RQ-DIX-008
def join_text(a, b):
    """Joins two lines of text, gluing back a word that a hyphen broke at the end of the first."""
    if not a:
        return b
    if a.endswith("-") and len(a) > 1 and a[-2].isalpha() and b[:1].islower():
        return a[:-1] + b
    return a + " " + b


# RQ-DIX-010
def split_index_entry(text):
    """'Goto Dest 85, 91, 94' -> ('Goto Dest', [85, 91, 94]); a line of page numbers only -> ('', [...])."""
    words = text.split()
    i = len(words)
    while i > 0 and PAGE_NUM_RE.match(words[i - 1]):
        i -= 1
    return " ".join(words[:i]), [int(w.rstrip(",")) for w in words[i:]]


# ---- .md pages ---------------------------------------------------------------------------------------------------
# RQ-DIX-003, DEC-DIX-009
def md_page_ranges(md_lines, pdf_has_header):
    """pdf page -> (first md line, last md line), 1-based. Every page of the .md starts after a blank line with the
    running header ("s5000/s6000 CHAPTER"); a pdf page without a header (cover, notice, back cover) takes the next
    block that has none."""
    starts = [1] + [i + 1 for i in range(1, len(md_lines)) if not md_lines[i - 1].strip() and md_lines[i].strip()]
    with_header = iter(s for s in starts if "s5000/s6000" in md_lines[s - 1])
    without = [s for s in starts if "s5000/s6000" not in md_lines[s - 1]]
    page_starts = []
    for has in pdf_has_header:
        prev = page_starts[-1] if page_starts else 0
        start = next(with_header, prev) if has else next((s for s in without if s > prev), prev)
        page_starts.append(start)
    ends = [s - 1 for s in page_starts[1:]] + [len(md_lines)]
    return {pi: (s, max(e, s)) for pi, (s, e) in enumerate(zip(page_starts, ends), 1)}


# ---- rows of a pdf page ------------------------------------------------------------------------------------------
# DEC-DIX-006: TEXTFLAGS_TEXT leaves the image blocks out
def page_spans(page):
    """The page's text spans, (x0, y0, x1, y1, text, font, size), without the empty ones."""
    spans = []
    for b in page.get_text("dict", flags=pymupdf.TEXTFLAGS_TEXT)["blocks"]:
        for line in b.get("lines", []):
            spans += [(*s["bbox"], s["text"], s["font"], s["size"]) for s in line["spans"] if s["text"].strip()]
    return spans


# RQ-DIX-009, DEC-DIX-006
def is_superscript(span):
    """A footnote reference: a small figure set in Geneva, above the baseline of its line."""
    return span[5].startswith("Geneva") and span[6] < 8


# RQ-DIX-004, RQ-DIX-009, DEC-DIX-006
def build_row(spans):
    """One visual row from its spans: segments [x0, text, bold, size] split at column gaps, the joined text, and the
    text without footnote references (the key matched with the .md)."""
    segs, plain, prev_x1 = [], [], None
    for x0, _y0, x1, _y1, text, font, size in sorted(spans, key=lambda s: s[0]):
        sup = font.startswith("Geneva") and size < 8
        if sup and segs:
            segs[-1][1] += "^" + text.strip()
            continue
        if prev_x1 is None or x0 - prev_x1 > COL_GAP:
            segs.append([x0, text.strip(), "Bold" in font, size])
            plain.append(text.strip())
        else:
            sep = " " if x0 - prev_x1 > 1 else ""
            segs[-1][1] = (segs[-1][1] + sep + text).strip()
            plain[-1] = (plain[-1] + sep + text).strip()
        prev_x1 = x1 if prev_x1 is None else max(x1, prev_x1)
    segs = [(x, re.sub(r"\s+", " ", t), b, sz) for x, t, b, sz in segs]
    return {"y": min(s[1] for s in spans), "x0": segs[0][0], "segs": segs, "text": " | ".join(s[1] for s in segs),
            "key": " ".join(plain)}


# RQ-DIX-004, DEC-DIX-006
def page_rows(spans):
    """Group spans into visual rows (baselines within 2 points), top to bottom; a footnote reference joins the row
    whose height holds it."""
    rows = []
    for s in sorted((s for s in spans if not is_superscript(s)), key=lambda s: s[3]):
        if rows and abs(rows[-1][-1][3] - s[3]) <= 2:
            rows[-1].append(s)
        else:
            rows.append([s])
    for s in filter(is_superscript, spans):
        row = next((r for r in rows if min(t[1] for t in r) - 3 <= s[1] <= max(t[3] for t in r)), None)
        if row is None:
            rows.append([s])
        else:
            row.append(s)
    return [build_row(r) for r in sorted(rows, key=lambda r: min(t[1] for t in r))]


# RQ-DIX-002, RQ-DIX-004
def page_furniture(spans):
    """The running header's chapter name and the footer's page label of a page ('' when the page has none)."""
    chapter = " ".join(s[4].strip() for s in spans if s[1] < HEADER_Y and "Bold" in s[5]).strip()
    labels = (s[4].strip() for s in spans if s[1] > FOOTER_Y)
    return chapter, next((t for t in labels if t.isdigit() or ROMAN_RE.match(t)), "")


# DEC-DIX-009, RQ-DIX-011, RQ-DIX-017
class Pager:
    """Matches the rows of one pdf page with the lines of its .md page, in order."""

    def __init__(self, md_lines, rng):
        self.md_lines, self.rng = md_lines, rng
        self.cursor = rng[0]
        self.matched = self.total = 0

    def anchor(self, text):
        """The first .md line from the cursor on that starts like the row; the cursor itself when none does."""
        key = norm(text)[:20]
        self.total += 1
        for ln in range(self.cursor, self.rng[1] + 1):
            if key and norm(self.md_lines[ln - 1]).startswith(key):
                self.cursor = ln
                self.matched += 1
                return ln
        return self.cursor


# DEC-DIX-008: one pass over the rows, one current entity carried across pages
class IndexBuilder:
    """Reads the manual page by page: writes its clean text, and keeps the headings, the parameters with their values,
    the notes and tips, the footnotes and the entries of the alphabetical index at the end of the manual."""

    def __init__(self, doc, md_lines):
        self.doc, self.md_lines = doc, md_lines
        self.outline = {}
        for lvl, title, pg in doc.get_toc():
            self.outline.setdefault(pg, []).append([lvl, title, False])
        self.clean, self.headings, self.terms, self.index_rows = [], [], [], []
        self.page_clean = {}     # pdf page -> (banner line, last clean line)
        self.labels = {}         # pdf page -> printed page label
        self.md_ranges = {}
        self.anchors = [0, 0]    # rows matched with an .md line, rows
        self.cur = None          # the parameter (P), value (V), note (N) or footnote (F) being continued
        self.footnote = 0        # the number of the last footnote: they are numbered 1, 2, ... through the manual
        self.chapter = ""

    # ---- page loop ----------------------------------------------------------------------------------------------
    def build(self):
        spans_by_page = [page_spans(p) for p in self.doc]
        has_header = [any(s[1] < HEADER_Y and "s6000" in s[4] for s in sp) for sp in spans_by_page]
        self.md_ranges = md_page_ranges(self.md_lines, has_header)
        for pi, spans in enumerate(spans_by_page, 1):
            self.add_page(pi, spans)
        self.parse_back_index()

    # RQ-DIX-004 (banner), RQ-DIX-005/RQ-DIX-011 (outline entry with no row: top of its page)
    def add_page(self, pi, spans):
        chapter, label = page_furniture(spans)
        self.chapter = chapter or self.chapter
        self.labels[pi] = label
        rng = self.md_ranges[pi]
        self.clean.append(f"=== pdf p{pi} | doc p{label or '-'} | md L{rng[0]}-{rng[1]} | {self.chapter or '-'} ===")
        first = len(self.clean)
        rows = page_rows([s for s in spans if HEADER_Y <= s[1] < FOOTER_Y])
        pager = Pager(self.md_lines, rng)
        where = (pi, label, pager)
        row_keys = {norm(r["text"]) for r in rows}
        for entry in self.outline.get(pi, []):   # an outline entry with no row of its own
            if norm(entry[1]) not in row_keys:
                entry[2] = True
                self.add_heading(entry[0], entry[1], where, rng[0])
        if self.chapter == "INDEX":
            self.index_rows += [(pi, r) for r in rows]
        body = pi >= FIRST_BODY_PDF and self.chapter not in ("INDEX", "")
        for i, row in enumerate(rows):
            self.process_row(row, rows[i + 1] if i + 1 < len(rows) else None, where, body)
        self.page_clean[pi] = (first, len(self.clean))
        self.anchors = [self.anchors[0] + pager.matched, self.anchors[1] + pager.total]

    def process_row(self, row, nxt, where, body):
        md = where[2].anchor(row["key"])
        levels = self.heading_levels(row, where[0], body)
        for lvl in levels:
            self.add_heading(lvl, row["text"], where, md)
        if levels:
            return
        self.clean.append(row["text"])
        if body:
            self.classify(row, nxt, where, md)

    # ---- headings -----------------------------------------------------------------------------------------------
    # RQ-DIX-005, DEC-DIX-007
    def heading_levels(self, row, pi, body):
        """The levels of a heading row: those of the PDF outline entries it carries (a chapter and its first section
        can share one title), else one level from its bold font; empty when the row is not a heading."""
        key = norm(row["text"])
        levels = []
        for entry in self.outline.get(pi, []):
            if not entry[2] and norm(entry[1]) == key:
                entry[2] = True
                levels.append(entry[0])
        if levels or not body or len(row["segs"]) > 1:
            return levels
        x, text, bold, size = row["segs"][0]
        if not bold or not is_term(text) or NOTE_RE.match(text) or text.startswith(("*", "WARNING")):
            return []
        if not TERM_X[0] <= x <= 105:
            return []
        return [2 if size >= 11.5 and x < 92 else 3]

    # RQ-DIX-005, RQ-DIX-006
    def add_heading(self, lvl, title, where, md):
        pi, label, _ = where
        self.clean.append("#" * lvl + " " + title)
        self.headings.append({"lvl": lvl, "title": title, "pdf": pi, "doc": label, "md": md,
                              "clean": len(self.clean), "chapter": self.chapter})
        self.cur = None

    # ---- parameters, values, notes, footnotes -------------------------------------------------------------------
    # RQ-DIX-007, RQ-DIX-009, DEC-DIX-008
    def classify(self, row, nxt, where, md):
        x, text, bold, _ = row["segs"][0]
        rest = " | ".join(s[1] for s in row["segs"][1:])
        if bold and x <= 115 and NOTE_RE.match(text):
            body = (NOTE_RE.sub("", text, count=1).strip() + " " + rest).strip()
            interrupted = self.cur if self.cur and self.cur["kind"] in "PV" else None
            self.cur = self.add_term("N", NOTE_RE.match(text).group(1), "", body, where, md, x)
            self.cur["resume"] = interrupted     # a note inside a list of values does not end the list
        elif self.is_footnote(row):
            self.footnote = int(text)
            self.cur = self.add_term("F", text, "", row["segs"][1][1], where, md, row["segs"][1][0])
        elif self.cur and self.cur["kind"] in "NF":
            self.continue_note(row, nxt, where, md)
        elif TERM_X[0] <= x <= TERM_X[1] and is_term(text) and has_description(row, nxt, DESC_MIN_X):
            dx = row["segs"][1][0] if len(row["segs"]) > 1 else nxt["x0"]
            self.cur = self.add_term("P", text, "", rest.replace(" | ", " "), where, md, dx)
        else:
            self.continue_term(row, nxt, where, md)

    # RQ-DIX-009
    def is_footnote(self, row):
        """A footnote: the next number of the sequence then its text, low on the page."""
        segs = row["segs"]
        return (len(segs) == 2 and segs[0][1] == str(self.footnote + 1) and TERM_X[0] <= segs[0][0] <= TERM_X[1]
                and FOOTNOTE_X[0] <= segs[1][0] <= FOOTNOTE_X[1] and row["y"] >= FOOTNOTE_MIN_Y)

    # RQ-DIX-008, RQ-DIX-009
    def continue_note(self, row, nxt, where, md):
        """A note or footnote goes on with the rows aligned with its text; any other row ends it."""
        if abs(row["x0"] - self.cur["dx"]) <= 4:
            self.cur["desc"] = join_text(self.cur["desc"], row["text"])
            return
        self.cur = self.cur.get("resume")
        self.classify(row, nxt, where, md)

    # RQ-DIX-008, DEC-DIX-008
    def continue_term(self, row, nxt, where, md):
        """A row that goes on with the current parameter: one of its values, or more of a description."""
        p = self.cur["owner"] if self.cur and self.cur["kind"] == "V" else self.cur
        if p is None:
            return
        x, text, _, _ = row["segs"][0]
        if DESC_MIN_X <= x <= p["dx"] + 6 and is_term(text) and has_description(row, nxt, x + 40):
            # a value: at the description's column, or further left on the pages whose list of values is shifted
            dx = row["segs"][1][0] if len(row["segs"]) > 1 else nxt["x0"]
            desc = " ".join(s[1] for s in row["segs"][1:])
            self.cur = self.add_term("V", text, p["term"], desc, where, md, dx)
            self.cur["owner"] = p
            return
        if x < p["dx"] - 4 and not (self.cur["kind"] == "V" and x >= self.cur["dx"] - 4):
            self.cur = None
            return
        if self.cur["kind"] != "V" or x < self.cur["dx"] - 4:
            self.cur = p
        self.cur["desc"] = join_text(self.cur["desc"], row["text"].replace(" | ", " "))

    # RQ-DIX-007, DEC-DIX-003
    def add_term(self, kind, term, parent, desc, where, md, dx=0):
        pi, label, _ = where
        h = self.headings[-1] if self.headings else None
        t = {"kind": kind, "term": term, "parent": parent, "desc": desc, "section": h["title"] if h else "",
             "chapter": self.chapter, "pdf": pi, "doc": label, "md": md, "clean": len(self.clean), "dx": dx}
        self.terms.append(t)
        return t

    # ---- back-of-book index -------------------------------------------------------------------------------------
    # RQ-DIX-010
    def parse_back_index(self):
        """The alphabetical index: two columns of "Term page, page, ..." lines; one row per (term, page)."""
        cells = sorted((pi, 0 if x < 300 else 1, row["y"], text)
                       for pi, row in self.index_rows for x, text, bold, size in row["segs"]
                       if not (bold and size >= 11.5))      # not the letter headings
        entries, held = [], ""
        for *_, text in cells:              # page by page, left column then right column, top to bottom
            term, pages = split_index_entry(text)
            if term and not pages:          # a term carried over to the next line
                held = join_text(held, term)
            elif term:
                entries.append((join_text(held, term), pages))
                held = ""
            elif entries:                   # page numbers carried over to the next line
                entries[-1][1].extend(pages)
        by_label = {v: k for k, v in self.labels.items() if v.isdigit()}
        for term, pages in entries:
            for p in pages:
                if str(p) in by_label:
                    self.add_index_term(term, pages, p, by_label[str(p)])

    # RQ-DIX-010, RQ-DIX-018, DEC-DIX-003
    def add_index_term(self, term, pages, printed, pi):
        """Points an index entry at the first line that names it on its page, else on the next or the previous page
        (the index is sometimes one page off), else at the first line of its page holding all its words, else at its
        page itself."""
        words = index_words(term)
        key = "".join(words)
        found = self.find_line(lambda t: key in t, (pi, pi + 1, pi - 1)) or self.find_line(
            lambda t: all(w in t for w in words), (pi,))
        pi, clean = found or (pi, self.page_clean[pi][0])
        a, b = self.md_ranges[pi]
        target = norm(self.clean[clean - 1]) if found else key
        md = next((ln for ln in range(a, b + 1) if norm(self.md_lines[ln - 1]).startswith(target[:20])), None) or next(
            (ln for ln in range(a, b + 1) if key in norm(self.md_lines[ln - 1])), a)
        h = self.heading_at(clean)
        self.terms.append({"kind": "I", "term": term, "parent": "", "desc": "p" + ", ".join(map(str, pages)),
                           "section": h["title"] if h else "", "chapter": h["chapter"] if h else "",
                           "pdf": pi, "doc": self.labels[pi] or str(printed), "md": md, "clean": clean})

    def find_line(self, test, pages):
        """(pdf page, clean line) of the first clean line of these pages whose normalised text passes the test."""
        for p in (p for p in pages if p in self.page_clean):
            first, last = self.page_clean[p]
            ln = next((ln for ln in range(first + 1, last + 1) if test(norm(self.clean[ln - 1]))), None)
            if ln:
                return p, ln
        return None

    def heading_at(self, clean_line):
        i = bisect.bisect_right([h["clean"] for h in self.headings], clean_line) - 1
        return self.headings[i] if i >= 0 else None

    # ---- outputs ------------------------------------------------------------------------------------------------
    # RQ-DIX-006
    def toc_rows(self):
        """Every heading with the span of its section: up to the next heading of the same or a higher level."""
        rows, path = [], []
        for i, h in enumerate(self.headings):
            nxt = next((n for n in self.headings[i + 1:] if n["lvl"] <= h["lvl"]), None)
            path = path[:h["lvl"] - 1] + [h["title"]]
            end_pdf = nxt["pdf"] if nxt else len(self.labels)
            end_md = nxt["md"] - 1 if nxt else len(self.md_lines)
            end_clean = nxt["clean"] - 1 if nxt else len(self.clean)
            rows.append({"lvl": h["lvl"], "title": h["title"], "path": " > ".join(path),
                         "pdf": span(h["pdf"], end_pdf), "doc": span(h["doc"], self.last_label(end_pdf)),
                         "md": span(h["md"], max(end_md, h["md"])), "clean": span(h["clean"], end_clean)})
        return rows

    def last_label(self, pi):
        """The printed label of this pdf page, or of the last numbered page before it."""
        return next((self.labels[p] for p in range(pi, 0, -1) if self.labels[p]), "")

    # RQ-DIX-001, RQ-DIX-013, DEC-DIX-002, DEC-DIX-010
    def write_outputs(self):
        OUT.mkdir(exist_ok=True)
        write(OUT / "user_manual.clean.txt", self.clean)
        write_tsv(OUT / "user_manual.toc.tsv", ["lvl", "title", "path", "pdf", "doc", "md", "clean"], self.toc_rows())
        write_tsv(OUT / "user_manual.terms.tsv",
                  ["kind", "term", "parent", "desc", "section", "chapter", "pdf", "doc", "md", "clean"], self.terms)
        kinds = {k: sum(t["kind"] == k for t in self.terms) for k in "PVNFI"}
        missing = sum(not e[2] for v in self.outline.values() for e in v)
        print(f"{len(self.headings)} headings, {kinds['P']} parameters, {kinds['V']} values, {kinds['N']} notes, "
              f"{kinds['F']} footnotes, {kinds['I']} index entries; rows anchored in the .md: "
              f"{self.anchors[0]}/{self.anchors[1]}; outline entries not placed: {missing}")


# RQ-DIX-010, RQ-DIX-018
def index_words(term):
    """What an index entry's page says: its words, normalised, without the parenthesised context and the generic
    words ('Fine Tune Window (Zone)' -> ['fine', 'tune'])."""
    words = [norm(w) for w in re.sub(r"\([^)]*\)?", " ", term).split() if w.lower() not in INDEX_NOISE]
    return [w for w in words if w] or [norm(term)]


# RQ-DIX-007, DEC-DIX-008
def has_description(row, nxt, min_x):
    """A name row followed, on the same row or the next, by text further right."""
    if len(row["segs"]) > 1:
        return row["segs"][1][0] >= min_x
    return nxt is not None and nxt["x0"] >= min_x and nxt["x0"] > row["x0"] + 40


# DEC-DIX-003
def span(a, b):
    """'a-b', or one of them when they are equal or one is missing (an unnumbered page)."""
    a, b = str(a), str(b)
    if a == b or not b:
        return a
    return f"{a}-{b}" if a else b


# RQ-DIX-019: UTF-8 and LF on every platform
def write(path, lines):
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def write_tsv(path, cols, rows):
    out = ["\t".join(cols)]
    out += ["\t".join(str(r[c]).replace("\t", " ") for c in cols) for r in rows]
    write(path, out)


def main():
    builder = IndexBuilder(pymupdf.open(PDF), MD.read_text(encoding="utf-8").splitlines())
    builder.build()
    builder.write_outputs()


if __name__ == "__main__":
    main()
