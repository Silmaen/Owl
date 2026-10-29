# Skinned meshes, levels of detail and sockets {#page-skinned_meshes}

[TOC]

A glTF file can carry a skeleton, its animations and named attachment points besides its geometry. Owl draws such a
mesh skinned on the GPU, plays its clips through a state machine, snaps other entities on its sockets, and swaps any
mesh for coarser copies when it covers little of the screen. Design and decisions:
[Skeletal animation](design/skeletal-animation.md).

## Import

`MeshLoader` reads, from a `.gltf` or `.glb`:

| Content  | Read from                                                      | Kept as                                         |
|----------|----------------------------------------------------------------|-------------------------------------------------|
| Skeleton | The first skin: its joints, their nodes, `inverseBindMatrices` | `data::animation::Skeleton` (256 bones at most) |
| Skin     | `JOINTS_0` and `WEIGHTS_0` of the skinned primitives           | `VertexSkin` per vertex, four joints at most    |
| Clips    | The animations whose channels target joints                    | `data::animation::SkeletalClip`                 |
| Sockets  | The nodes named `socket_<name>`                                | `MeshSocket`: name, parent bone, offset         |

Skinned primitives stay in bind space (glTF ignores the transform of their node); the static primitives of the file are
placed by their node chain as before. Step keys hold, linear keys interpolate (rotations on the shortest arc), cubic
spline keys are read as linear.

## Animator

Add an **Animator** to an entity whose **Mesh Renderer** draws a skinned mesh. Its state machine has:

- **Parameters**: numbers, flags and triggers the game sets (`animator.set_float`, `animator.set_bool`,
  `animator.trigger`)
- **States**: a clip of the mesh, a speed and a loop flag; **Add a State per Clip** fills the list from the file
- **Transitions**: from a state (or `*`, any state) to another, with a cross-fade length, an optional exit time
  (normalized time of the source clip) and conditions on the parameters; the first one that holds fires

```lua
function on_update(dt)
    animator.set_float(entity_id, "speed", input.get_action_value("move_forward"))
    if input.is_action_just_pressed("jump") then
        animator.trigger(entity_id, "jump")
    end
end
```

**Preview** plays the machine in Edit mode, so the clips and the fades can be checked without starting the game. The
viewport draws the bones of the selected entity and its sockets.

## Sockets

A socket is a named point of a mesh or of an entity, optionally bound to a bone: the `socket_*` nodes of the file, and
the **Mesh Sockets** component of the entity (name, bone, offset), which a designer edits in the inspector or in the
viewport: click the marker of a socket (or its **Move in the viewport** button in the inspector) to select it, drawn
larger in yellow, and the transform gizmo moves, turns or scales it relative to its bone (the entity without bone);
the drag is one undoable `entity.set_socket` on release. The sockets of the mesh file are selectable but read-only. An
entity is
snapped on a socket with **Attach to Socket** in the context menu of the hierarchy (the `entity.attach_socket`
command), or from Lua:

```lua
socket.attach(sword_id, hero_id, "hand_r")   -- the sword becomes a child of the hero and follows the hand
socket.detach(sword_id)                      -- it stays where it is, under the hero
```

The attached entity carries a **Socket Attachment**: its local transform follows the socket every frame, after the
animation. Release it with **Release from Socket**.

## Levels of detail

A mesh gets reduced copies in the **Import** panel of the content browser: **Levels of detail** (0 to 4), the
**Reduction** of the triangles from one level to the next, and the **Screen size** under which the first reduced level
is drawn (each next level at half of it). The settings live in the `.owlmeta` sidecar of the mesh (`asset.set_import`
with `lods`, `lod_reduction`, `lod_screen_size`). The levels keep real corners of the mesh (vertex clustering), so
their texture coordinates, normals and skin stay valid.

The cook builds the levels into the packed mesh; Owl Nest builds them when it loads the file, and the **Levels of
detail** section of the mesh document draws each one with its triangle count. At run time the mesh layer draws the
coarsest level whose screen size is above the fraction of the screen height the mesh covers, seen from the camera.

## Mesh document

Opening a mesh shows, besides its geometry, the **Skeleton** tree (a bone selected is highlighted in the preview), the
sockets of the file, the **Animation** section (pick a clip, play it, scrub it on the timeline) and the levels of detail.
