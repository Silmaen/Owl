# Sample: Wolfenstein mini {#page-sample-wolfenstein}

[TOC]

**Level:** Beginner. **Read first:** [Sample Games](samples.md).

| Title screen                                                        | In game                                                       |
|---------------------------------------------------------------------|---------------------------------------------------------------|
| ![Wolfenstein mini title](../images/showcase/wolfenstein_title.png) | ![Wolfenstein mini floor](../images/showcase/wolfenstein.png) |

An exploration game in the spirit of Wolfenstein 3D: three floors of 32x32 cells drawn by the raycast renderer, with
treasure, food, keys and locked doors, secret walls and an elevator on each floor, held by guards; four weapons, ammo
and first aid kits. At the end of a floor a tally counts the items, the secrets, the share of the map explored and the
guards killed. The project is
`samples/wolfenstein`; play `scenes/title.owl`.

## What it brings

The reference for the raycast style, see [Raycaster Method](raycaster_method.md).

- **Raycast renderer:** the walls come from `.owltilemap` files, doors are `RaycastDoor` entities and secret walls
  `RaycastPushWall` entities; a `RendererRaycast` layer draws them.
- **Triggers and physics:** pick-ups and locked doors are `Trigger` entities counted by their tag (`Cross`,
  `GoldKey`, `Secret`...), the player is a `PhysicBody`, see [Physics](physics.md).
- **Behaviour trees:** each guard runs `scripts/game/guard.owlbt` (flinch, fire, chase when alerted, patrol), its
  leaves the exported functions of `guard.owlvs`, see [AI](ai.md#ai-trees).
- **Ray queries:** the line of sight and the shots are `physics.raycast` queries (`scripts/game/tactics.lua`), the
  chase a walk over the wall grid of the floor.
- **Sprite sheets:** a guard is an `AnimatedSpriteRenderer` of 8 x 7 frames (8 directions, walk, pain, death, fire)
  whose frame its graph picks from the view; the weapon in hand is a frame of a `UiImage` sheet
  (`ui.set_image_frame`); a guard shot dead spawns its corpse and an ammo clip (`scene.spawn_prefab`).
- **Status bar:** the bar of Wolfenstein 3D along the bottom (floor, score, face, health, ammo, keys, weapon), a row
  of `UiPanel` boxes. `scripts/game/status.owlvs` picks the frame of the face (bloodier as the health drops, wincing
  on a hit, grinning at a new gun) and of three red sheets over the view: the flash of a hit, the glow on the side it
  came from and the vignette of low health (below 25 %); a hit also shakes the view, unless reduced motion is on.
- **Particles in the raycast view:** the sparks of a pick-up are drawn by the `raycast_fx` particle layer.
- **HUD markers:** a key and the elevator switch get a `HudMarker` (name, distance, an arrow on the screen edge)
  once seen; the minimap is a tilemap that fills as the player looks around.
- **Menus:** the DOS menus of 1992 over a turning raycast view: blood red windows with a hard shadow, grey entries in
  the pixel font `Jersey10` (`fonts/`), and under the selected entry a red bar with a skull: a nine-slice texture
  shown only in the hover colour of the button, its focus frame transparent.
- **Visual scripts:** every script is a graph (`.owlvs`); the fog of war of `explore.owlvs` stays in Custom Lua
  nodes.
- **Sequences:** the elevator ride is `sequences/elevator.owlseq`, see [Sequences](sequences.md).
- **Accessibility:** the settings hold a colour vision correction, reduced motion and subtitles of the secret walls
  and the elevator (`ui.show_subtitle`).
- **Saves:** the floor is saved when it starts and on F6; **Continue** loads the newer, see
  [Lua API: Saves](lua-api-saves.md).

## Files

| Path         | Content                                                                       |
|--------------|-------------------------------------------------------------------------------|
| `scenes/`    | `title`, `level_1` to `level_3`, `tally`, `settings`, `victory`               |
| `scripts/`   | `game/` (player, guards, arsenal, pick-ups, doors, secrets, fog, exit), `ui/` |
| `prefabs/`   | `clip`, `guard_corpse`                                                        |
| `tilemaps/`  | The walls of each floor and its minimap                                       |
| `sequences/` | `elevator.owlseq`                                                             |
| `huds/`      | `play.owlhud`: status bar, red of the hits and of low health, pause window    |

The guard sheet is *The Guard* and the weapon sheet *FPS weapons overlay*, both CC0 on OpenGameArt, sliced by
`tools/samples/wolfenstein/generate_combat.py`. To add a guard, duplicate one in Owl Nest and set its patrol cell
(`patrol_c`, `patrol_r`, -1 to stand). To add a floor, duplicate a level scene and set the `next_scene` property of its `ExitElevator` script on the floor
before.
