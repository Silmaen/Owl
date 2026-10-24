# HUD Editor {#page-editor-hud}

[TOC]

A HUD is the screen layer of a game: scores, health bars, menus, dialogs. It is a tree of widgets under a
**Canvas**, kept in an `.owlhud` file shared by several scenes, or inside a scene. The HUD editor lays it out in
reference pixels, as the player will see it.

![The HUD editor on the gameplay HUD of the Engine Demo, the score label selected](../images/nest/hud_editor.png)

## Opening it

- An `.owlhud` opens in the HUD editor: a scene tab whose hierarchy, inspector and undo work as for a scene;
  **Save** writes the file. **Home > Create > New > HUD** creates one holding a full-screen canvas.
- In a scene with canvases, **Scene > Scene > HUD Editor** swaps the viewport for the HUD editor; press it again to
  go back to the scene view. Play always shows the scene.
- The HUD a scene draws is set in [Scene Settings](editor_scene.md#scene-settings), the project default in the
  [Project Settings](editor_projects.md#project-settings). **Extract Canvas to .owlhud...** (Scene Hierarchy menu of a
  root canvas) moves an in-scene canvas to its own file, one undo step.

The scenes drawing a HUD read it again when it is saved; Play reads the HUD files again at its start and end, so
nothing the game changed stays.

## Layout

- **Toolbar**: the preview resolution (presets or custom), fit / 1:1 (the wheel zooms, a middle drag pans, `F`
  fits), outlines and the reference frame, the backdrop (a colour or a dropped image), snapping (pixel grid, edges),
  draw order and delete.
- **Palette**: Canvas, Panel, Label, Image, Button, Slider, Progress Bar, Window, Layout, Scroll View, List. A click
  adds the widget at the centre; a drag drops it into the panel, window or scroll view under the mouse, else into the
  canvas (created when missing). A texture dragged from the Content Browser makes an Image.
- **Preview**: a click selects, `Ctrl`-click toggles, a drag on empty space selects with a box. The selection is the
  Scene Hierarchy's.

| Gesture                                    | Effect                                                  |
|--------------------------------------------|---------------------------------------------------------|
| Drag the body                              | Move the selected widgets together                      |
| Drag an edge or a corner handle            | Resize; the opposite edge stays                         |
| Alt + drag the circle                      | Move the pivot; the widget stays where it is            |
| Drag a diamond (Ctrl+Alt when under pivot) | Move the anchors; the widget stays where it is          |
| Arrow keys (Shift: 10 pixels)              | Nudge the selection                                     |
| Delete                                     | Delete the selection and its children                   |
| Shift while dragging                       | Snap nothing                                            |
| Right click                                | Add a widget here, bring to front, send to back, delete |

Positions snap to whole reference pixels and to the edges and centres of the siblings, the parent and the screen (red
guides). Every gesture is one undo step.

## Widget properties

The inspector edits each widget:

- **Canvas**: sort order, reference resolution, scale mode (`ConstantPixels`, `ScaleWithHeight`, `MatchSmallerSide`);
  the focus can wrap around.
- **UI Rect**: anchor preset (Custom when stretched), Anchor Min / Max, Pivot, Width, Height, Offset X / Y in
  reference pixels or, with the unit selector, a ratio of the parent or of the screen (changing the unit keeps the
  widget in place).
- **UI Text / Image / Panel / Button / Slider / Progress Bar**: their content, colours and the Lua handler a button
  or a slider calls.
- **UI Window / Layout / Tooltip / Tool Cursor**: a draggable closable window, a row, column or grid, a hover
  tooltip, a sprite that follows the mouse.
- **UI Focus**: focusable, initial, its neighbours by name, its callback and the colour of its frame, for pads and
  keyboards.

The widgets, their fields and their Lua side: [Scene and components](scene.md#components) and
[Scripting](scripting.md).
