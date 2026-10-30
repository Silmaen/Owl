# Hierarchy and Inspector {#page-editor-scene}

[TOC]

The right column edits the active scene: the **Scene Hierarchy** lists its entities, **Properties** shows the
components of the selection, **Scene Settings** holds what belongs to the scene as a whole. Every change is an undo
step.

## Scene Hierarchy

The entities of the scene as a tree: a child moves with its parent ([Transform hierarchy](scene.md#transform-hierarchy)).

- **Add** creates an empty entity, at the root or as a child of the selection. The search field filters by name
  (every word, case-insensitive) as a flat list.
- Each row shows a chevron to fold the children, the icon of the main component, the child count and, on hover or
  while hidden, the eye (hide in the editor) and the camera (hide in the game). Prefab instances are in the info
  colour, entities hidden in the editor greyed, the selection marked by an accent bar.
- Drag an entity onto another to make it its child; its world position is kept. A parent cannot become the child of
  its own descendant.
- A light shows a sun, a flashlight or a bulb, grey when switched off.

The context menu (on an entity, or on the background):

| Entry                          | Effect                                                                                                                                 |
|--------------------------------|----------------------------------------------------------------------------------------------------------------------------------------|
| Create Root Entity             | A new entity at the root                                                                                                               |
| Create Child Entity            | A new entity under this one                                                                                                            |
| Light                          | **Directional**, **Point**, **Spot**, **Ambient 2D**, **Point 2D**, **Spot 2D**: a light entity, under the selection when there is one |
| Audio                          | **Reverb Zone**, **Adaptive Music** (states calm, danger, combat): an audio entity, under the selection when there is one              |
| Duplicate Entity               | Copy the entity alone (`Ctrl+D`)                                                                                                       |
| Duplicate Subtree              | Copy the entity and all its descendants                                                                                                |
| Create Prefab...               | Save the entity and its descendants as a `.owlprefab` file                                                                             |
| Update from Prefab             | Refresh the components not overridden from the prefab file                                                                             |
| Revert to Prefab               | Drop every override (the placement of the instance is kept)                                                                            |
| Unlink Prefab                  | Make the instance a plain entity                                                                                                       |
| Camera Viewpoint, Pilot Camera | On a camera: look through it, or move it with the view ([viewport](editor_viewport.md#cameras-and-lights))                             |
| Open in Tilemap Editor         | On a tilemap: open its `.owltilemap`                                                                                                   |
| Extract Canvas to .owlhud...   | On a root HUD canvas: move it to its own HUD file ([HUD editor](editor_hud.md))                                                        |
| Unparent                       | Move the entity to the root, keeping its world position                                                                                |
| Delete                         | Remove the entity; its children move up to its parent (`Delete`)                                                                       |
| Delete with Children           | Remove the entity and all its descendants                                                                                              |

The prefab entries show on a prefab instance only.

## Properties

The components of the selected entity, each under a header with its icon, its name and a **⋮** menu (**Revert this
component** on a prefab instance, **Remove component**). **Add Component** lists the components the entity does not
have yet; Transform and Hierarchy cannot be removed. Every field of every component is listed in
[Scene and components](scene.md#components); `F1` over a component header opens its page.

- Vectors sit in a label / value grid with the axis colours; a reset button (↺) appears once a value differs from its
  default.
- Asset fields (texture, font, sound, script, mesh, material, HDRI) take a file dragged from the Content Browser, show
  a swatch and the path, **Open** the asset in its editor and **Clear** it. A texture shows a thumbnail, a font a
  sample strip.
- A **Lua Script** lists the hooks of the script with **Open** at the line of each function, and its properties, each
  with a reset button.
- The **Speed Curve** of an animated sprite is a curve editor: drag points, double-click adds one, right click removes
  it.
- A **Mesh Renderer** lists one **Slot** per submesh: its material (drop a `.owlmat`, clear it for the file's own
  material) and the fields it overrides for this entity only.

An edit is one undo step when it ends: drag released, text validated, box ticked, entry chosen, asset dropped. Rapid
edits of one field merge into one step.

## Prefabs

A prefab is an entity subtree saved as a `.owlprefab` and reused across scenes.

| Gesture                       | Effect                                                                               |
|-------------------------------|--------------------------------------------------------------------------------------|
| **Create Prefab...**          | Save the selection and its descendants as a prefab file                              |
| Double-click the file         | Open the prefab as a document of its own: edit it like a scene, **Save** writes it   |
| Drag the file to the viewport | Place an instance, linked to the file                                                |
| Edit an instance              | The edited component is marked overridden (an accent bar in Properties)              |
| **Revert this component**     | Take the prefab's value back for one component; undoing an edit removes its override |
| **Update from Prefab**        | Bring the instance up to the file: new entities appear, removed ones go              |
| **Revert to Prefab**          | Drop every override but the placement                                                |
| **Unlink Prefab**             | Keep the entities, forget the file                                                   |

The root's Transform belongs to the instance: moving an instance is never an override. The Engine Demo's
`platformer_house.owl` holds two instances of `prefabs/coin_pair.owlprefab`; the second overrides the colour of one
coin.

## Scene Settings

**Scene > Scene > Settings** opens the settings of the active scene, as a tab beside Properties. Every change is an
undo step.

- **Physics**: sub-steps, solver threads (`auto` = 0) and interpolation of the 2D physics ([Physics](physics.md)),
  read when Play starts.
- **World Origin**: the floating origin (on / off), its shift radius and the **Origin** itself, moved with
  `scene.set_world_origin` (undoable; the roots keep their place in the world); entities with a **World Position**
  component keep their position in double ([World positions](scene.md#world-positions)).
- **World Partition**: the streaming of the scene's cells ([World partition](world_partition.md)): the enable switch,
  the cell size, the load and unload radii, and the cell table (coordinates, scene, state while playing). Drop a scene
  on the table to add a cell at the typed coordinates, on a row to change its scene; select rows to **Remove** them or
  **Move selected** by an offset in cells; **Open** opens a cell scene. Every change is an undo step
  (`scene.set_world_partition`, `scene.add_world_cell`, `scene.set_world_cell_scene`, `scene.remove_world_cells`,
  `scene.move_world_cells`).
- **HUD**: the HUD drawn over the scene: the project default, none, a project file or a dropped one.
- **Script**: the script of the scene itself ([Scene script](scripting.md#scene-script)): its file, **Open**, the
  hooks and the properties, as for a Lua Script component.
- **Renderer stack**: the layers that draw this scene, in order.

| Control                    | Effect                                                                       |
|----------------------------|------------------------------------------------------------------------------|
| Layer row                  | The layer name and its type (`Renderer2D`, `RendererRaycast`, …)             |
| Enabled switch             | Skip the layer in this scene; the order stays                                |
| Up / Down chevrons         | Reorder: listed layers run in this order, the others follow in project order |
| Detach                     | Remove the scene's entry: the layer takes the project's configuration back   |
| **Overrides**              | The keys of the layer type this scene changes                                |
| **+** next to a layer name | Add a project layer the scene does not list yet                              |

The overrides per layer type: **Renderer2D** `Space` (`World`, or `Screen` for overlays the camera never moves) and
`ReferenceHeight`; **RendererRaycast** field of view, distance, rays, ceiling and floor colours or tiles, fog;
**RendererIsometric** tile size, `ZStep` and the zoom of Play; **RendererMesh** occlusion culling and lighting;
**RendererVoxel** sun, ambient and their intensities. The layers themselves are described in
[Renderer](renderer.md#renderer-stack).

## Mixer

**Scene > Scene > Mixer** opens the mix of the active scene, docked with the bottom panels: one strip per bus
(Master, Music, SFX, Ambient, Voice) with its fader, **M** (mute), **S** (solo) and a meter of the bus gain now
([Mixer](sound.md#sound-mixer)), then the **Doppler factor**. In Edit mode a change runs `scene.set_mixer` (an undo
step per bus, however long its fader moves, saved as `Mixer:` with the scene, heard at once in the previews); while the scene plays it runs
`sound.set_live_bus`, which changes the live mixer only and ends with Play.

A **Reverb Zone** draws its volume in the viewport, brighter when selected with the edge of its fade; move, turn and
resize it with the gizmo. Its inspector picks the shape, the preset (or **Custom** parameters) and the zone filter; a
**Sound Source** picks its bus, its filter (low-pass, high-pass, band-pass) and, spatial, its Doppler; an **Adaptive
Music** lists its layers (name, asset, volume) and its states (a checkbox per layer, a stinger), with add and remove
buttons.
