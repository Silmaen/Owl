# Viewport and Gizmos {#page-editor-viewport}

[TOC]

**Level:** Beginner. **Read first:** [The Owl Nest window](editor.md).

Each scene tab is a viewport: the scene drawn by the engine, with the editor camera, the gizmo of the selection and a
few translucent overlays. Scenes can sit side by side: drag a scene tab out of the tab row.

## Overlays

| Place        | Controls                                                                                                                                                                          |
|--------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Top left     | Gizmo mode (Select, Move, Rotate, Scale), Local / World, **Snap** and its step (a click: presets)                                                                                 |
| Top centre   | Play (Resume), Pause, Step, Stop; while playing a chevron (`F10`) folds them into a corner handle                                                                                 |
| Top right    | The view (**2D**, **3D**, or **Iso**), the navigation modes and the fly speed, **Show**, the isometric **Navigation tool**, **Frame selection**, the **Camera** menu, frame stats |
| Bottom left  | CPU and GPU time of a frame, the waits for the display excluded: a click opens Stats                                                                                              |
| Bottom right | The view-cube; above it the preview of the selected camera                                                                                                                        |

**Show** toggles the grid, the camera, light and reflection probe markers (a probe: its capture point and its
volume, brighter selected), the trigger icons, the camera preview, the HUD (Edit only,
Play always draws it) and, in the isometric mode, the navigation graph (Shift-click tests a path); its **Editor ambient** slider (0 to 2) adds a grey light to
the lit 3D surfaces in Edit only, never in Play. **Colour vision preview** shows the whole frame, HUD included, as a
player with protanopia, deuteranopia or tritanopia sees it, in Edit and in Play, for this session
([Accessibility](design/accessibility.md#accessibility-colour-vision)).

A viewport hidden behind another tab or collapsed does not draw its scene (a running one still steps), unless the MCP
server is on: its captures read the viewport back.

A selected entity with a skinned mesh shows its bones, and its sockets as small crosses: a click on a socket selects it
(larger, in yellow) and the gizmo moves the socket instead of the entity, relative to its bone; a click elsewhere gives
the gizmo back to the entity ([Skinned meshes](skinned_meshes.md)).

The gizmo and view overlays show in Edit only. In Play a frame in the play colour outlines the viewport (see
[Play mode](editor_play.md)).

## Navigating the scene {#viewport-navigation}

**In short:** hold the **right** button and move the mouse to look around; while it is held, `W` `A` `S` `D` fly
forward, left, back and right, `Q` and `E` go down and up. The **middle** button pans, the **wheel** zooms, `F`
frames the selection. To move the view with the left button instead, pick a mode with the four buttons of the top
right overlay: **Orbit**, **Fly** or **Pan** (the arrow, **Standard**, gives the left button back to the selection).

The view moves only while the viewport is hovered, in Edit. The fly keys follow the key positions (Z Q S D on an
AZERTY keyboard); while a button flies, the gizmo letters are off.

### Gestures of every mode

| Gesture                     | Effect                                                     |
|-----------------------------|------------------------------------------------------------|
| Right drag                  | Look around from the eye                                   |
| Right drag + W A S D        | Fly forward, left, back, right (3D view)                   |
| Right drag + Q / E          | Fly down / up                                              |
| Right drag + Shift          | Fly three times faster                                     |
| Right drag + wheel          | Set the fly speed (0.1 to 100 units/s)                     |
| Middle drag                 | Pan                                                        |
| Wheel                       | Zoom towards the point the view turns around               |
| Ctrl + left drag            | Orbit around the point the view turns around               |
| Ctrl + right or middle drag | Pan                                                        |
| Alt + left drag             | Look around from the eye                                   |
| Alt + right drag            | Pan                                                        |
| Alt + middle drag           | Dolly (the eye and the point it turns around move forward) |
| Right click without drag    | Context menu (below)                                       |
| `F` / `Shift+F` / `Home`    | Frame the selection / frame the scene / reset the view     |

### Navigation modes: what the left drag does

The buttons sit next to **2D** / **3D** in the top right overlay; their tooltips repeat the gestures. The mode is kept
in the editor settings and applies to every viewport. Each mode is also an action of the command palette
(**Navigation: Standard**, **Orbit**, **Fly**, **Pan**), without a factory key, bindable in
[Settings > Shortcuts](editor_settings.md).

| Mode         | Left drag                                      | Left click | Keys while the left button is held  |
|--------------|------------------------------------------------|------------|-------------------------------------|
| **Standard** | Nothing: the gizmo, the brushes, the selection | Selects    | Gizmo letters                       |
| **Orbit**    | Orbit around the point the view turns around   | Selects    | Gizmo letters                       |
| **Fly**      | Look around from the eye                       | Selects    | W A S D, Q / E, Shift, wheel: speed |
| **Pan**      | Pan, the scene follows the hand                | Selects    | Gizmo letters                       |

In **Orbit**, **Fly** and **Pan** a press on the gizmo still drives the gizmo, and a click that does not drag (less
than 4 pixels) selects on release; a drag moves the view and selects nothing. `Ctrl` and `Alt` drags keep their
meaning in every mode. The voxel brush and the isometric **Navigation tool** own the left button while they are on.

The rabbit button next to the modes sets the **fly speed** (0.1 to 100 units/s, kept in the settings, also in
**Settings > Camera**); the wheel during a flight changes it too. **Frame selection** (the focus button) centres the
view on the selection.

### Per rendering style

| View                                     | Differences                                                                                                                                                                                                                        |
|------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **3D** (meshes, voxels, planets)         | Everything above.                                                                                                                                                                                                                  |
| **2D** (sprites, tilemaps, raycast maps) | The view faces the XY plane and never turns: the right drag, the orbit and the fly keys do nothing; the middle drag and every left-drag mode pan, the wheel zooms.                                                                 |
| **Iso** (isometric layer)                | Middle or right drag pans, `Alt` + left drag pans, every left-drag mode pans; the wheel zooms by powers of two around the cursor; `F` centres the selection, `Home` goes back to the origin. No look-around, no fly, no view-cube. |
| Voxel brush on                           | Left places, right erases: the right and middle drags do not move the view, `Ctrl` and `Alt` drags still do.                                                                                                                       |
| Look through a sequence                  | The view shows the sequence camera and does not move until the option is off.                                                                                                                                                      |
| Piloting a camera                        | Every gesture above moves the piloted camera, one undo step per gesture.                                                                                                                                                           |

A raycast scene is edited on its top-down map, in the 2D or 3D view.

**Settings > Camera** scales orbit, pan and zoom, inverts the vertical drag, sets a dead zone and the fly speed
([Settings](editor_settings.md)).

The **2D** view faces the XY plane with the framing of the 3D view. The grid follows the snap step, its axes in
their colours: on the ground (`y = 0`) in 3D, on the XY plane in 2D and isometric (**Show > Grid on the ground** puts
the 3D grid on the XY plane too).

**Camera presets** (the **Camera** overlay menu, **View > Camera**, the command palette):

| Preset                         | Shortcut  | Effect                                                                     |
|--------------------------------|-----------|----------------------------------------------------------------------------|
| Reset View                     | `Home`    | The XY plane from +Z, ten units away                                       |
| Frame Selection                | `F`       | Centre and fit the selection                                               |
| Frame Scene                    | `Shift+F` | Fit the whole scene                                                        |
| Move World Origin Here         |           | Move the world origin to the point the view turns around, on the snap step |
| Move World Origin to Selection |           | Move the world origin to the selection, on the snap step                   |
| Selected Camera Viewpoint      |           | Look through the selected camera (not a raycast camera)                    |
| Pilot Selected Camera          |           | Render through the selected camera and move it with the view (below)       |
| Axis views                     |           | **Front**, **Back**, **Top**, **Bottom**, **Right**, **Left**              |

A move of the world origin (these presets, the **Origin** field of [Scene Settings](editor_scene.md), an undo) moves
the view with it, so nothing jumps on screen; the entities keep their place in the world
([World positions](scene.md#world-positions)). On a snap step, the grid and the snapping stay aligned with the world.

The view-cube does the same: a click on an axis end looks from it, on the centre resets.

## Selecting

A left click selects the entity under the mouse (sprites, tilemaps, isometric objects, voxel worlds, meshes, camera,
light and probe markers); a click on empty space clears the selection. The selection is shared with the Scene Hierarchy and
Properties and outlined in orange: its quad for a 2D entity, the silhouette of its meshes for a 3D one, dimmed where
other geometry hides it.

A planet is selected by a click on its terrain like a mesh. In a scene with a world partition the viewport outlines every
cell, the loaded ones in green ([World partition](world_partition.md)); the editor itself does not stream.

A right click without drag opens the context menu: on an entity **Frame**, **Duplicate**, **Hide in the Editor**,
**Delete** (and **Camera Viewpoint**, **Pilot Camera** on a camera); on empty space 2D / 3D, the grid, **Reset View**
and **Frame Scene**.

## Gizmos and snapping

| Mode      | Key | Gizmo                                |
|-----------|-----|--------------------------------------|
| None      | `Q` | Select only                          |
| Translate | `W` | Arrows and planes                    |
| Rotate    | `E` | Rings                                |
| Scale     | `R` | Boxes                                |
| All       | `T` | Translate, rotate and scale together |

The keys act while the viewport is hovered or focused and no drag is in progress. A gizmo drag is one undo step.

**Snap** (or `Ctrl` held during a drag) rounds translations to the snap step and rotations to the **Rotation Step**
(45° by default). With **Step from Tilemap** and a tilemap in the scene, the step is a multiple of its cell size
(presets `1/4 cell` to `10 cells`) and a dropped entity lands on the nearest cell centre; otherwise the step is in
world units (presets `0.25` to `10`). The settings are in **Settings > Gizmo Snap**.

### Splines {#viewport-splines}

The selected entity with a **Spline** shows its curve (green), its lanes (faded) and its control points (squares). Click
a point to select it and move it with the gizmo; **Ctrl+click** adds or removes a point from the selection and the
gizmo then moves the group (one `spline.move_points`). **Shift+click** on the curve inserts a point there without
changing the shape (`spline.insert_point`); **Delete** removes the selected points (`spline.delete_points`) instead of
the entity. A selected point of a Bezier spline shows its two handles: click one and drag it (`spline.set_point`).
Click elsewhere to drop the point selection and get the entity gizmo back. The inspector lists the same points, with
**Add Point**, **Insert After Selected** and **Delete Selected**, and the lanes.

### Gravity {#viewport-gravity}

The selected entity with a **Gravity Source** shows its radius (the surface) as a ring and its range as a second ring;
a `Uniform` source adds an arrow along its direction. One with a **Simulation LOD** shows the rings of its `radius` and
of `radius` plus `hysteresis`. A selected dynamic body shows its predicted path (600 fixed steps) under the gravity. The
overlay shows in Edit and in Play.

### Walk areas {#viewport-walk-areas}

The selected entity with a **Walk Area** shows its floors: the outlines in green, the holes in orange, the triangles in
faint green and the vertices as squares (the dashed blue lines are the two heights of its scale by depth). Click a
vertex to select it and move it with the gizmo; **Ctrl+click** adds or removes a vertex from the selection and the
gizmo then moves the group (one `walk.move_vertices`). **Shift+click** on an edge inserts a vertex there
(`walk.insert_vertex`), **Alt+click** inside a floor cuts a square hole there (`walk.add_hole`) and outside every floor
adds a square floor (`walk.add_polygon`); **Delete** removes the selected vertices (`walk.delete_vertices`, a ring left
with fewer than three goes with them) instead of the entity. Each gesture is one undo step. The inspector lists the
same floors, holes and vertices, with **Add Floor**, **Add Hole**, **Delete Hole** and **Delete Floor**; see
[Walkable areas](pathfinding.md#pathfinding-walk-areas). **Show > Navigation graph and routes** draws every walk area of
the scene and the paths of the walking agents.

### Sequences {#viewport-sequences}

The player previewed by an open **Sequencer** (else the selected entity with a **SequencePlayer**) draws the path of
each camera track (one colour per track, dimmed when muted), a small frustum at every key and a larger white one at the
playhead; the unsaved edits of the Sequencer show. The tools overlay has **Look through the sequence** (the view shows
the sequence camera at the playhead and stays at that pose when turned off) and **Key from view**: the pose of the
viewport camera becomes the key at the playhead of the selected camera track (`sequence.set_key` on a key already
there, else `sequence.add_key`). See [Cutscene sequences](sequences.md#sequences-editor).

The selected **Oscillator** shows its range and the selected **Patrol** its route: click a waypoint to move it with a
translation gizmo.

## Cameras and lights

- **Camera markers**: an icon, an arrow and the field of view in 2D; in 3D a camera model with its frustum and a
  triangle pointing to its up. Lights have markers too: a sun disc with its direction, a spot cone, a bulb lit or off;
  selected, a light shows its range or its cones. 2D lights show a disc, a bulb, their radius or cone.
  A particle emitter shows an orange sparkle facing the camera, brighter when selected; a click on it selects it.
- **Camera preview**: with a camera selected in Edit, the bottom-right corner shows what it sees, with a button that
  pilots it.
- **Piloting**: **Pilot Selected Camera** renders the view through the camera; every move of the view moves the
  camera, each gesture one undo step. **Stop** on the banner at the bottom, or Play, puts the editor view back.
  Raycast cameras and the isometric mode cannot be piloted.

## Isometric scenes

When the scene's renderer stack holds an isometric layer, the viewport draws it as in Play and locks the projection
(**Iso**, no view-cube): middle or right drag pans, the wheel zooms by steps around the cursor. The cell under the
cursor is outlined along its slope, the gizmo moves along the map axes, and a click on any slice of a building
selects its entity. The **Navigation tool** paints one-way roads, closed sides and slower cells
([Pathfinding](pathfinding.md)).

## Dropping assets

| Drop from the Content Browser    | Result                                                                       |
|----------------------------------|------------------------------------------------------------------------------|
| A prefab (`.owlprefab`)          | An instance of the prefab under the mouse                                    |
| A mesh (`.gltf`, `.glb`, `.obj`) | An entity with a Mesh Renderer, on the surface under the mouse or the ground |
| A material (`.owlmat`) on a mesh | Every slot of the mesh takes the material                                    |
| Any other file                   | Opens it in its document, as a double-click does                             |

A mesh needs a mesh layer in the renderer stack; when the scene has none, a dialog offers to add one to the project,
to place the mesh anyway (not drawn) or to cancel.
