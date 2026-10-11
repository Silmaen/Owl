# Owl Nest style guide {#page-design-nest-style-guide}

[TOC]

Rules every Owl Nest panel follows, so that a new panel looks like the others without a review round. The plan
that produced them is in [Nest UI](nest-ui.md); the editor itself is described in [Editor](../editor.md).

## Principles

- **Quiet surfaces, one accent.** Panels are neutral greys; Owl amber marks the selected entity, the active tool, a
  switch that is on and the primary action. Headers, combo lists, slider grabs and the toggled ribbon buttons stay
  grey. Blue is kept for Play mode, red / green / blue for the X / Y / Z axes, and the status colours for status
  only (the Log level chips take the colour of their level).
- **Labels on the left, values on the right.** Every property is one row of the property grid.
- **An icon never stands alone.** An icon button has a tooltip naming the action and its shortcut.
- **An empty panel says what to do.** No blank area: an `emptyState` with an icon, a title and a hint.
- **Feedback is visible and short.** A save, an export or a failure shows a toast; an incomplete component shows a
  `notice` in the inspector.

## Colours

Code never writes a colour: it reads a token of `gui::Theme` (`UiLayer::getTheme()`), and `UiLayer::setTheme`
derives every ImGui colour from the tokens. Owl Dark values:

| Token                                    | Owl Dark                                      | Use                                               |
|------------------------------------------|-----------------------------------------------|---------------------------------------------------|
| `surface0`                               | `#111215`                                     | Window background, behind the docked panels.      |
| `surface1`                               | `#191B1F`                                     | Panel body.                                       |
| `surface2`                               | `#202328`                                     | Popups, headers, the ribbon, the command palette. |
| `surface3` / `surface4`                  | `#292D33` / `#333840`                         | Hovered / pressed controls.                       |
| `well`                                   | `#0D0E11`                                     | Inside of a field.                                |
| `line` / `lineSoft`                      | `#2A2E35` / `#1F2227`                         | Borders and separators.                           |
| `text` / `textMuted` / `textDisabled`    | `#DADDE2` / `#9AA1AB` / `#5F6670`             | Values, labels and captions, unavailable items.   |
| `accent`                                 | `#EAA53C`                                     | Selection, active tool, primary button, focus.    |
| `onAccent`                               | dark in both themes                           | Text drawn on the accent.                         |
| `play`                                   | `#3D8BFF`                                     | Play mode: viewport frame, Play button.           |
| `success` / `warning` / `error` / `info` | `#4CB782` / `#EF8A3C` / `#E5534B` / `#5AA2F0` | Toasts, badges, notices, log levels.              |
| `axisX` / `axisY` / `axisZ`              | `#E5534B` / `#5DBB63` / `#4C8DF0`             | Vector fields, gizmos.                            |

- A tint is a token with an alpha (`Theme::withAlpha`): 0.12 for a notice background, 0.18 for a badge, the
  selection colour for selected rows (`Theme::getSelection`). A blend between two tokens is `Theme::mix`.
- Owl Light keeps the same roles; the `themePreset` test checks the contrast of text on its surfaces in both themes.

## Typography

Inter for the interface, JetBrains Mono for numbers in columns and code. Pick a role with `UiLayer::pushTextStyle`,
never a raw font size:

| Role (`gui::TextStyle`) | Font, size            | Use                                        |
|-------------------------|-----------------------|--------------------------------------------|
| `Body`                  | Inter, 1×             | Running text, values.                      |
| `Strong`                | Inter SemiBold, 1×    | Emphasis inside body text.                 |
| `Title`                 | Inter SemiBold, 1.15× | Panel and dialogue titles.                 |
| `Section`               | Inter SemiBold, 0.9×  | Component and section headers.             |
| `Label`                 | Inter, 0.875×         | Inspector rows, ribbon labels.             |
| `Caption`               | Inter SemiBold, 0.75× | Ribbon group captions, sub-section titles. |
| `Value`                 | JetBrains Mono, 0.92× | Numbers aligned in columns (stats).        |
| `Code`                  | JetBrains Mono        | The code editor.                           |

Write property labels in Title Case, as the inspector does ("Fixed Aspect Ratio"), button labels as a verb ("Add
Component"), and end a hint, a tooltip or a notice with a period.

## Spacing and shape

- Base unit 4 px, scaled with the monitor (`UiLayer::getDpiScale`); never hard-code a size that does not scale with
  the font: derive it from `ImGui::GetFontSize()` or `ImGui::GetFrameHeight()`.
- Rounding from the theme: windows `windowRounding` (5), fields and buttons `frameRounding` (3), tabs `tabRounding`
  (4), pills and chips fully round.
- Property grid: the label column takes 38 % of the width, the value fills the rest, the reset button shows only
  when the value differs from its default.

## Icons

- **Action glyphs** come from the Lucide font, `gui::icon::*` (`gui/IconFont.h`), inside labels, icon buttons,
  menus and the ribbon quick access. Add a missing name from the `lucide-static` `info.json`.
- **Ribbon buttons** use the duotone SVGs of `icons/ribbon/` (`ribbon_<name>` in the `IconBank`), drawn after the
  Lucide shapes with the accent on the part that carries the meaning (the label of the floppy, the arrow of undo).
- **Asset kinds and components** are hand-drawn SVGs in `source/owlnest/assets_sources/icons/`, on a 24-unit grid
  with a 1.5 stroke and a duotone fill. Asset kinds carry the fixed colour of their family (scene amber, prefab
  blue, texture red, sound green, script teal...); components and ribbon icons use `#ffffff` (the theme text
  colour) plus one piece in `#ff00ff` (the theme accent). SVGs are edited by hand only.
- Draw an icon with `IconBank::getIcon(name, size)` at the size it is shown, on a whole pixel.

## Widgets

Build panels from the kit in `gui/widgets/` before writing raw ImGui:

| Need                                 | Widget                                                                                               |
|--------------------------------------|------------------------------------------------------------------------------------------------------|
| A property row                       | `beginProperties`, `propertyLabel`, `propertyReset`, `propertyFloat`, `propertyVec3`, `propertyBool` |
| A whole number / a line of text      | `propertyInt` / `propertyText`                                                                       |
| Two to four numbers                  | `vectorField` (thin axis mark, no coloured box)                                                      |
| A collapsible block with a menu      | `sectionHeader` (IconBank icon or `gui::icon` glyph), `endSection`                                   |
| A button showing only an icon        | `iconButton` with an `IconButtonSpec` (tooltip, shortcut)                                            |
| Help on hover                        | `richTooltip`, `itemTooltip`                                                                         |
| A filter field                       | `searchField`, `matchesFilter`                                                                       |
| Nothing to show                      | `emptyState`                                                                                         |
| An on / off property / option        | `toggle` (inspector rows) / `checkbox` (a label after a small box)                                   |
| A nested block inside a component    | `subHeader`                                                                                          |
| A row of a tree (entities, folders)  | `treeRow`, `endTreeRow`                                                                              |
| A labelled button / the main action  | `button` (`primary` for the accent; `width` for the OK / Cancel row of a dialog)                     |
| A drop-down list                     | `beginCombo`, `combo` (never `ImGui::BeginCombo`: its arrow is a filled triangle)                    |
| A context menu                       | `menuHeader`, then `menuItem` (glyph column, shortcut, `danger` for a deletion)                      |
| A tile, block or atlas swatch        | `imageTile` (outline in the accent for the choice of the left button, `info` for the right)          |
| What follows the cursor in a drag    | `dragPreview` (glyph, name, kind) inside `BeginDragDropSource`                                       |
| A count or a state / a filter toggle | `badge` / `chip`                                                                                     |
| A property that needs attention      | `notice`                                                                                             |
| A short confirmation or failure      | `ToastQueue::push`                                                                                   |

## Interaction

- Every action lives in the `ActionRegistry`: it gets a shortcut that can be rebound, a place in the command
  palette (double `Shift`) and its shortcut in tooltips. A ribbon button or an overlay calls the action, not a copy of
  its code.
- Selection is amber; hover is one surface step lighter; a disabled item uses `textDisabled` and keeps its
  tooltip, saying why it is disabled when it is not obvious.
- Controls that act on the view float over the viewport in pills; the ribbon holds document and project commands.
- Every fold uses a thin chevron (right when closed, down when open), never ImGui's filled triangle; windows draw no
  window-menu arrow (`WindowMenuButtonPosition` is `None`).
