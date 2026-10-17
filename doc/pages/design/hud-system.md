# HUD system {#page-design-hud-system}

[TOC]

Design page for the v0.4.0 HUD overhaul and the text work it relies on, summarised in the [Roadmap](../roadmap.md).
The pull requests and their order are in [Isometric worlds](isometric-worlds.md) (TX1, TX2, HUD1 to HUD7).

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
- **Outline and background box** (default none, absent from the file): `outlineWidth` (reference pixels) and
  `outlineColor` draw a contour around the glyphs in the same draw as the text, `backgroundColor` (zero alpha = none) and
  `backgroundPadding` a box behind the measured text block. They keep white text readable on a light map. `Text` has the
  same four fields, in text heights. Edited in the inspector, undoable like any component edit.

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
  `ConstantPixels`, so no widget moves; the Engine Demo and the templates were rewritten and use
  `ScaleWithHeight`.
- Anchors: `anchorMin` / `anchorMax` (normalised, a distinct pair stretches the widget, `size` is then added to the
  span) besides the 9 presets (`UiRect::setAnchor`, `getAnchor`), which stay as shortcuts in the inspector.
- Relative units: each of `size.x`, `size.y`, `offset.x` and `offset.y` has a `UiRect::Unit`, `Pixels` (default,
  reference pixels, scaled by the scale mode) or a ratio (0.25 = a quarter) of `ParentWidth`, `ParentHeight`,
  `ScreenWidth` or `ScreenHeight` (the parent content area or the whole canvas). `HudLayout` resolves them in
  reference pixels at layout time against the parent and the canvas (the scale is uniform, so this is the ratio of the
  screen pixels too, in every scale mode); a square widget uses `ParentHeight` for both sizes, a child of a `UiLayout`
  is measured the same way. Files write `sizeUnit: [X, Y]` / `offsetUnit: [X, Y]` only when a number leaves the pixels,
  so older files and pixel widgets are unchanged (no format bump: absent means `Pixels`, an unknown name too).
  `UiRect::convertSizeUnit` / `convertOffsetUnit` change a unit keeping the screen rectangle, `moveBy` moves in
  reference pixels whatever the units (window drag, `hud.move`).
- `Canvas::referenceResolution` (1280×720 by default) with `Canvas::scaleMode`: `ConstantPixels`, `ScaleWithHeight`
  (default) or `MatchSmallerSide`; widgets are authored in reference pixels, fonts scale with them.
- Legibility floor: a scale mode that shrinks (scale under 1) never draws a text below 12 screen pixels of em height
  (a 16 px font at 640×360 draws at 12 px, not 8); the scale of that text is raised, never past 1, and its layout runs at
  that scale, so a box too small for it wraps or clips the text as for any other size. Authored sizes at scale 1 or
  more, and boxes, are untouched.
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
- Each window has a rank in the `Renderer2D` sort key (and, inside it, shapes under circles under text), so a window
  covers the text of the window under it without a batch of its own.

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

## HUD editor (HUD4, done)

- `HudDocument` (`document/HudDocument.h`) holds the state of the editor of a scene's canvases: preview resolution
  (presets 640 x 360, 1280 x 720, 1920 x 1080, 2560 x 1440, 1024 x 768, 720 x 1280, or a custom size), backdrop (a
  colour or an image), snapping, selection and the gizmo gestures. It owns no scene and no undo history: it edits the
  scene of a `SceneDocument`, an `.owlhud` or the canvases embedded in a scene, so the hierarchy, the inspector, the
  autosave, the hot reload and Save work as they do for a scene. A live scene backdrop comes with "look through scene
  camera" (v0.5.0).
- `panel::HudEditor` draws it in the tab of the document in place of the 3D viewport: an `.owlhud` always opens in it,
  a scene switches with **HUD Editor** of the Scene ribbon tab (Play brings the viewport back). The preview is the
  HUD drawn into its own framebuffer at the chosen resolution (`Scene::renderHud`), zoomed to fit or 1:1, panned with
  the middle button; the reference resolution of each canvas is framed.
- Rect gizmo on the primary selected widget: move (body), eight resize handles, the pivot (Alt), the anchors (diamonds on
  the parent rectangle; Ctrl+Alt when they sit under the pivot). Dragging the pivot, an anchor or an edge keeps the rest
  of the rectangle where it is on screen (`solveHudRect` finds the offset and the size). Snapping rounds to the pixel
  grid (step 1 to 64 reference pixels) and sticks to the edges and centres of the siblings, the parent and the screen
  (guides are drawn); Shift snaps nothing.
- Selection: click, Ctrl-click, box; shared with the Scene Hierarchy (the primary widget). Group move by dragging or the
  arrow keys (Shift = 10 pixels), Delete removes the group; a widget under another selected one follows it.
- Palette: Canvas, Panel, Label, Image, Button, Slider, Progress Bar, Window and Layout, by click (centre of the preview)
  or drag and drop into the container under the mouse; a texture dropped from the Content Browser makes an Image.
  Fonts are still assigned from the inspector.
- Every action is a named command of the registry (`hud.add_widget`, `hud.set_rect`, `hud.set_anchors`,
  `hud.set_pivot`, `hud.move`, `hud.delete`, `hud.reparent`, see [Editor](../editor.md#editor-commands)): undoable,
  journaled, replayable in macros and reachable through the MCP server. A gesture previews on the scene; when it ends
  the scene is put back and one command runs with the final values (a drag is one undo step, a nudge merges into the
  previous one within a second).
- The project `DefaultHud` stays a field of Project Settings, applied with the dialog: it is a project setting, not a
  scene edit, so it has no undo entry and no command yet (the dialog applies and saves the whole project at once).

Implementation notes:

- `HudRectsCommand` records the `UiRect` of one or several widgets before and after and merges with the next change of
  the same widgets; `HudReparentCommand` adds the parent and the index. Creation and deletion reuse
  `CreateEntityCommand`, `DeleteSubtreeCommand` and `UndoGroupCommand`.
- Positions in the commands are reference pixels, except the `size` and `offset` of `hud.add_widget` and `hud.set_rect`,
  which are read in `size_unit` / `offset_unit` (one unit name for both axes or two separated by a comma, the
  widget's own units when absent; the gizmo writes in each field's current unit through `solveHudRect`, and `hud.move`
  moves a ratio offset by the equivalent ratio, the unit kept); `keep_bounds` (default true on `hud.set_anchors`, `hud.set_pivot`
  and `hud.reparent`) lays the HUD out at `resolution` (the reference resolution of the canvas when absent) to read
  the parent rectangle.
- Tests: `HudDocument_test.cpp` (resolution, selection, hit-testing, gestures, snapping, creation by drop) and the
  `HudCommandsTest` cases of `CommandRegistry_test.cpp`.

## Focus navigation (HUD5, done)

Keyboard and gamepad focus on buttons and sliders, driven by the built-in `ui_up` / `ui_down` / `ui_left` /
`ui_right` / `ui_accept` / `ui_cancel` actions of the scene's action map (`UiInputSystem::navigate`, called by
`update` with `Scene::getActions()`; held directions repeat after 0.4 s, then every 0.1 s).

- **Where the focus goes.** A direction picks, among the focusable widgets of the scene then of its HUD, the one whose
  centre lies past the current one in that direction with the lowest `distance + 2 × sideways drift`, from the layout
  `HudPass` draws (`HudLayout`): what is seen is what is reached, a widget cut away by its clipping panel is not.
  No wrap-around unless the menu's `Canvas` has `wrapFocus`: with nothing ahead, the focus then jumps to the widget at
  the opposite end of that canvas (the left stick never navigates). A `UiFocus` names a neighbour by entity UUID per
  direction, which wins over the layout (a missing entity falls back to it); duplicating a subtree or instantiating a
  prefab points the neighbours inside the copy at the copies, like `EntityLink`. The inspector picks a neighbour by
  name among the UI entities of the scene (with a filter), undoable through `component.set`.
- **Who can take it.** A `UiButton` not disabled or a `UiSlider`, unless its `UiFocus` has `focusable` off. While a
  `UiWindow` is open the focus stays in the window drawn last (the HUD's over the scene's), and goes to the widgets
  behind when it closes.
- **Starting.** Nothing is focused until a `ui_*` direction or `ui_accept` is pressed: the first press only shows the
  focus, on the widget flagged `UiFocus::initial`, else the first one in hierarchy order. A widget flagged `initial`
  takes the focus, shown, as soon as the scene plays: a menu a gamepad can drive from the first frame.
- **Acting.** `ui_accept` runs the `onClickCallback` of the focused button; `ui_left` / `ui_right` change the focused
  slider by `UiSlider::step` (a twentieth of the range by default) and run its callback, unless the `UiFocus` names a
  neighbour on that side; `ui_cancel` closes the top window when it is closable. A cancel never starts the
  navigation, so the `pause` key of a game that shares Escape keeps working.
- **Mouse.** The mouse takes the focus where it moves or clicks and hides the highlight (`focusVisible`); the next
  key press shows it again, from the widget under the mouse. A motionless mouse never takes it back.
- **Look.** A focused button draws as hovered and a frame in the `UiFocus` colour and thickness (default gold, 2
  reference pixels) is drawn just outside it, over the windows, in the HUD pass overlay.
- **Lua.** `ui.set_focus(id)`, `ui.get_focus()`, and `UiFocus::onFocusCallback`, called with `_focused_entity` when the
  widget takes the focus.
- The state (`focus`, `focusInHud`, `focusVisible`, repeat) lives in `Scene::UiInputState`, like the hover state.

## Migration (HUD6, done)

- `hud.extract_canvas` (Scene Hierarchy context menu, [Editor](../editor.md#editor-commands)) writes a root canvas and
  its widgets to an `.owlhud`, removes them from the scene and sets the scene's `Hud:` key, as one undo step (a
  `DeleteSubtreeCommand` and a `SetSceneHudCommand` in a group). The file is written when the command is prepared and
  stays after an undo.
- Scene and prefab format 5 (`dropUiRendererTag`) removes `RendererTag: ui` from every entity carrying a `Canvas` or a
  `Ui*` component; a world entity tagged `ui` keeps its tag. Legacy fixture: `UiRendererTagMigration_test.cpp`.
- The project templates (`empty_2d`, `mixed`, `raycast`) each ship a `huds/gameplay.owlhud`; in `engine_demo/` the
  platformer keeps `huds/gameplay.owlhud` and the world map, the raycast demo and the isometric test have their own HUD
  (`world_map`, `raycast`, `voxel`) because their hints and widgets differ; the menus keep their embedded canvas.
  No shipped `.owl` or `.owlhud` names the `ui` layer (`EngineSceneTemplates.NoShippedWidgetNamesTheUiLayer`).
- The dead `UiPanel::layout`, `spacing` and `padding` are gone; old files keep loading (unknown keys are ignored).
- The empty raycast hint of `raycast_demo.owl` is fixed: its HUD now carries the walk, turn and interact keys.

## Scroll views and virtualised lists (HUD7, done)

- **`UiScrollView`.** The children are laid out in the viewport (the rectangle minus the bars the overflow needs),
  shifted by `scrollOffset`, and clipped to it by the same scissor as a window: a click, a hover or the focus reaches
  only what is in view. The content is `contentSize`, or, on an axis left at 0, the extent of the children a
  `UiLayout` of the same widget places (a column of buttons scrolls with no size to write), or else the viewport.
  `HudLayout` keeps the offset inside the content and writes it back to the component.
- **`UiList`.** A model of `itemCount` rows of `rowHeight` and `rowSpacing`, drawn by the pass: only
  `visibleRows` (the rows at least partly in view) are drawn, hit-tested and bound, whatever the count (a 1000-row list
  costs the same as a 10-row one). A row holds a text, a right-aligned detail and an icon (`UiListRow`), filled from
  Lua by `ui.list_set_row`; the engine calls `onBindCallback` once for a row in view that nobody filled
  (`_list_entity`, `_row_index`, 1-based), so a script builds the row it is asked for and nothing else. A row not filled
  shows `Item N`. `ui.list_invalidate` binds the rows in view again after the data changed. There is no row template
  entity: the rows are drawn by the engine, which keeps a row at a few quads.
- **Input.** The wheel scrolls the nearest container above the widget under the mouse that can move (`wheelStep`
  reference pixels per notch; a view that only scrolls sideways takes the vertical wheel); a bar is dragged by its
  thumb and a click on its track brings the thumb there. A click on a list row selects it and runs `onSelectCallback`
  (`_list_entity`, `_selected_index`, 1-based); the hovered row is lit. `UiInputSystem::update` takes the wheel of the
  frame (`Input::readState().wheel` in the runner, ImGui in the editor viewport).
- **Focus.** A list takes the focus like a button: `ui_up` / `ui_down` step the selection (and scroll it into view),
  and past the first or last row the focus moves on to the next widget. A widget scrolled out of a `UiScrollView` stays
  reachable (it is not cut away by a clipping panel): moving the focus to it scrolls the view just enough to show it,
  once per focus change and only while the focus is shown, so the mouse never scrolls by hovering. The new built-in
  `ui_scroll` action (right stick, Page Up / Page Down) scrolls the view or list holding the focus.
- **Look.** The bar track and thumb are drawn after the content of the view, over it, with the clip of the view.
- **Saves.** `scrollOffset`, `selectedIndex` and `itemCount` are component fields of the HUD entity, hence saved with
  its widget values; the rows are not, a script binds them again (they are asked for once the list is loaded).
- **Editor.** Scroll View (a panel with a column `UiLayout`) and List are in the HUD editor palette and in
  `hud.add_widget` (`scroll_view`, `list`); every field is in the inspector, undoable through `component.set`, and a
  widget dropped into a scroll view lands in its scrolled content. The gizmo and `keep_bounds` read the parent rectangle
  shifted by the scroll (`HudNode::childArea`). The preview shows `Item N` rows from `itemCount`.
- **Format.** New components only: scene and `.owlhud` files need no migration step.

Implementation notes:

- `scene/ui/ScrollMath.h` holds the pure maths (offset clamp, thumb length and position, reveal of a range, geometry of
  the viewport and bars, visible rows, row at a position); `HudLayout` adds a `HudScroll` per container
  (`getScrolls`, `HudNode::scroll`, `scrollOffset`, `scrollAncestor`) that the pass, the input system and Lua read.
- The pass draws the rows of a list in its own scissor (bounds clipped by the ancestors) and the bars of a scroll view
  once its last descendant is drawn.
- A bar is interactive for the hit-test, so it takes the click before the widgets and the world under it.
- Tests: `ScrollMath_test.cpp`, `HudScroll_test.cpp` (layout, clipping, hit-testing through the clip, wheel, bars,
  focus, virtualisation counted in quads), `LuaScrollList_test.cpp`, the `hud.add_widget` and `component.set` cases of
  `CommandRegistry_test.cpp`, bench `hud/frame/1000_row_list`.

## Inventory demo (DEMO1)

Done. Collectible objects and an inventory window with slots in `engine_demo/`, in Lua only,
navigable with the gamepad (now in the raycast level, `scenes/raycast_demo.owl`).

- **Window.** `huds/raycast.owlhud` is the scene's `Hud:` (one HUD per scene): a closable `UiWindow` holding a grid of
  nine `UiButton` slots (a `UiImage` icon and a `UiText` count each, a `UiTooltip` with the item name) and an info
  text. A slot exists per item; its icon and count show once the count is above zero (`ui.set_visible`, `ui.set_text`),
  so no image binding is needed.
- **Opening.** The `inventory` action (Tab, gamepad Y) shows the window and `ui.set_focus` puts the focus on the first
  filled slot; the window then keeps the focus, `ui_*` walk the slots (no wrap), `ui_accept` inspects, `ui_cancel`
  closes (`onCloseCallback`), `onFocusCallback` updates the info text. The player is still while it is open.
- **State.** The inventory is the gamestate keys `inv_<item>` (counts), so a save keeps it; `picked_<tag>` keeps taken items, F6 / F9 quick-save and load slot 2.
- **Journal.** A `UiList` under the info text lists every message of the game (pick-ups, doors, saves), grown by
  `inventory.lua` with `ui.list_set_count`, scrolled to the newest, its rows bound on demand (`onBindCallback`); a
  selected line shows in full in the info text.
- **Scripts.** `pickup.lua` (`Trigger.Type=LuaCallback`), `inventory.lua` (window, toast, save keys), `raycast_player.lua` (still while the window is open). Scenario
  `test/scenarios/raycast_level.owltest` picks an ammo clip, opens the window, navigates it and takes the elevator.

## Tests

Layout maths (anchors, pivot, nesting, scale modes), hit-testing and focus order, text layout (pen advance, wrap,
shrink, constant glyph aspect), `.owlhud` round-trip and legacy fixture, sharing between two scenes and across a
transition, Lua lookup, save and load, image tests of one HUD over 2D, raycast, voxel and isometric scenes at three
resolutions, the HUD editor commands, and a bench at 500 widgets.

## Later

- v0.5.0: a minimap on a render target, a live scene backdrop in the HUD editor.
- v0.6.0: docking and saved window layouts for in-game windows.
- v0.8.0: controller remapping UI on the HUD widgets.
