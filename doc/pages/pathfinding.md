# Pathfinding {#page-pathfinding}

[TOC]

**Level:** Confirmed. **Read first:** [Scene and Components](scene.md).

Three ways to find a path. Vehicles of a Transport Tycoon-like game follow a graph of tiles (roads, tracks). A `Tilemap`
carries `navigation` options that turn one of its layers into a graph, the roads coming from the tileset; the engine searches shortest paths over it, spreads
the long queries over frames, and updates the graph chunk by chunk when a tile changes. The code is `scene::nav`
(`source/owl/public/scene/nav/`). Point-and-click and top-down games walk freely on 2D floors instead: see
[Walkable areas](#pathfinding-walk-areas). 3D games bake the floors of a scene into a navigation mesh walked by
agents: see [Navigation meshes](#pathfinding-navmesh).

```mermaid
flowchart LR
    T[TilemapAsset<br/>32x32 chunks, generations] -->|sync: only the chunks whose generation changed| M[NavMap<br/>costs, moves, slopes]
    P[NavProfile<br/>options and exceptions of the Tilemap component] --> M
    K[Tileset<br/>connections of each tile] --> M
    M --> F[Pathfinder<br/>flat A* or hierarchical]
    F --> Q[PathQueue<br/>budget of nodes per frame]
    Q -->|owl.navigation, Late phase| S[Scene::getNavigation]
    S --> L[Lua path.*]
    M --> E[Owl Nest overlay]
```

## The profile

The `navigation` key of the `Tilemap` component (inspector section **Navigation**) holds a `NavProfile`:

| Field        | Meaning                                                                                                                                                                           |
|--------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `mode`       | How the graph is read: `connections` (the tileset says which tiles are roads), `cells` (any non-empty cell), `rules` (explicit rules; the mode of a file without `mode`).         |
| `layer`      | Layer of the tilemap the graph is read from.                                                                                                                                      |
| `source`     | `overlay` (roads, tracks) or `terrain` (walking on the ground tile).                                                                                                              |
| `maxRise`    | Highest rise, in height steps, a move may cross over one cell (0 to 4).                                                                                                           |
| `climbCost`  | Extra cost, in cells, of climbing one height step.                                                                                                                                |
| `allowSteep` | Whether a cell on a steep slope can be crossed.                                                                                                                                   |
| `overrides`  | Exceptions of single cells: `cell: [x, y]`, `noExit` / `noEnter` (sides closed to leaving or entering) and `cost`.                                                                |
| `rules`      | `mode: rules` only: one rule per tile (`tile`, `cost`, `exits`, `enters`).                                                                                                        |

```yaml
Tilemap:
  tilemapPath: tilemaps/iso_town.owltilemap
  navigation:
    mode: connections
    layer: 1
    source: overlay
    maxRise: 1
    climbCost: 0.5
    allowSteep: false
    overrides:
      - {cell: [12, 7], noEnter: [north], cost: 2}   # exit only by the north side, twice as slow
```

### Roads come from the tileset

In `connections` mode a cell is a node when its tile has `connections` in the tileset (`.owltileset`, per tile):

```yaml
tiles:
  - {index: 3, name: road, connections: [east, west]}      # a straight road along x
  - {index: 13, name: road end n, connections: [north]}    # a dead end open to the north
  - {index: 12, name: road alone, connections: []}         # a road tile that joins nothing
```

A tile without `connections` is not part of the graph. The sides are the map directions (`east` is +x, `south` is +y,
`west` is -x, `north` is -y); in the isometric view they point to the lower right, lower left, upper left and upper
right of the screen. The `tools/iso_atlas` generator writes them for the 16 road shapes and the Tileset Editor edits
them (section **Navigation** of the tile properties). A move to a neighbour exists when both cells join each other
(the first one is connected towards it, the second one back), and no slope is in the way: a steep cell is blocked
unless `allowSteep`, and a cell whose rise along the move is above `maxRise` blocks it. The cost of a move is the sum
of the costs of both cells (a base move is 1) plus `climbCost` for the height climbed. Changing the connections of the
tileset rebuilds the graph.

### Exceptions

`overrides` only restrict what the tiles allow, they never open a side a tile keeps closed. Per cell side the state is
**two-way**, **exit only** (movers leave the cell by this side, none comes in), **enter only** or **closed**; the
neighbour cell is updated to agree, so a one-way road is one click. A cell can also carry its own `cost` (1 is a normal
road, 3 three times slower; absent keeps the cost of its tile). They are edited with the Navigation tool of the
isometric view, through the `nav.set_edge`, `nav.set_cell_cost` and `nav.clear_cell` commands (undoable, journaled).

### Explicit rules

A profile with `rules` and no `mode` loads as `mode: rules` and builds the graph from the rules: a move exists when both
cells have a rule, the first one lets the mover leave in that direction (`exits`) and the second one lets it enter
heading that way (`enters`). The inspector offers **Use the tileset connections instead** to drop them.

## Searches

Both searches are exact and return paths of the same cost (a unit test compares them on random maps with one-way
tiles, costs and slopes):

- **Flat**: A* over the cells with the Manhattan distance at the lowest edge cost as heuristic.
- **Hierarchical**: A* over the border cells of the 32 x 32 chunks. Inside a chunk, the cost between two border cells
  is the best path that stays in it, computed once per border cell and kept in the map until that chunk changes. The
  path found is then refined chunk by chunk. It pays when the same network is queried again and again.
- **Auto** (default): flat, except hierarchical for cells at least 3 chunks apart on a sparse graph (at most one cell
  in five is a node).

- **No reverse** (`PathOptions`): for a vehicle that keeps its heading. A move back to the cell just left is forbidden
  unless the cell is a dead end (a single exit), and the first move cannot reverse the `heading` the vehicle has at the
  start. The state of the search is the cell and the direction it was entered by (5 entry states per cell, none when
  there is no heading), so the path is the cheapest one under the constraint, and a goal that only a U-turn reaches is
  `NotFound`. It is always a flat search: `Auto` and `Hierarchical` fall back to flat when `noReverse` is set. Sliced
  and resumable like the others, `PathQueue::request` keeps the options of each request.

`Pathfinder::begin` / `step(budget)` run a search in slices, `Pathfinder::find` runs it whole. The working arrays are
kept, so a search allocates nothing once they reached the size of the map.

## Changes of the map

`NavMap::sync` compares the generation of every chunk of the layer (and of the corner heights around it) with the one
it saw and rebuilds only the chunks that changed: a bulldozed road costs one chunk, about 5 µs. `getVersion` grows with
every change, a request being searched when it changes starts over, and `isPathValid` tells whether a path found
earlier still exists.

## Requests over frames

`PathQueue::request` queues a query, the `owl.navigation` system (`Late` phase) spends `NavWorld::getBudget` node
expansions per frame (4000 by default, about 0.2 ms) on the oldest ones, `getStatus` and `getResult` poll. Slots and
buffers of released requests are reused.

## Lua

```lua
local request = path.find(map, from_x, from_y, to_x, to_y)   -- 0 when the entity has no graph
-- next frames
if path.poll(map, request) == "found" then
    local cells, cost = path.get(map, request)               -- {x1, y1, x2, y2, ...}
    path.release(map, request)
end
if path.get_version(map) ~= version then ... end              -- a road was built or bulldozed
-- a vehicle heading East that never turns back, except at a dead end (options may replace the mode)
local request = path.find(map, x, y, to_x, to_y, { no_reverse = true, heading_x = 1, heading_y = 0 })
```

Every function is in the [Lua API reference](lua-api-isometric.md#path). The `iso_town` vehicles of the Engine Demo
(`scripts/iso/iso_town_traffic.lua`) drive to a random road cell (seeded, another one on arrival) that way, never turning back except at a dead end where
they loop round the middle of the cell, look for another way when a road of their route is bulldozed (scenario
`iso_town`), plan their route again at the next cell when a road was built (a vehicle left standing on a bulldozed cell
asks from the road cell it came from, without a heading), and wait when no route exists.

## Seeing the graph and the routes

`path.set_debug(true)` makes the scene draw the graph of every tilemap with a navigation graph (two-way moves in green, one-way
moves in orange with an arrow) and the routes scripts hand to `path.show_route(map, id, cells, r, g, b)` (one line per
id, a diamond on the last cell, `path.clear_routes(map)` removes them all). The overlay is drawn by the scene over the
isometric view, in the game and in the Play view of Owl Nest. In the Engine Demo **F3** (the `nav_debug` action) toggles
it and `iso_town_traffic.lua` gives each vehicle its own colour and gives the overlay its route at every plan.

## Owl Nest

The **Navigation** section of the `Tilemap` inspector holds the global options only, in plain words: the layer the
graph is read from (by its name), the tile read, the read mode (tileset connections or any non-empty cell), the maximum
slope rise, the climb cost, allow steep slopes and a **Show graph** button (undoable like any component edit). Roads are
described by the tileset: turn on **Road** and light the **Joined Sides** arrows of the sides a tile joins in the **Navigation**
section of its properties in the Tileset Editor (the arrows are oriented as on the screen of the isometric view).

In the isometric viewport, **Show > Navigation graph and routes** (or **Show graph**) draws the graph over the map: two-way
moves in green, one-way moves in orange with an arrow, moves cut or made one-way by an exception in magenta, a closed side
as a red cross and a cell with its own cost ringed in yellow. In Play, the eye button at the bottom-right of the view toggles
the same overlay, with the routes the running scripts give it. With the overlay on, **Shift + click** picks the start then
the goal of a test path drawn in yellow (a third click starts over, **Shift + right click** clears it).

The **Navigation tool** (signpost button of the view overlay) edits the exceptions directly on the map: hover a road cell
to see an arrow on each of its sides, the one nearest to the cursor is highlighted; a **left click** cycles it two-way,
exit only, enter only, closed; a **right click** opens the cell popup (its own cost, **Clear exceptions**). Every click is
undone with Ctrl+Z (clicks within a second merge into one step).

## Measures

`owl_bench --filter=path/` on a 1024 x 1024 map (`test/bench/cases/PathBench.cpp`, reference i9-13950HX, one core):

| Case                           | Flat    | Hierarchical (warm) | Hierarchical (cold) |
|--------------------------------|---------|---------------------|---------------------|
| Open ground, corner to corner  | 0.22 ms | 2.7 ms              | 5.2 ms              |
| Streets every 8 cells          | 0.14 ms | 0.46 ms             | 1.1 ms              |
| Comb (one avenue, 128 streets) | 3.7 ms  | 0.83 ms             | 10 ms               |
| Walls with one gap each        | 145 ms  | 171 ms              | 1.6 s               |

Building the graph of the whole map takes 7 ms, a one-cell edit 5 µs, an edit followed by a hierarchical query
0.43 ms. Hierarchical wins only on repeated queries over a sparse, maze-like network: that is why `Auto` rarely picks
it, and why a long open-ground query (the walls case) stays a job for several frames.

## Walkable areas {#pathfinding-walk-areas}

A point-and-click room or a top-down plaza is not a graph of tiles: the actor walks anywhere on a floor, around the
well and the table. A `WalkArea` component holds that floor as polygons with holes on the plane (x, y) of its entity
(`scene::nav::WalkMesh`, `scene/component/WalkArea.h`); `WalkAgent` entities walk on it and `WalkBehind` masks hide
them behind parts of the backdrop.

```mermaid
flowchart LR
    P[WalkArea polygons<br/>outlines and holes] -->|holes bridged, ear clipping, on change| M[WalkMesh<br/>triangles and neighbours]
    M -->|A* over the triangles,<br/>then the funnel| W[path: corners only]
    W --> A[WalkAgent<br/>owl.walk_agents, PrePhysics]
    A -->|scale and Z by height| T[Transform]
    B[WalkBehind baseline] -->|Z by height| T
    L[Lua walk.*] --> W
```

### The floor

Each polygon is an outline and its holes, in either winding; polygons whose edges meet vertex to vertex join into one
floor, two that do not touch are two floors no path crosses. The triangles are cut again the first time the area is
read after an edit. A point off the floor (a click on a wall) is moved to the nearest point of its border, so
`walk.go_to` always walks as close as it can.

### Paths

A* runs over the triangles, from the middle of an edge to the middle of the next, then the corridor of triangles is
pulled tight (funnel algorithm): the path turns only at corners of the outline and of the holes, and is a straight line
when nothing is in the way. The search runs at once when asked (`walk.go_to`, `walk.find_path`): a room has tens of
triangles, not the thousands of cells of a tile graph.

### Depth: scale, sort and walk-behind masks

Walking up the plane moves an actor away from the viewer. With `depthScale` the area scales the agents from
`nearScale` at `nearY` to `farScale` at `farY` (the scale the entity has at the start of Play is its scale at 1), and
with `scaleSpeed` they walk slower far away. With `depthSort` it sets their Z from their height: `depthPerUnit` per unit
walked down from `nearY`. A `WalkBehind` on a sprite cut from the backdrop (a pillar, a hedge) takes the Z of its
`baseline`: an actor whose feet are above the line is drawn behind it, one below the line in front. Sprites of a 2D
layer are drawn in the order of their Z; meshes of a mesh layer are hidden by the masks of a depth-tested 2D layer drawn
after them (`DepthTest: true`, [Renderer stack](renderer.md#renderer-stack)), so 3D actors walk behind a 2D backdrop
the same way.

### Agents

`walk.go_to(agent, x, y)` finds the path and the `owl.walk_agents` system walks it at `speed`: it places the entity
(its origin is the feet of the actor), turns it (`facing`: `MirrorX` mirrors a sprite walking left, `TurnY` turns a mesh
around Y, facing the viewer when walking down) and gives the speed to the `speedParameter` float of its `Animator` (or
of its first child holding one), so an idle / walk state machine follows the steps. `walk.stop`, `walk.is_moving` and
`walk.get_target` drive and poll it, `walk.set_area` moves it to another floor.

```lua
local hero = scene.find_entity("Hero")
local floor = scene.find_entity("Floor")

function on_click(x, y)                         -- a world point of the room
    if walk.is_inside(floor, x, y) then
        walk.go_to(hero, x, y)
    end
end

function on_update()
    if arrived and not walk.is_moving(hero) then
        -- the hero stands at the hotspot: look, use, talk
    end
end

local points = walk.find_path(floor, 1, 1, 9, 4)  -- {x1, y1, x2, y2, ...} or nil
local scale = walk.get_scale_at(floor, 4, 6)      -- the size of an actor standing there
```

Every function is in the [Lua API reference](lua-api-isometric.md#walk). The `iso_town` plaza of the Engine Demo is
walked by pedestrians this way (`scripts/iso/iso_town_walkers.owlvs`), and the point-and-click sample game The Moon
Lantern moves its actors with it over 3D-on-2D rooms (`samples/point_and_click/`).

### Seeing and editing the floors

The navigation overlay (`path.set_debug(true)`, **Show > Navigation graph and routes** in Owl Nest) draws every walk
area, its borders and triangles, and the path left to each walking agent in yellow. In Owl Nest the selected walk area
is edited in the viewport: vertices moved, inserted and deleted, holes and floors added, each gesture a `walk.*`
command ([Walk areas in the viewport](editor_viewport.md#viewport-walk-areas)).

## Navigation meshes {#pathfinding-navmesh}

A 3D level has no tiles and no single plane: floors at several heights, stairs, walls, props. A `NavMeshVolume` cuts
the static geometry inside its box into a mesh of triangles; `NavAgent` entities walk on it around the walls and around
each other, `NavObstacle` entities are steered around, `NavLink` entities join floors the mesh does not (a jump, a
ladder). The code is `scene::nav` (`NavMesh.h`, `NavBake.h`, `Avoidance.h`); the components are in
[Scene and Components](scene.md#nav-components).

```mermaid
flowchart LR
    G[Static colliders and meshes<br/>inside the box] -->|bake: voxels, filters, erosion,<br/>regions, contours, triangles| V[NavMeshVolume<br/>triangles saved in the scene]
    V -->|read: links attached| M[NavMesh]
    K[NavLink] --> M
    M -->|A* over the triangles and links,<br/>then the funnel| P[path: corners and links]
    P --> A[NavAgent<br/>owl.nav_agents, PrePhysics, fixed tick]
    O[NavObstacle] --> R[ORCA avoidance]
    A -->|preferred velocity| R
    R -->|velocity kept clear| A
    A -->|position, turn around Y| T[Transform]
    A -->|speed| N[Animator float]
    L[Lua nav.*] --> A
    L --> P
```

### The bake

A bake is done in the editor (or by `nav.bake` in Lua) and its triangles are saved in the scene, so every target loads
the same mesh. The steps:

1. The static geometry inside the box is rasterised into voxels (`cellSize` wide, `cellHeight` high).
2. Floors that are too steep (`maxSlope`), steps higher than `maxClimb`, ledges and spans without `agentHeight` of
   free space above are dropped.
3. The floor is eroded by `agentRadius`, so a path along a wall keeps the actor clear of it.
4. Islands smaller than `minRegionArea` (a table top, a crate lid) are dropped.
5. The floor is split into monotone regions; stacked floors stay apart.
6. The outline of each region is traced and simplified within `maxEdgeError`, then cut into triangles.

The bake reads the colliders of the static bodies and the meshes of the entities without a body (`sources` picks
`CollidersAndMeshes`, `Colliders` or `Meshes`). It skips what moves (agents, obstacles, spline followers, bodies that
are not static, and their children) and hidden entities. The box is centred on the entity, aligned on the world axes.

| Setting         | Default | Meaning                                                      |
|-----------------|---------|--------------------------------------------------------------|
| `cellSize`      | 0.25    | Width of a voxel: the precision of the borders.              |
| `cellHeight`    | 0.1     | Height of a voxel: the precision of the floor heights.       |
| `agentHeight`   | 1.8     | Free height an agent needs above the floor.                  |
| `agentRadius`   | 0.4     | Radius of the agents: the floor is shrunk by it along walls. |
| `maxClimb`      | 0.4     | Highest step an agent climbs.                                |
| `maxSlope`      | 45      | Steepest walkable slope, in degrees.                         |
| `minRegionArea` | 2       | Smallest island of floor kept, in square units.              |
| `maxEdgeError`  | 0.3     | Largest distance between a simplified border and its voxels. |

### Paths and links

A* runs over the triangles and the links, from the middle of an edge to the middle of the next, then the corridor is
pulled tight (funnel algorithm): the path turns only at corners. A start or goal off the mesh is moved to the nearest
point of the mesh within 2 units, else there is no path.

A `NavLink` goes from the entity origin to `end` (relative, world axes), one way or both (`bidirectional`), with a
`costScale` of at least 1 on its length (walking costs 1). Links are attached to the mesh when it is read, to every
volume whose mesh is within reach of both ends: moving or adding a link needs no new bake.

### Agents and avoidance

`nav.go_to` gives a `NavAgent` a goal; the `owl.nav_agents` system (`PrePhysics`, fixed tick only) finds the path and
steers along it at up to `speed`, with at most `acceleration`. The agent slides along the walls, crosses a link in a
straight line, keeps the entity on the surface of the mesh and, with `turn`, turns it around Y (its front is +Z). Its
speed goes to the `speedParameter` float of its `Animator` (or of its first child holding one).

With `avoidance`, the agent steers clear of the other agents and of the obstacles with ORCA (reciprocal velocity
obstacles): two agents each take half of the avoidance, an obstacle takes none, so the agent takes all of it. An agent
blocked by another retries bearing right. Everything runs in a fixed order (UUID order), so the simulation stays
deterministic ([Simulation and networking](design/simulation-networking.md)).

### Obstacles

A `NavObstacle` is a vertical cylinder (`radius`, `height`) at the entity origin: a crate pushed about, a cart, a
monster without a path. Its velocity is read from its moves between two ticks. It is avoided, never carved out of the
mesh: a wall that stays belongs to the bake.

### Lua

```lua
local regular = scene.find_entity("Regular_1")
local volume = scene.find_entity("TavernNav")

nav.go_to(regular, 2.1, 0, 2.9)                       -- the path is found at the next tick
if not nav.is_moving(regular) then
    -- arrived: wait, then pick another spot
end

local points = nav.find_path(volume, 0, 0, 0, 3, 0, 3)  -- {x1, y1, z1, x2, y2, z2, ...} or nil
```

Every function is in the [Lua API reference](lua-api-navigation.md#nav). The two regulars of the tavern in the Engine
Demo stroll that way (`scripts/tavern/tavern_regular.owlvs`), round the tables, the server, the barrels and the player,
and reach the second table by the `TableHop` link (scenario `tavern_regulars`).

### Seeing and editing in Owl Nest

The **Nav Mesh Volume** inspector sets the box (**Size**, **Sources**) and the bake settings under **Agents** and
**Voxels**, shows the triangle count and area of the mesh (or *Not baked*) and has a **Bake** button. The same bake is
the `nav.bake` command. **Nav Agent**, **Nav Obstacle** and **Nav Link** have their own
inspectors, every field with a tooltip.

In the viewport, every volume is a blue box (brighter when selected, and its border is then drawn in cyan), a link is a
magenta line ending in a ring, an obstacle an orange cylinder box; all are pickable by a click. **Show >
Navigation graph and routes** draws every baked mesh (borders and triangles in cyan), the links, the obstacles and the
path left to each agent in yellow.
