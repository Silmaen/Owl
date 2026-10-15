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

## HUD layer (HUD1, done)

- An implicit last pass of the frame, in pixel space, after the renderer stack and the overlays, before the toasts and
  the screen transition (`HudPass`); it cannot be misordered, whatever the layers underneath draw, in Play
  as in the editor viewport (where a click on a widget selects it). `Scene::renderHud` (voxel highlight and crosshair)
  became `renderOverlays`; `RendererTag` no longer routes a canvas, so the `ui` layer of the templates stays empty.
- The code left `Scene.cpp`: `HudLayout` lays the canvases out once per frame (in ascending `sortOrder`,
  depth first, a parent before its children) and `UiInputSystem` hit-tests that same layout, so a click lands on the
  rectangle that was drawn; the topmost button or slider under the mouse takes it, labels and panels over it do not.
- Nested `UiRect`: the nearest ancestor carrying a `UiRect` is the parent rectangle, an entity without one passes
  its parent on, a hidden entity hides its subtree.
- Y-down pixels with the origin at the top left; scene and prefab format 4 converts every `UiRect` written before
  (preset point to both anchors, `pivot.y = 1 - pivotY`, `offset.y = -offsetY`) and gives the legacy canvases
  `ConstantPixels`, so no widget moves; the sample project and the templates were rewritten and use
  `ScaleWithHeight`.
- Anchors: `anchorMin` / `anchorMax` (normalised, a distinct pair stretches the widget, `size` is then added to the
  span) besides the 9 presets (`UiRect::setAnchor`, `getAnchor`), which stay as shortcuts in the inspector.
- `Canvas::referenceResolution` (1280×720 by default) with `Canvas::scaleMode`: `ConstantPixels`, `ScaleWithHeight`
  (default) or `MatchSmallerSide`; widgets are authored in reference pixels, fonts scale with them.
- Every new field is edited in the inspector, undoable through `component.set`; bench `hud/{layout,frame}/500_widgets`
  (budget in [Isometric worlds](isometric-worlds.md)); image tests `hud_raycast` and `hud_voxel` at 320×180, 480×270
  and 240×240.

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
