# Owl Nest UI {#page-design-nest-ui}

[TOC]

Design page for the editor user interface, summarised in the [Roadmap](../roadmap.md).

## Goal

A much more polished Owl Nest before 1.0, inspired by Unreal Engine's editor but with a style of its own: asset
thumbnails, icons everywhere, adjustable text scale, context menus, tooltips, a crafted theme and drag & drop
everywhere it makes sense.

All of it lands in v0.3.0 as phase E of [Foundations](foundations.md): an ergonomics revamp designed together with the
maintainer, with no new feature. It opens with a review of the main workflows (pain points listed, target layout
agreed), then the three parts below.

## Plan agreed on 10/10/2026

One pull request, iterated locally: shell and layout, design tokens and typography, icons, widget kit, ribbon,
viewport overlays, panels, assistance, then polish. Decisions: Owl amber as the only accent (blue kept for Play
mode), Lucide for action glyphs and hand-redrawn duotone SVGs for asset kinds and components, only Owl Dark and Owl
Light themes, command palette (double `Shift`), panels on the right by default with an optional left column, thumbnails
filling their whole tile.

- **Shell and layout** — ![Done][done]: default dock layout built on the first launch (`DockLayout`), presets
  Default and Hierarchy Left, Reset Layout, a status bar, document-bound panels shown only with their document.
- **Design tokens and typography** — ![Done][done]: `gui::Theme` tokens and their ImGui mapping, Owl Dark and
  Owl Light, Inter with text roles, DPI scaling of the style and the fonts.
- **Icons** — ![Done][done]: Lucide font for action glyphs, icons rasterized at the size they are drawn, asset-kind
  and component SVGs redrawn in duotone (components carry one piece in the theme accent).
- **Widget kit** — ![Done][done]: property grid, vector field, section header, icon button, rich tooltip, search
  field, empty state, badge, chip and toasts in `gui/widgets/`; the inspector headers and vectors use it.
- **Ribbon** — ![Done][done]: File screen, Home / View / document tabs, quick access, collapse, one large button
  per group.
- **Viewport overlays** — ![Done][done]: tools, playback, display toggles, frame-time chip and axis gizmo float over
  the view; play-colour frame while playing.
- **Panels** — ![Done][done]: label-left inspector, Hierarchy search / add / icons, Content Browser breadcrumb /
  search / views / thumbnails, Log chips, empty states, node canvas fixed and themed.
- **Assistance** — ![Done][done]: command palette on a double `Shift` and in the ribbon search box (actions, entities, assets), inline notices for an
  incomplete component, toasts on save, `F1` contextual help.
- **Polish** — ![Done][done]: the conventions of these steps written down in the [style guide](nest-style-guide.md).

## Interaction basics

Status after the revamp pull request: done — tooltips everywhere, context menus on the Hierarchy, the Content
Browser, the viewport, the document tabs and the node graphs, one drag preview (`dragPreview`) and drop highlight,
DPI scaling and a text scale applied at once (`UiLayer::setTextScale`, 75 to 150 %).

- Tooltips on every button, field and icon (extends the `fieldTooltip()` helper of v0.1.1 to the whole editor)
- Context menus on every selectable object: hierarchy entries, Content Browser items, viewport selection, tabs,
  node-graph nodes
- Consistent drag & drop: assets onto viewport / inspector fields / hierarchy, entities onto entity fields, files from
  the OS file manager into the Content Browser; one payload convention, one visual feedback
- Adjustable text scale and DPI awareness (per-monitor scale, crisp fonts, icon atlas tier chosen by scale)
- Session restore and autosave live in [Foundations](foundations.md), phase D (before the revamp)

## Visual overhaul

Status after the revamp pull request: done — theme, icon set (duotone ribbon and File screen icons included),
[style guide](nest-style-guide.md), previews of images, tilesets, scenes, prefabs, meshes and sounds in the Content
Browser (`AssetThumbnails`), atlas swatches in the asset fields of the inspector.

- Asset thumbnails in the Content Browser and asset fields (textures, prefabs, scenes, tilesets, meshes, sounds),
  rendered in the background and cached
- A crafted Owl theme (light and dark), with a written style guide: spacing, colours, typography, icon usage
- A complete icon set (SVG sources in `source/owlnest/assets_sources/icons/`), one per component, asset kind and
  action
- Panel layout presets and a cleaner ribbon

## Editor camera controls overhaul (Done)

- **DCC-style navigation** — ![Done][done] (landed early, v0.2.1): Alt+LMB rotate-in-place, Alt+RMB pan, Alt+MMB
  dolly; Ctrl+LMB orbit, Ctrl+MMB/RMB pan, wheel zoom — shared by every scene viewport. A corner XYZ orientation gizmo
  shows the camera facing.
- **Orientation math reworked** — ![Done][done]: `CameraEditor` now builds its orientation from a proper Euler
  quaternion, so a full 360° turn works and the view no longer shears (the old `{1,-pitch,-yaw,0}` convention warped
  past ~180°).
- **Navigation settings** — ![Done][done]: orbit, pan and zoom speeds, vertical inversion and a dead zone under
  `Settings > Editor > Camera` (`CameraEditor::Navigation`).
- **Interactive view-cube** — ![Done][done]: the corner gizmo shows the six axis ends; a click on one looks from that
  side, a click on the centre resets the view.
- **Standard navigation presets** — ![Done][done]: ribbon `View > Camera` group, viewport overlay menu and command
  palette: **Reset View** (`Home`), the six axis views (`CameraEditor::viewAlong`; Front is the XY plane, Top XZ, Right
  YZ, orthographic in the 2D view), **Frame Selection** (`F`), **Frame Scene** (`Shift+F`).
- **Go to camera viewpoint** — ![Done][done]: move the editor camera onto the selected `component::Camera` entity
  (`CameraEditor::lookFrom`; an orthographic camera gives the 2D view at its size). Disabled for a camera of a
  `RendererRaycast` layer (the editor renders those scenes flat — see "Look through scene camera" below).

## "Look through scene camera" mode (v0.5.0)

Let the user temporarily drive the editor viewport from any `component::Camera` entity in the scene (primary or
otherwise), for previewing what the runtime camera will see without entering Play. Toggleable from the camera entity's
context menu or a viewport overlay dropdown. Reverts to the editor camera on demand.

## Native file dialogues through the desktop portal (v0.4.0)

- The GTK backend of NFD briefly freezes the UI on Linux when GTK initializes (it trips the IDE "antiloop"
  detection); a pure ImGui picker is not pursued
- Build nativefiledialog-extended with its `portal=True` option (`conan/recipes/nativefiledialog-extended`):
  dialogues go through `xdg-desktop-portal` over D-Bus, no GTK, with the desktop's own look
- The `FileDialog::openFile` / `saveFile` / `pickFolder` calls stay unchanged
- The build image already has `dbus-1`; D-Bus joins the Linux packages' dependencies; checked on GNOME and KDE,
  Wayland and X11 (NEST1 of [Isometric worlds](isometric-worlds.md))

## In-editor documentation (v0.5.0)

### Mermaid diagram rendering in the help panel

- Today the md4c-based renderer treats ` ```mermaid ` fences as plain code blocks; the actual diagrams (used by
  `architecture.md`, `editor.md`, `node_graph.md`, `physics.md`, `renderer.md`, `scene.md`, `scripting.md`,
  `sound.md`) only render on GitHub / Doxygen
- Build-time pre-render of mermaid blocks → SVG (or PNG) files in the build tree's `help/images/mermaid/`, with the
  Markdown rewriter swapping each fence for an `![alt](images/mermaid/<sha>.svg)` reference. No runtime JS/Node
  dependency — pre-rendering can run with a packaged tool or a custom subset renderer in C++
- Update `cmake/HelpAssets.cmake` to invoke the pre-renderer and surface the cache files
- Tests: assert each bundled `help/*.md` no longer contains ` ```mermaid ` after the bundle step and
  that the rasterized diagram files exist

### Help-panel rendering polish (V2)

- Hanging indent in unordered/ordered lists (current V1 wraps to column 0 — see `MarkdownPreview::renderList`
  comment)
- Inline `code` rendered as a flat span (today it goes through `SmallButton` for the tinted background — works but
  adds a clickable affordance that reads as a button)
- Ordered-list numbering survives `is_tight` md4c quirks (current OL counter is renderer-side, not parser-side)
- Native rendering of GFM task lists `[ ]` / `[x]` (currently shown as inline text)
- Optional dark/light theme switch for the help panel content (today inherits from the editor theme via
  `ImGuiCol_Text` / `Owl::Theme::buttonHovered`)
- Side-by-side preview of the source `.md` next to the rendered output (debug aid for contributors editing pages)

[done]: https://img.shields.io/badge/-Done-2ea043?style=flat-square
