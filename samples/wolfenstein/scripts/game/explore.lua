-- Fog of war, minimap and "map explored" ratio of a Wolfenstein mini floor (script of the `Minimap` entity).
-- Reused from the raycast level of the Engine Demo (`scripts/raycast/raycast_level.lua`).
--
-- The minimap is a second rendering style over the raycast view: the `overlay` layer of the stack (a `Renderer2D` with
-- `Space: Screen` and `ReferenceHeight: 720`: the screen is 720 units high at any resolution) draws this entity's
-- `Tilemap` (`tilemaps/level_N_minimap.owltilemap`) and two dots this script moves: the player and where it looks. The
-- map is centred on its entity, `scale` units per cell; `map` (Tab, pad Y) switches between the corner minimap and a
-- large map.
--
-- The tilemap has two layers: `walls` (hidden, the whole floor, read once) and `seen` (empty at first). Every frame
-- FOG_RAYS rays are cast over the wall grid across the field of view up to FOG_RANGE cells (a DDA over the cells, no
-- allocation); the cells a ray crosses and the wall it stops at are copied into `seen` with `isometric.set_cell`. A
-- closed door and a secret wall stop a ray and show as walls, a slid secret wall in the cell where it stopped.
--
-- Map explored = floor cells seen / floor cells of the floor (`wolf_explored`, in %; `wolf_seen`, the count). The seen
-- cells are kept as a string of 0 and 1 in `wolf_seen_map` of the gamestate (written at most twice a second) so that a
-- save brings them back; level.lua clears it when a floor starts.
--
-- The fog is set up a few rows per frame: reading and clearing every cell at once overruns the script time quota on
-- slow machines (emulated arm64).

local MAP_SIZE = 32
local WALL_LAYER, SEEN_LAYER = 0, 1
local TILE_WALL, TILE_FLOOR = 0, 1
local FOG_RAYS, FOG_FOV, FOG_RANGE = 41, math.rad(80), 14
local FOG_ROWS_PER_FRAME = 8
local SAVE_PERIOD = 0.5
local HEARING = 12 -- cells beyond which a door is not heard
-- Corner minimap and large map: cell size in units (1/720 of the screen height) and centre (from the lower left corner of the screen).
local SMALL = { scale = 5, x = 96, y = 96 }
local LARGE = { scale = 11, x = 192, y = 192 }
local DOOR_TAGS = { "Door", "GoldDoor", "BlueDoor", "ElevatorDoor" }

local player_id, dot_id, facing_id = 0, 0, 0
local view = SMALL
local solid = {}    -- cell key -> true for a wall of the floor
local blockers = {} -- cell key -> {closed = boolean} for a door or a secret wall (an entity, not a tile)
local doors = {}
local secrets = {}
local seen = {}     -- cell key -> true once seen
local bits = {}     -- the same as a "0" / "1" per cell, joined into `wolf_seen_map`
local floor_total = 0
local floor_seen = 0
local dirty = false
local save_clock = 0
local fog_row = 0

local function cell_of(x, y)
    return math.floor(x + MAP_SIZE / 2), math.floor(MAP_SIZE / 2 - y)
end

local function key_of(c, r)
    return r * MAP_SIZE + c
end

local function closed(key)
    local blocker = blockers[key]
    return blocker ~= nil and blocker.closed
end

-- Copies a cell of the hidden layer into the seen one, a closed door or secret wall counting as a wall.
local function show(c, r)
    local key = key_of(c, r)
    local wall = solid[key] or closed(key)
    isometric.set_cell(entity_id, SEEN_LAYER, c, r, wall and TILE_WALL or TILE_FLOOR, -1)
end

local function mark(key)
    seen[key] = true
    bits[key + 1] = "1"
    if not solid[key] then
        floor_seen = floor_seen + 1
    end
end

local function reveal(c, r)
    if c < 0 or r < 0 or c >= MAP_SIZE or r >= MAP_SIZE then
        return
    end
    local key = key_of(c, r)
    if not seen[key] then
        mark(key)
        dirty = true
        show(c, r)
    end
end

-- One ray of the fog: walks the cells from the player along an angle, revealing each, up to the first wall or
-- FOG_RANGE.
local function cast(px, py, angle)
    local dx, dy = -math.sin(angle), -math.cos(angle) -- the rows grow downward on the map: the world y is flipped
    local c, r = math.floor(px), math.floor(py)
    local step_c, step_r = dx < 0 and -1 or 1, dy < 0 and -1 or 1
    local delta_c = dx == 0 and math.huge or math.abs(1 / dx)
    local delta_r = dy == 0 and math.huge or math.abs(1 / dy)
    local side_c = dx == 0 and math.huge or ((dx < 0 and px - c or c + 1 - px) * delta_c)
    local side_r = dy == 0 and math.huge or ((dy < 0 and py - r or r + 1 - py) * delta_r)
    while true do
        local distance
        if side_c < side_r then
            distance = side_c
            side_c = side_c + delta_c
            c = c + step_c
        else
            distance = side_r
            side_r = side_r + delta_r
            r = r + step_r
        end
        if distance > FOG_RANGE or c < 0 or r < 0 or c >= MAP_SIZE or r >= MAP_SIZE then
            return
        end
        reveal(c, r)
        local key = key_of(c, r)
        if solid[key] or closed(key) then
            return
        end
    end
end

local function percent()
    if floor_total == 0 then
        return 100
    end
    return math.floor(100 * floor_seen / floor_total)
end

local function update_fog(dt)
    local px, py = transform.get_position(player_id)
    local _, _, yaw = transform.get_rotation(player_id)
    local tx, ty = px + MAP_SIZE / 2, MAP_SIZE / 2 - py
    local c, r = math.floor(tx), math.floor(ty)
    for dr = -1, 1 do
        for dc = -1, 1 do
            reveal(c + dc, r + dr)
        end
    end
    for i = 0, FOG_RAYS - 1 do
        cast(tx, ty, yaw + (i / (FOG_RAYS - 1) - 0.5) * FOG_FOV)
    end
    gamestate.set("wolf_seen", floor_seen)
    gamestate.set("wolf_explored", percent())
    save_clock = save_clock + dt
    if dirty and save_clock >= SAVE_PERIOD then
        dirty = false
        save_clock = 0
        gamestate.set("wolf_seen_map", table.concat(bits))
    end
end

-- Reads the walls of the hidden layer and clears the seen one (the tilemap asset is shared with an earlier visit).
local function setup_fog_rows()
    local last = math.min(fog_row + FOG_ROWS_PER_FRAME, MAP_SIZE) - 1
    for r = fog_row, last do
        for c = 0, MAP_SIZE - 1 do
            local key = key_of(c, r)
            solid[key] = isometric.get_cell(entity_id, WALL_LAYER, c, r) == TILE_WALL
            if not solid[key] then
                floor_total = floor_total + 1
            end
            bits[key + 1] = "0"
            if isometric.get_cell(entity_id, SEEN_LAYER, c, r) >= 0 then
                isometric.set_cell(entity_id, SEEN_LAYER, c, r, -1, -1)
            end
        end
    end
    fog_row = last + 1
    return fog_row >= MAP_SIZE
end

-- Brings back the cells of the gamestate (a loaded save) once the walls are known.
local function restore_seen()
    local saved = gamestate.get("wolf_seen_map", "")
    for key = 0, MAP_SIZE * MAP_SIZE - 1 do
        if saved:byte(key + 1) == 49 then
            mark(key)
            show(key % MAP_SIZE, key // MAP_SIZE)
        end
    end
end

local function add_blocker(list, id, state)
    local x, y = transform.get_position(id)
    local c, r = cell_of(x, y)
    local blocker = { closed = state == "idle" }
    blockers[key_of(c, r)] = blocker
    list[#list + 1] = { id = id, state = state, blocker = blocker, c = c, r = r }
end

local function place(scale, x, y)
    view = { scale = scale, x = x, y = y }
    transform.set_position(entity_id, x, y, 0)
    transform.set_scale(entity_id, scale, scale, 1)
end

function on_create()
    player_id = scene.find_entity("Player")
    dot_id = scene.find_entity("MinimapPlayer")
    facing_id = scene.find_entity("MinimapFacing")
    for _, tag in ipairs(DOOR_TAGS) do
        for _, id in ipairs(scene.find_entities(tag)) do
            add_blocker(doors, id, door.get_state(id))
        end
    end
    for _, id in ipairs(scene.find_entities("Secret")) do
        add_blocker(secrets, id, pushwall.get_state(id))
    end
    place(SMALL.scale, SMALL.x, SMALL.y)
end

-- Plays a sound at the position of an entity, softer the farther the player is.
local function play_near(path, id)
    local px, py = transform.get_position(player_id)
    local x, y = transform.get_position(id)
    local near = 1 - math.sqrt((px - x) ^ 2 + (py - y) ^ 2) / HEARING
    if near > 0 then
        sound.set_volume(sound.play(path), near)
    end
end

-- A secret wall that reached the end of its slide stands in another cell: that cell becomes a wall of the map, which
-- stops the fog and shows as a wall (it showed as floor, and the fog saw through the wall into the room behind it).
local function settle(d)
    local x, y = transform.get_position(d.id)
    local c, r = cell_of(x, y)
    local key = key_of(c, r)
    blockers[key] = { closed = true }
    if seen[key] then
        show(c, r)
    end
end

-- A door or a secret wall that changed state: its cell opens or closes on the map; an opening door is heard and counted
-- (`wolf_doors_opened`).
local function follow(list, state_of)
    for _, d in ipairs(list) do
        local state = state_of(d.id)
        if state ~= d.state then
            if state == "opening" and list == doors and player_id ~= 0 then
                play_near("sounds/game/door.wav", d.id)
                gamestate.set("wolf_doors_opened", gamestate.get("wolf_doors_opened", 0) + 1)
            end
            d.blocker.closed = state == "idle"
            if seen[key_of(d.c, d.r)] then
                show(d.c, d.r)
            end
            if state == "final" and list == secrets then
                settle(d)
            end
            d.state = state
        end
    end
end

function on_update(dt)
    follow(doors, door.get_state)
    follow(secrets, pushwall.get_state)
    if fog_row < MAP_SIZE then
        if setup_fog_rows() then
            restore_seen()
        end
        return
    end
    if player_id == 0 then
        return
    end
    update_fog(dt)

    if input.is_action_just_pressed("map") then
        local next_view = view.scale == SMALL.scale and LARGE or SMALL
        place(next_view.scale, next_view.x, next_view.y)
        gamestate.set("wolf_large_map", view.scale == LARGE.scale and 1 or 0)
    end
    local px, py = transform.get_position(player_id)
    -- Clamped to the map so that a player pushed outside never leaves it.
    local cx = math.max(-MAP_SIZE / 2, math.min(MAP_SIZE / 2, px)) * view.scale
    local cy = math.max(-MAP_SIZE / 2, math.min(MAP_SIZE / 2, py)) * view.scale
    transform.set_position(dot_id, view.x + cx, view.y + cy, 0.1)
    local _, _, yaw = transform.get_rotation(player_id)
    local offset = view.scale * 1.4
    transform.set_position(facing_id, view.x + cx - math.sin(yaw) * offset, view.y + cy + math.cos(yaw) * offset, 0.2)
end
