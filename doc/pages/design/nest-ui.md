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

## Interaction basics

- Tooltips on every button, field and icon (extends the `fieldTooltip()` helper of v0.1.1 to the whole editor)
- Context menus on every selectable object: hierarchy entries, Content Browser items, viewport selection, tabs,
  node-graph nodes
- Consistent drag & drop: assets onto viewport / inspector fields / hierarchy, entities onto entity fields, files from
  the OS file manager into the Content Browser; one payload convention, one visual feedback
- Adjustable text scale and DPI awareness (per-monitor scale, crisp fonts, icon atlas tier chosen by scale)
- Session restore and autosave live in [Foundations](foundations.md), phase D (before the revamp)

## Visual overhaul

- Asset thumbnails in the Content Browser and asset fields (textures, prefabs, scenes, tilesets, meshes, sounds),
  rendered in the background and cached
- A crafted Owl theme (light and dark), with a written style guide: spacing, colours, typography, icon usage
- A complete icon set (SVG sources in `source/owlnest/assets_sources/icons/`), one per component, asset kind and
  action
- Panel layout presets and a cleaner ribbon

## Editor camera controls overhaul (In Progress)

- **DCC-style navigation** — ![Done][done] (landed early, v0.2.1): Alt+LMB rotate-in-place, Alt+RMB pan, Alt+MMB
  dolly; Ctrl+LMB orbit, Ctrl+MMB/RMB pan, wheel zoom — shared by every scene viewport. A corner XYZ orientation gizmo
  shows the camera facing.
- **Orientation math reworked** — ![Done][done]: `CameraEditor` now builds its orientation from a proper Euler
  quaternion, so a full 360° turn works and the view no longer shears (the old `{1,-pitch,-yaw,0}` convention warped
  past ~180°).
- Still to do: per-axis sensitivity / dead zones under `Settings > Editor > Camera`, and an **interactive** view-cube
  for axis snapping (now feasible on the reworked orientation math).
- **Standard navigation presets** — quick buttons (ribbon `View` group + viewport overlay) for: **Reset View** (snap
  back to the default editor pose); axis-aligned ortho views **XY** (top-down), **XZ** (front), **YZ** (side);
  **Frame Selection** (zoom to fit the selected entity); **Frame Scene** (zoom to fit the whole scene's bounds).
- **Go to camera viewpoint** — snap the editor camera onto any selected `component::Camera` entity's pose
  (translation + rotation) for a quick preview. Disabled for cameras whose `RendererTag` targets a `RendererRaycast`
  layer (the editor renders those scenes flat — see "Look through scene camera" below).

## "Look through scene camera" mode (v0.5.0)

Let the user temporarily drive the editor viewport from any `component::Camera` entity in the scene (primary or
otherwise), for previewing what the runtime camera will see without entering Play. Toggleable from the camera entity's
context menu or a viewport overlay dropdown. Reverts to the editor camera on demand.

## Custom ImGui-based file picker (v0.5.0)

- Replace the native file dialogue (NFD/GTK) which briefly freezes the UI on Linux when GTK initializes (triggers IDE
  "antiloop" detection)
- Pure ImGui implementation integrated with the task scheduler for async folder scanning
- Benefits: consistent look-and-feel, truly non-blocking, theme-aware
- Replaces the current sync `FileDialog::openFile/saveFile/pickFolder` blocking calls
- Revisit if the SDL3 evaluation of v0.3.0 adopts SDL dialogues (see [Windowing and input](windowing-input.md))

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
