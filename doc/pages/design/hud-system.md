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

## `.owlhud` asset (HUD2, done)

- A HUD is an `.owlhud` file (scene entity format, own `DocumentFormat`) loaded into its own scene by a `HudLibrary`
  cached per path in `EngineContext`: shared by several scenes, kept across scene transitions.
- A scene references it with `Hud: huds/gameplay.owlhud` (empty for none); `owl_project.yml` may set a `DefaultHud`.
- Canvases embedded in a scene stay legal (full-screen menu scenes) and draw through the same pass; `RendererTag` is
  ignored on them.
- Lua: `ui.find(name)` searches the active HUD then the scene; `scene.find_entity` falls back to the HUD.
- Saves record the HUD path and its widget values. Content Browser kind and thumbnail.

Implementation notes:

- `SceneSerializer(scene, DocumentKind::Hud)` reads and writes the file: root key `Hud`, `FormatVersion` of its own
  `DocumentFormat` (`SceneSerializer::format(DocumentKind::Hud)`, version 1, no migration yet), the entities only. A
  scene remembers the document it was loaded from (`Scene::getDocumentKind`), so every save goes back to the right one.
- `HudLibrary::get(path)` reads the open pack, then the asset folders, then the plain path; a failure is logged once and
  cached. A scene loaded from an `.owlhud` has an explicit empty `Hud:`: a HUD never draws another one.
- `Scene::getHudPath()` is nothing (project `DefaultHud`), empty (no HUD) or a path; `Scene::getHud()` resolves it.
- `HudPass` draws the scene's canvases, then the HUD's. `UiInputSystem` lets a HUD widget take the mouse before the
  scene's own. The HUD scene does not run its scripts: a button callback runs on the nearest script above it, else on
  the first script of the active scene defining it.
- Saves add `HudData` (the effective path and the HUD entities); loading puts the widget values back by UUID into the
  shared HUD scene. Widget UUIDs of a HUD and of the scenes using it should not collide (Lua ids look in the scene
  first).
- The exporter follows `Hud:` keys and `DefaultHud` (`runner.yml` carries it) and packs the HUD with its textures and
  fonts. The editor reads the HUD files again after a save and when Play starts or stops.

## Management-game widgets (HUD3, done)

- `UiWindow`: a title bar over the body; the title bar drags the window (kept inside the screen), a click raises it
  (it becomes the last child of its parent, hence drawn last), the close button hides it (`Visibility`, Lua
  `ui.set_visible` shows it again, `onCloseCallback` runs first). The rectangle includes the bar; the children live in
  the content area under it and are clipped to it.
- A window under the mouse blocks every widget under it, in the scene and in the HUD, and the world
  (`UiInputSystem::isUIConsuming`); so does a drag in progress.
- `UiLayout`: row, column and grid arrangement of the children that carry a `UiRect`, with spacing, padding, a cross
  alignment and, for a grid, a column count and an optional cell size. The children keep their `size`; their anchors
  and offset no longer apply.
- Clipping by scissor for windows and for `UiPanel::clipChildren`: the layout gives every node the clip rectangle of
  its ancestors (`HudNode::clip`), the pass sets it on `Renderer2D` and hit-testing stops at it too.
- `UiTooltip`: text after a hover delay, sized to the text, kept inside the screen, drawn over every window; it
  belongs to the widget and to its descendants without a tooltip of their own, and hides while the button is down.
- `UiToolCursor`: a sprite following the mouse in the HUD, active from Lua (`ui.set_tool_cursor`), paired with the tile
  highlight of the isometric layer for construction tools ("build road", "raise land").
- Each window draws as its own batch (`Renderer2D::nextBatch` between two windows) so a window covers the text of the
  window under it, until the v0.5.0 draw-order sort key.

Implementation notes:

- `HudLayout` is still computed once per frame. A node gains `content` (the area its children live in), `clip` /
  `clipped`, `clipsChildren` and `window` (index of the nearest window above it, itself included, which the pass uses to
  break batches). A window is clamped to the screen in the layout, and the drag clamps the stored offset too, so the
  window follows the mouse again as soon as it comes back.
- `UiInputSystem::update` takes the frame time (tooltip delay). Window state (drag, tooltip, mouse position) lives in
  the active scene's `UiInputState`, with a flag telling whether the entity belongs to the HUD asset.
- Scissor in the RHI: `RenderAPI::setScissor` / `clearScissor` (Y-down rectangle plus the target height, OpenGL counts
  from the bottom), applied by `Renderer2D::flush` inside the open batch and lifted at its end; on Vulkan it is the
  dynamic scissor of the open render pass.
- Scene and `.owlhud` files list the siblings of a parent in the reverse of their hierarchy order (the serializer
  walks the entity storage backwards): the last window listed in a file is the first child, hence the one drawn
  under the others.

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
