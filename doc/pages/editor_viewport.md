# Viewport and Gizmos {#page-editor-viewport}

[TOC]

Each scene tab is a viewport: the scene drawn by the engine, with the editor camera, the gizmo of the selection and a
few translucent overlays. Scenes can sit side by side: drag a scene tab out of the tab row.

## Overlays

| Place        | Controls                                                                                                                                  |
|--------------|-------------------------------------------------------------------------------------------------------------------------------------------|
| Top left     | Gizmo mode (Select, Move, Rotate, Scale), Local / World, **Snap** and its step (a click: presets)                                         |
| Top centre   | Play (Resume), Pause, Step, Stop; while playing a chevron (`F10`) folds them into a corner handle                                         |
| Top right    | The view (**2D**, **3D**, or **Iso**), **Show**, the isometric **Navigation tool**, **Frame selection**, the **Camera** menu, frame stats |
| Bottom left  | CPU and GPU time of a frame, the waits for the display excluded: a click opens Stats                                                      |
| Bottom right | The view-cube; above it the preview of the selected camera                                                                                |

**Show** toggles the grid, the camera, light and reflection probe markers (a probe: its capture point and its
volume, brighter selected), the trigger icons, the camera preview, the HUD (Edit only,
Play always draws it) and, in the isometric mode, the navigation graph (Shift-click tests a path); its **Editor ambient** slider (0 to 2) adds a grey light to
the lit 3D surfaces in Edit only, never in Play.

A viewport hidden behind another tab or collapsed does not draw its scene (a running one still steps), unless the MCP
server is on: its captures read the viewport back.

A selected entity with a skinned mesh shows its bones, and its sockets as small crosses: a click on a socket selects it
(larger, in yellow) and the gizmo moves the socket instead of the entity, relative to its bone; a click elsewhere gives
the gizmo back to the entity ([Skinned meshes](skinned_meshes.md)).

The gizmo and view overlays show in Edit only. In Play a frame in the play colour outlines the viewport (see
[Play mode](editor_play.md)).

## Moving the view

| Gesture                     | Effect                                       |
|-----------------------------|----------------------------------------------|
| Right drag                  | Look around from the eye                     |
| Middle drag                 | Pan                                          |
| Ctrl + left drag            | Orbit around the reference point             |
| Ctrl + right or middle drag | Pan                                          |
| Alt + left drag             | Look around from the eye                     |
| Alt + right drag            | Pan                                          |
| Alt + middle drag           | Dolly (the eye and the reference point move) |
| Wheel                       | Zoom towards the reference point             |
| Right drag + W A S D        | Fly forward, left, back, right (3D view)     |
| Right drag + Q / E          | Fly down / up                                |
| Right drag + Shift          | Fly three times faster                       |
| Right drag + wheel          | Set the fly speed (0.1 to 100 units/s)       |

The view moves only while the viewport is hovered. The fly keys follow the key positions (Z Q S D on an AZERTY
keyboard); while the right button flies, the gizmo letters are off. **Settings > Camera** scales orbit, pan and zoom,
inverts the vertical drag and sets a dead zone.

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
