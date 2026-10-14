# HUD system {#page-design-hud-system}

[TOC]

Design page for the v0.4.0 HUD overhaul and the text work it relies on, summarised in the [Roadmap](../roadmap.md).
The pull requests and their order are in [Isometric worlds](isometric-worlds.md) (TX1, TX2, HUD1 to HUD6).

## Goal

A HUD authored once and drawn on top of any rendering style (2D, raycast, voxel, isometric), in pixels, at any
resolution, edited in its own document in Owl Nest — and rich enough for a management game: several windows, toolbars,
tooltips and construction tools at once, as in Transport Tycoon.

## Today (v0.3.0)

- The HUD is a second `Renderer2D` layer named `ui`, in world space, and every UI entity carries `RendererTag: ui` by
  hand; in the editor it floats at world coordinates over the level.
- Only direct children of a `Canvas` draw and react to the mouse; the parent size is always the viewport, without
  resolution scaling; there is no keyboard or gamepad focus.
- HUD drawing and hit-testing live in `Scene.cpp` (C-14).

## Text (TX1, TX2)

- UTF-8 decoded to code points, `Font` API on `char32_t`, default charset Latin only (Latin-1 + Latin Extended-A +
  punctuation + €), extra ranges per font, atlas cached on disk.
- Glyphs drawn at their **native aspect ratio**: the pen advances by each glyph's metrics with one uniform scale per
  em; the rectangle governs layout (wrap width, line height, alignment), never the glyph shape.
- Horizontal (left / centre / right) and vertical (top / middle / bottom) alignment, optional word wrap, fit mode
  (clip / shrink to fit / overflow), on `UiText` and on world-space `Renderer2D::drawString`.

## HUD layer (HUD1)

- An implicit last pass of the frame, in pixel space, after the renderer stack and before the toasts; it cannot be
  misordered, whatever the layers underneath draw. The old `Scene::renderHud` (voxel highlight and crosshair) becomes
  `renderOverlays`.
- The code leaves `Scene.cpp` for its own module; one layout pass per frame feeds both drawing and hit-testing.
- Nested `UiRect`: a parent rectangle is its children's space.
- Y-down pixels with the origin at the top left, the most common UI convention; a scene migration converts every
  existing `UiRect` (today Y-up, pivot at the centre).
- Anchors: min / max anchors (stretch) besides the 9 presets, which stay as shortcuts.
- Reference resolution (1280×720 by default) with a scale mode: constant pixels, scale with height, match the
  smaller side.

## `.owlhud` asset (HUD2)

- A HUD is an `.owlhud` file (scene entity format, own `DocumentFormat`) loaded into its own scene by a `HudLibrary`
  cached per path in `EngineContext`: shared by several scenes, kept across scene transitions.
- A scene references it with `Hud: huds/gameplay.owlhud` (empty for none); `owl_project.yml` may set a `DefaultHud`.
- Canvases embedded in a scene stay legal (full-screen menu scenes) and draw through the same pass; `RendererTag` is
  ignored on them.
- Lua: `ui.find(name)` searches the active HUD then the scene; `scene.find_entity` falls back to the HUD.
- Saves record the HUD path and its widget values. Content Browser kind and thumbnail.

## Management-game widgets (HUD3)

- `UiWindow`: title bar, drag, close, raise on click, clamped to the screen; a window blocks world clicks under it.
- `UiLayout`: row, column and grid arrangement with spacing and padding.
- Clipping by scissor for windows and panels.
- `UiTooltip`: text after a hover delay.
- Tool cursor: a sprite following the mouse in the HUD, paired with the tile highlight of the isometric layer, for
  construction tools ("build road", "raise land").
- Until the v0.5.0 draw-order sort key, each window draws as its own batch so a window covers the text of the window
  under it.

## HUD editor (HUD4)

- `HudDocument` on `.owlhud` files and on embedded canvases: a pixel-space viewport with resolution presets and the
  reference resolution framed, over a colour or image backdrop (a live scene backdrop comes with "look through scene
  camera", v0.5.0).
- Rect gizmo snapped to pixels and anchors, anchor handles, widget palette, drag & drop of textures and fonts,
  hierarchy and multi-select.
- Every action is a named command (`hud.add_widget`, `hud.set_rect`, …): undoable, and reachable through the MCP
  server.

## Focus navigation (HUD5)

Keyboard and gamepad focus on buttons and sliders, driven by the `ui_up` / `ui_down` / `ui_left` / `ui_right` /
`ui_confirm` / `ui_cancel` input actions ([Windowing and input](windowing-input.md)).

## Migration (HUD6)

- An undoable "Extract canvas to `.owlhud`" command; the sample project and the project templates move to a shared
  `huds/gameplay.owlhud` and lose their `ui` layer; scene migration drops `RendererTag: ui` from UI entities.
- The empty raycast hint of `raycast_demo.owl` is fixed on the way.

## Inventory demo (DEMO1)

Pulled from v0.5.0: collectible objects, key-locked switches and an inventory window with slots in
`sample_project/`, in Lua only, navigable with the gamepad.

## Tests

Layout maths (anchors, pivot, nesting, scale modes), hit-testing and focus order, text layout (pen advance, wrap,
shrink, constant glyph aspect), `.owlhud` round-trip and legacy fixture, sharing between two scenes and across a
transition, Lua lookup, save and load, image tests of one HUD over 2D, raycast, voxel and isometric scenes at three
resolutions, the HUD editor commands, and a bench at 500 widgets.

## Later

- v0.5.0: scroll views and virtualised lists (vehicle lists), a minimap from a CPU tile-colour texture, the
  draw-order sort key across primitives, a live scene backdrop in the HUD editor.
- v0.7.0: docking and saved window layouts for in-game windows.
- v0.10.0: controller remapping UI on the HUD widgets.
