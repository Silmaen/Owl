# The Moon Lantern

A small point-and-click fairy tale made with Owl Engine, in the spirit of The Legend of Kyrandia: three rooms, one chain
of puzzles, objects picked, combined and used, no death and no dead end, and an ending. Painted pixel-art backdrops,
3D skinned actors walking on them, drawn as pixel art too (toon shading, the backdrop's resolution, an outline), scaled
by depth and hidden, whole, behind the parts of the backdrop in front of them. The
plan of the game is in [Sample games](../../doc/pages/design/sample-games.md#sample-games-point-and-click).

The Moon Lantern of the valley has gone dark, and tonight the fairies need it to find their way. Wren, apprentice of
the wizard Barnaby, has to find its moonstone and give it a spark.

## How to play

Open the folder in Owl Nest (**File > Sample games**, or **Open project**) and press Play, or export it as a game. The
cursor names what is under it and shows what a click does there: walk, look, pick up, use, talk or go; the object
under it glows. The hero walks to the spot and does it; the right button looks. Objects picked go into the bag: click
one to take it in hand (it becomes the cursor, the bag stays open), then click another object of the bag to combine
them (the result appears in the bag), or something of the room to use it there (a click on the room closes the bag).

| Action                   | Mouse                      | Keyboard     | Gamepad    |
|--------------------------|----------------------------|--------------|------------|
| Move the cursor          | Move                       | Arrows       | Left stick |
| Next / previous hotspot  |                            | Tab or E / Q | RB / LB    |
| Do what the cursor shows | Left button                | Enter, Space | A          |
| Look                     | Right button               | L            | X          |
| Open the bag             | Bag button                 | I            | Y          |
| Put the object back      | Right button               | Escape       | Start      |
| Close the bag            | Click the room, Bag button | I, Escape    | Y          |
| Pause (save, title)      |                            | Escape, P    | Start      |

The game is saved in its slot when the hero enters a room and from the pause window; **Continue** on the title screen
loads it. The settings hold the volumes, the language (English or French) and the subtitles of the sounds.

## Rooms

| Scene          | What is there                                                                                    |
|----------------|--------------------------------------------------------------------------------------------------|
| `title.owl`    | The glade at night, the hero waving, New game, Continue, Settings, Quit                          |
| `cottage.owl`  | Barnaby (a dialogue with choices), the jar on the shelf, the pie crust on the table, the lantern |
| `glade.owl`    | The berry bush, the fireflies over the pond, a fairy ring, the old oak the hero walks behind     |
| `bridge.owl`   | The troll sitting on the moonstone (a dialogue, a toll), the stream, the parapet in front        |
| `settings.owl` | Master, music and effects volumes, language, subtitles                                           |
| `ending.owl`   | The lantern lit: a sequence closes in while the end is told, then Barnaby dances                 |

The solution: the jar and the crust in the cottage, the berries in the glade, the jar on the fireflies, the berries
with the crust make a pie, the pie on the troll, the moonstone where he sat, then the moonstone and the fireflies in
the lantern.

## How it is made

| Part                | Where                                                                                                                                                           |
|---------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Renderer stack      | `actors` (`RendererMesh` as pixel art, 180 texels high), `world` (2D, `DepthTest`: backdrop, props, masks), `cursor` (screen)                                   |
| Floor of a room     | The `Floor` entity, a `WalkArea` with its holes (the table, Barnaby), scaled and sorted by depth                                                                |
| Navigation mesh     | `RoomNav`, a `NavMeshVolume` of the walk areas, baked when the room starts: the hero walks the corners of its paths                                             |
| Actors              | `WalkAgent`s with an `Animator` (idle, walk, reach out, wave), toon shaded: the hero, Barnaby, the troll                                                        |
| Walk-behind masks   | `Mask_<name>` sprites with a `WalkBehind`: the table, the oak, the parapet                                                                                      |
| Hotspots            | `Hotspot_<id>` entities (their rectangle) with a `Stand_<id>` child, listed in the `hotspots` property; the `Prop_<id>` sprite glows under the cursor           |
| Cursor              | The action sprites of the `cursor` layer; the object in hand a `UiToolCursor` of the HUD (`Held_<item>`); the system cursor hidden (`input.set_cursor_visible`) |
| Rules               | `scripts/game/adventure.lua` (cursor, verbs, bag, dialogues, pause) and one visual script per room in `scripts/rooms/`                                          |
| Texts               | `localisation.yml`: English and French, the `menu`, `hud`, `items`, `rooms`, `remarks` and `dialogue` tables                                                    |
| Dialogues, cutscene | `dialogues/barnaby.owldlg`, `dialogues/troll.owldlg`, `sequences/ending.owlseq`                                                                                 |
| Generators          | `tools/samples/point_and_click/`: the backdrops, props, icons and sounds, the scenes and the data                                                               |
| 3D characters       | `meshes/`: the adventurer, the wizard and the goblin of Quaternius (CC0)                                                                                        |

The scenarios `test/unit/scenarios/pnc_*.owltest` play the title, each room, the bag, the gamepad and the whole game to its
ending.
