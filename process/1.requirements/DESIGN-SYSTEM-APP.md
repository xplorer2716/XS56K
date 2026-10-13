# Design System of the XS56K Editor Application

*Instantiated from `process/1.requirements/design-system-template.md` in session APP (2026-10-13, TASK-APP-004) for RQ-APP-001 to
RQ-APP-005; its structure and mechanisms are decided by ADR-APP-002. The owner's choices of 2026-10-13: a style inspired by the synthesizer
plugin Vital (a screenshot given as reference), one colour per functional role, rotary knobs with their value, an embedded free font, a
compact display. "Inspired" means the general style — dark, flat, coloured by role, dense — not a copy: no logo, name, asset or measured value
of Vital is used, and every value below is this project's own.*

---

## 0. Agent Operating Rules (read this first)

Every change that touches a colour, a size, a spacing, a radius, a stroke, a duration or an interaction state SHALL follow the procedure of the
template's §0, unchanged:

```
Need a colour/size/duration/radius for new or changed UI code
  ├─ 1. A COMPONENT token (§4) names this exact thing?            → use it.
  ├─ 2. A SEMANTIC token (§2) fits the role?                       → reference it, or add a component token aliasing it.
  ├─ 3. None fits: reusable?                                       → add a SEMANTIC token in §2 and in the token file, in the same change;
  │                                                                  truly one-off → a COMPONENT token in §4, never an inline literal.
  └─ 4. NEVER add a GLOBAL token while implementing a component    → stop and ask the owner.
```

**Hard rules (checked by the `ctest` entry of RQ-APP-004, ADR-APP-002 DEC-APP-014):**

1. No raw colour literal (`juce::Colour (0x…)`, `Colour::fromRGB`, `juce::Colours::…`) outside the token module.
2. No raw numeric literal for a font size, radius, stroke width, spacing or duration outside the token module, `LookAndFeel` included.
3. Every component token resolves to a semantic token, every semantic token to a global token; only global tokens hold a value.
4. Two components needing "the same-looking thing" share one token.
5. A state rule not in §3 is either looked up there or added there, never buried in a component.

**When blocked** (no token fits, not one-off, not a new semantic role): stop and ask the owner.

**Where the values live.** The single machine-readable source of truth is the token file `juce/app/design/tokens.json` (ADR-APP-002
DEC-APP-011). The value tables of §2 are the values proposed on 2026-10-13; once the token file exists (the implementation task of
ADR-APP-002), they are written from it by the generator and checked by its `--check`, never edited by hand.

---

## 1. Purpose & Principles

- **P1 — The sampler's state, nothing else.** What is drawn is what was read from the sampler; a value not yet confirmed is drawn as pending
  (§3), never as if it were held (RQ-APP-010, RQ-APP-052).
- **P2 — Colour says the role, text says the meaning.** A colour marks a family of functions (§2.1 roles); every colour-coded thing also has a
  label, an icon or a position that says the same (RQ-APP-005).
- **P3 — Dense but legible.** A tab shows as many parameters as fit on a 1280 × 800 window without scrolling, at the type scale of §2.2, on a
  4 px grid (§2.3).
- **P4 — Flat and calm.** No shadow, no gradient, no decorative animation; contrast between surfaces carries the structure (§2.4).
- **P5 — Immediate feedback.** Moving a control redraws it at once; nothing animates between the person's input and its drawing (§2.6).

## 2. Foundations — Design Tokens

Three tiers, each aliasing only the tier below; only global tokens hold a value.

### Naming grammar

| Tier | Pattern | Example |
|---|---|---|
| Global | `<category>.<family>.<step>` | `colour.grey.900`, `size.space.2` |
| Semantic | `<category>.<role>.<variant?>` | `colour.surface.panel`, `colour.role.filter` |
| Component | `<component>.<part>.<state?>` | `knob.arc.value`, `tab.underline.selected` |

Transliteration to C++ (§7.1): the dots and hyphens are dropped, each word capitalised, prefix `k`: `colour.surface.panel` →
`kColourSurfacePanel`.

### 2.1 Colour

**Theme.** One dark theme. No light theme is planned; adding one later means one value per theme for each semantic token, the component
tokens unchanged.

**Global palette** (proposed 2026-10-13; the only place colour values appear):

| Token | Value | Token | Value |
|---|---|---|---|
| `colour.grey.950` | `#1B1C1F` | `colour.violet.300` | `#A897F2` |
| `colour.grey.900` | `#202125` | `colour.teal.300` | `#4FD1C1` |
| `colour.grey.850` | `#2A2B30` | `colour.amber.300` | `#E9A85A` |
| `colour.grey.800` | `#35363C` | `colour.yellow.300` | `#F2C94C` |
| `colour.grey.700` | `#45464D` | `colour.red.300` | `#F47A74` |
| `colour.grey.500` | `#6C6D75` | `colour.green.300` | `#6CCB7A` |
| `colour.grey.300` | `#A9AAB2` | | |
| `colour.grey.100` | `#ECECEF` | | |

**Semantic roles:**

| Token | Aliases | Use |
|---|---|---|
| `colour.surface.base` | `grey.950` | window background, between panels |
| `colour.surface.panel` | `grey.850` | panels (browser, memory view, edit panel groups) |
| `colour.surface.recessed` | `grey.900` | inputs, lists, value boxes, graph backgrounds |
| `colour.surface.raised` | `grey.800` | hovered rows, buttons, dialogs |
| `colour.border.default` | `grey.700` | panel and control outlines, separators |
| `colour.text.primary` | `grey.100` | values, names |
| `colour.text.secondary` | `grey.300` | labels (upper case), captions |
| `colour.text.disabled` | `grey.500` | disabled controls and their labels |
| `colour.accent.default` | `violet.300` | selection, focus, current tab |
| `colour.role.sample` | `violet.300` | samples, zones, sample assignment |
| `colour.role.modulation` | `teal.300` | envelopes, LFOs, modulation sources |
| `colour.role.filter` | `amber.300` | filter and its envelope controls |
| `colour.role.general` | `grey.300` | parameters of no colour-coded family (output, MIDI/tune, pitch bend) |
| `colour.status.active` | `yellow.300` | "on", current item of a list, a switch that is on |
| `colour.status.success` | `green.300` | connected |
| `colour.status.error` | `red.300` | failed, refused, lost connection |
| `colour.status.destructive` | `red.300` | the confirm button of a destructive action |
| `colour.status.pending` | `grey.300` | value sent and not yet read back |

Which role a parameter takes is a property of the parameter (its family), not of the tab it is shown in: a tab may mix roles (RQ-APP-056).

**User-overridable colours:** none.

### 2.2 Typography

- **Family:** Inter, embedded (SIL Open Font License 1.1, licence verified on 2026-10-13 in `LICENSE.txt` of github.com/rsms/inter; the version
  is pinned when it is added, ADR-APP-002 DEC-APP-015). Fallback: the system's default sans-serif when the embedded font fails to load.
- **Type scale (by role):**

| Token | Size (px) | Weight | Case | Use |
|---|---|---|---|---|
| `font.caption` | 11 | Regular | as written | captions, units, status details |
| `font.label` | 11 | Medium | UPPER CASE, tracking `font.label.tracking` = 0.5 px | parameter and group labels |
| `font.value` | 13 | Regular, tabular figures | as written | values, names in lists and trees |
| `font.title` | 15 | Medium | as written | panel titles, the edited item's name |
| `font.heading` | 18 | Medium | as written | dialog titles |

- **Dynamic-fit bounds:** a value shrunk to fit never goes below `font.caption` (11 px) nor above its own size.

### 2.3 Spacing & Layout Grid

- **Base unit:** 4 px.
- **Spacing scale:** `space.1` 4 · `space.2` 8 · `space.3` 12 · `space.4` 16 · `space.6` 24 · `space.8` 32 (px). Semantic:
  `space.inside` = `space.2` (padding inside a control or a panel), `space.between` = `space.2` (between controls of a group),
  `space.group` = `space.4` (between groups), `space.edge` = `space.2` (window edge).
- **Scaling model:** sizes are logical pixels; the window does not zoom its content when resized, it shows more or less (lists and graphs
  stretch, controls keep their size). The operating system's display scale is applied uniformly by JUCE; hairlines stay 1 logical pixel.
- **Minimum sizes:** main window `layout.window.min` 1024 × 680; left column `layout.left.min` 240 wide; browser and memory view
  `layout.area.min` 160 high each; edit panel `layout.edit.min` 640 wide (RQ-APP-008).

### 2.4 Geometry

- **Corner radius:** `radius.sharp` 0 · `radius.soft` 3 · `radius.round` 6 (px). Panels `radius.round`, controls and value boxes
  `radius.soft`, separators `radius.sharp`.
- **Stroke widths:** `stroke.hairline` 1 · `stroke.focus` 2 · `stroke.arc` 3 (px).
- **Elevation:** flat — no shadow at any level; a dialog is distinguished by `colour.surface.raised` and `colour.border.default`.

### 2.5 Iconography & Imagery

- **Style:** outline icons, `stroke.focus` wide, `radius.soft` corners, drawn in `colour.text.secondary` or the role colour of what they act on.
- **Format & sizing:** vector only (SVG paths drawn by JUCE), sizes `icon.small` 12 and `icon.default` 16 px; no raster image.
- **Sourcing:** drawn for this project, or taken from a set under a licence compatible with AGPL-3.0, recorded with its licence when added.
- **File kinds of the browser** (RQ-APP-026): one icon per kind — program, sample, multi, song file, set list, scenelist, unknown, folder, disk —
  each also named in the file's information line.

### 2.6 Motion & Timing

- **Durations:** `motion.hover` 80 ms (a hovered row or button fades to its hover colour); `motion.busy` 1000 ms (one turn of the busy
  indicator, RQ-APP-012); `motion.message` 4000 ms (an information message stays before it fades; errors stay until dismissed).
- **Easing:** linear for `motion.busy`, ease-out for the others.
- **When not to animate:** the value of a control, its arc, a curve, a list's content — drawn at once (P5).

## 3. Component Interaction States

| State | Trigger | Visual rule | Second cue (RQ-APP-005) |
|---|---|---|---|
| Idle | no interaction | panel and control tokens of §2 | — |
| Hover | pointer over | surface goes to `colour.surface.raised` in `motion.hover` | pointer shape |
| Pressed / dragging | pointer down | value text in `colour.text.primary`, arc thickened to `stroke.arc` + `stroke.hairline` | the value shown follows the pointer |
| Focused | keyboard focus | `stroke.focus` outline in `colour.accent.default` | outline shape, not only colour |
| Disabled | control inactive, or the application disconnected (RQ-APP-019) | text and arc in `colour.text.disabled` | the control does not respond; tooltip says why |
| Selected | chosen tab, row or node | `colour.accent.default` underline (tab) or left bar (row) | underline or bar shape |
| Active | on, current item | `colour.status.active` dot | a filled dot, and "current" in the tooltip |
| Pending | value sent, not read back (RQ-APP-052) | arc and value in `colour.status.pending`, value followed by "…" | the "…" suffix |
| Error | refused or failed (RQ-APP-010) | `colour.status.error` outline, the sampler's value shown | an error icon and the message (RQ-APP-010) |

## 4. Component Catalogue

### Knob (rotary control with its value)
| Field | Value |
|---|---|
| Anatomy | track arc, value arc, pointer, label (above or below), value box |
| States implemented | idle, hover, pressed, focused, disabled, pending, error |
| Tokens used | `knob.arc.track` → `colour.border.default`; `knob.arc.value` → the parameter's role colour; `knob.pointer` → `colour.text.primary`; `knob.size.default` → 40 px (component token aliasing `size.control.large`); `knob.size.small` → 28 px (`size.control.medium`) |
| Variants | default, small |
| Behaviour owner | RQ-APP-051, RQ-APP-052, RQ-APP-054; the value box accepts typing (range checked, RQ-APP-051) |

### Value box (number field)
| Field | Value |
|---|---|
| Anatomy | recessed box, value text, unit |
| States implemented | idle, hover, focused, disabled, pending, error |
| Tokens used | `valuebox.surface` → `colour.surface.recessed`; `valuebox.text` → `colour.text.primary`; `valuebox.unit` → `colour.text.secondary`; `valuebox.height` → `size.control.small` |
| Behaviour owner | RQ-APP-051 |

### Choice selector (`< name >`)
| Field | Value |
|---|---|
| Anatomy | recessed box, previous arrow, name, next arrow; a click on the name opens the list |
| States implemented | idle, hover, focused, disabled, pending, error |
| Tokens used | `choice.surface` → `colour.surface.recessed`; `choice.arrow` → `colour.text.secondary`; `choice.text` → `colour.text.primary` |
| Behaviour owner | RQ-APP-051 (labels as the sampler names them) |

### Switch
| Field | Value |
|---|---|
| Anatomy | a dot and a label |
| States implemented | idle, hover, focused, disabled, active, pending, error |
| Tokens used | `switch.dot.on` → `colour.status.active`; `switch.dot.off` → `colour.border.default` |
| Behaviour owner | RQ-APP-051 |

### Tab bar (bottom of the edit panel)
| Field | Value |
|---|---|
| Anatomy | tab titles in a row, underline of the selected tab |
| States implemented | idle, hover, focused, selected, disabled |
| Tokens used | `tab.text` → `colour.text.secondary`; `tab.text.selected` → `colour.text.primary`; `tab.underline.selected` → `colour.accent.default`; `tab.height` → `size.control.large` |
| Behaviour owner | RQ-APP-049, RQ-APP-055 |

### Tree (file browser) and List (memory view)
| Field | Value |
|---|---|
| Anatomy | rows: expander (tree), icon, name, detail (size, count); headings with counts (memory view) |
| States implemented | idle, hover, focused, selected, active (current folder, current item), disabled |
| Tokens used | `row.height` → `size.control.medium`; `row.surface.hover` → `colour.surface.raised`; `row.bar.selected` → `colour.accent.default`; `row.dot.current` → `colour.status.active` |
| Behaviour owner | RQ-APP-024 to RQ-APP-027, RQ-APP-035, RQ-APP-037 |

### Context header (edited item and keygroup selector)
| Field | Value |
|---|---|
| Anatomy | item name, item kind, keygroup selector (a choice selector) |
| States implemented | idle, disabled, pending |
| Tokens used | `header.title` → `font.title`, `colour.text.primary`; `header.kind` → `font.label`, `colour.text.secondary` |
| Behaviour owner | RQ-APP-050, RQ-APP-058 |

### Button
| Field | Value |
|---|---|
| Anatomy | raised box, label |
| States implemented | idle, hover, pressed, focused, disabled |
| Tokens used | `button.surface` → `colour.surface.raised`; `button.text` → `colour.text.primary`; `button.destructive.surface` → `colour.status.destructive`; `button.destructive.text` → `colour.surface.base` |
| Variants | default, destructive (confirm of RQ-APP-011) |
| Behaviour owner | RQ-APP-011 |

### Status bar (connection and busy indicator)
| Field | Value |
|---|---|
| Anatomy | connection dot and text, busy spinner and job label, message area |
| States implemented | idle, active (connected), error, pending (busy) |
| Tokens used | `status.dot.connected` → `colour.status.success`; `status.dot.failed` → `colour.status.error`; `status.spinner` → `colour.text.secondary` |
| Behaviour owner | RQ-APP-012, RQ-APP-019 |

### Dialog (confirmation, question, MIDI setup)
| Field | Value |
|---|---|
| Anatomy | title, text naming the object and what is lost, buttons |
| States implemented | idle, focused |
| Tokens used | `dialog.surface` → `colour.surface.raised`; `dialog.border` → `colour.border.default`; `dialog.title` → `font.heading` |
| Behaviour owner | RQ-APP-011, RQ-APP-015, RQ-APP-045 |

**Size tokens used above** (global): `size.control.small` 20 · `size.control.medium` 24 · `size.control.large` 28 · `size.knob.large` 40 (px);
`knob.size.default` aliases `size.knob.large`, `knob.size.small` aliases `size.control.large`.

## 5. Layout Patterns

- **Three areas** (RQ-APP-006): left column (browser over memory view), edit panel on the right; separated by `space.edge`, resizable
  (RQ-APP-008) down to the minimum sizes of §2.3.
- **Group panel:** a `colour.surface.panel` rectangle, `radius.round`, holding a label row (`font.label`) and its controls on the 4 px grid;
  the groups of a tab flow left to right, then wrap.
- **Control cell:** a knob with its label under it and its value box, as one unit of fixed width; cells align on a column grid of
  `size.knob.large` + `space.4`.
- **Context header over tabs over tab bar:** the header at the top of the edit panel (RQ-APP-050), the tab's groups in the middle, the tab bar
  at the bottom (RQ-APP-049).

## 6. Accessibility & Ergonomics

- **Contrast** (WCAG 2.x formula, computed in session on 2026-10-13 for the values of §2.1):

| Text | on `surface.base` | on `surface.panel` | on `surface.recessed` | on `surface.raised` |
|---|---|---|---|---|
| `text.primary` | 14.5 | 12.0 | 13.6 | 10.2 |
| `text.secondary` | 7.4 | 6.1 | 7.0 | 5.2 |
| `role.sample` / `accent` | 6.8 | 5.6 | 6.4 | 4.8 |
| `role.modulation` | 9.1 | 7.5 | 8.6 | 6.4 |
| `role.filter` | 8.3 | 6.9 | 7.8 | 5.9 |
| `status.active` | 10.7 | 8.9 | 10.1 | 7.6 |
| `status.error` | 6.4 | 5.3 | 6.0 | 4.5 |
| `status.success` | 8.5 | 7.0 | 8.0 | 6.0 |

  Every pair used for text is at least 4.5 : 1. `text.disabled` (2.3 to 3.3) is used only for inactive controls, which WCAG exempts.
  `button.destructive.text` (`grey.950` on `red.300`): 6.4.
- **Minimum hit target:** 20 × 20 px (`size.control.small`); a knob's hit area is its whole cell.
- **Resizability:** minimum sizes of §2.3; content does not scale with the window.
- **Never colour alone:** the second cue of each state is in §3; each role colour comes with the group's label; each status with a word.
- **Keyboard:** every control can take focus; arrows change a knob by one step, page keys by ten.

## 7. Framework Binding

### 7.1 Token module
`juce/app/design/tokens.json` is the source; `juce/tools/generate_design_tokens.py` writes `DesignTokens.generated.hpp` (namespace
`app::tokens`) into `xs56k_app_core`, with no JUCE include: colours as `std::uint32_t` ARGB, sizes and durations as `float` or
`std::chrono::milliseconds`. Transliteration: §2 naming grammar. ADR-APP-002 DEC-APP-011, DEC-APP-012.

### 7.2 Rendering integration
One `juce::LookAndFeel_V4` subclass in `XS56KApp` reads the tokens and draws the knobs, tabs, rows, buttons and dialogs; components draw
nothing of their own beyond what it offers. ADR-APP-002 DEC-APP-013.

### 7.3 State-to-code mapping
Hover: `isMouseOver()`; pressed: `isMouseButtonDown()` or a slider's drag; focused: `hasKeyboardFocus (true)`; disabled: `! isEnabled()`;
selected, active, pending and error: properties the views read from the model (DEC-APP-004), set on the component before it repaints.

### 7.4 Prototyping / mockup sync
No mockup tool. Mockups, if any, are screenshots of the running application; the token file is the only palette.

## 8. Testability

- **Headless:** the token resolution (every component token reaches a global value), the state-to-token mapping of the views' helpers, and the
  contrast figures of §6, recomputed from the token file by a unit test of the generator.
- **Visual or manual:** the drawing itself, checked by the owner on the running application.
- **Regression check:** the `ctest` entry of RQ-APP-004 (no raw visual literal outside the token module) and the generator's `--check`.

## 9. Governance

- **Adding a token:** propose it here (tier, name, value), add it to the token file in the same change, regenerate, migrate the call sites.
- **Changing a value:** in the token file only; no call site changes.
- **Deprecating a token:** search its spec name and its C++ name; remove only at zero references.
- **Drift prevention:** §0 and the `ctest` entry of RQ-APP-004.
- **Ownership:** the owner approves every change to the global tokens and to this document.

## 10. Non-Goals

- No application behaviour: that is FTR-APP-002 to FTR-APP-009.
- No light theme, no user-chosen colours, no zoom of the content.
- No reproduction of Vital's assets, logo, name or measured values.
- The editable curves (RQ-APP-073) are priority 2; their look is added here when they are detailed.

---

## Appendix: Good / Bad Examples

| ❌ Bad | ✅ Good | Why |
|---|---|---|
| `g.setColour (juce::Colour (0xffa897f2));` | `g.setColour (juce::Colour (tokens::kColourRoleSample));` | raw literal vs token (§0 rule 1) |
| a filter knob coloured violet because it sits in a tab of samples | `knob.arc.value` takes the parameter's own role | role belongs to the parameter (§2.1) |
| a pending value drawn in the value the person typed, normal colour | `colour.status.pending` and "…" until the read-back | P1, §3 |
| `font.withHeight (11.0f)` in a component | `tokens::kFontLabelSize` | §0 rule 2 |
