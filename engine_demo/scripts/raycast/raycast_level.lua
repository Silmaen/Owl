-- Logic of the raycast level (E1M1): the secrets and the HUD counters.
-- Demonstrates: the `pushwall` table (`pushwall.get_state` polled on every secret wall), `scene.find_entity`, a HUD
--               text refreshed when its value changes, the inventory counters of gamestate (`inv_<item>`).
--
-- The minimap in the lower left corner is a second rendering style composed with the raycast view: the `ui` layer of the
-- stack (a `Renderer2D` with `Space: Screen`, enabled with `raycast_world` in `EnabledRenderers`) draws a `Tilemap`
-- (`tilemaps/minimap.owltilemap`, the walls of the level reduced to two tiles, built by `tools/demo/generate_minimap.py`)
-- and a dot this script moves to the player. The map is centred on its entity, three screen pixels per cell, and the
-- world row is the map row seen from the top: tile = (x + 32, 32 - y).
--
-- Sounds of the level: a door that starts to open and a push-wall that starts to slide make their noise (`door.get_state`
-- and `pushwall.get_state` polled on every one; a state that changes from `idle` is the start), louder when the player is
-- near; the exit elevator and the pick-ups sound in their own scripts.
--
-- Fog of war: the tilemap has two layers, `walls` (hidden, the whole level, read once into a Lua table) and `seen` (empty
-- at first). Every frame FOG_RAYS rays are cast over the wall grid across the field of view up to FOG_RANGE cells (a
-- DDA over the cells, no allocation); the floor cells a ray crosses and the wall it stops at are copied into `seen` with
-- `isometric.set_cell`. A closed door and a secret wall that has not moved stop a ray and show as walls. The seen cells
-- are kept as a string of 0 and 1 in `raycast_seen` of the gamestate (written at most twice a second), so a save, a
-- continue or a visit of the hub brings them back; `minimap_seen` counts them for the scenario test.
--
-- The five secret walls slide when the player presses E near them (built-in activation of `RaycastPushWall`); a wall
-- that left the `idle` state counts as a found secret (`raycast_secrets` in gamestate, kept for the whole run).

local SECRETS = { "Secret1", "Secret2", "Secret3", "Secret4", "Secret5" }
local TREASURES = { "cross", "chalice", "chest", "crown" }
local TREASURE_TOTAL = 11

local walls = {}
local found = {}
local treasure_id = 0
local shown = ""

local DOORS_NORTH, DOORS_WEST = 15, 8
local HEARING = 16   -- cells beyond which a door or a wall is not heard
local doors = {}
local wall_states = {}
local wall_blockers = {}

local MAP_SIZE, MAP_SCALE = 64, 3
local player_id, map_id, dot_id, facing_id = 0, 0, 0, 0

local WALL_LAYER, SEEN_LAYER = 0, 1
local TILE_WALL, TILE_FLOOR = 0, 1
local FOG_RAYS, FOG_FOV, FOG_RANGE = 41, math.rad(80), 14
local FACING_OFFSET = 7   -- screen pixels from the dot to the facing marker
local SAVE_PERIOD = 0.5
local solid = {}          -- cell key -> true for a wall of the level
local blockers = {}       -- cell key -> {closed = boolean} for a door or a secret wall (an entity, not a tile)
local seen = {}           -- cell key -> true once the player has seen the cell
local bits = {}           -- the same as a "0" / "1" per cell, joined into `raycast_seen`
local seen_count = 0
local dirty = false
local save_clock = 0

-- Cell of the minimap (column, row from the top) of a world position, and the key of a cell.
local function cell_of(x, y)
    return math.floor(x + MAP_SIZE / 2), math.floor(MAP_SIZE / 2 - y)
end

local function key_of(c, r)
    return r * MAP_SIZE + c
end

-- Copies a cell of the hidden layer into the seen one: a wall, or a floor, a closed door counting as a wall.
local function show(c, r)
    local key = key_of(c, r)
    local blocker = blockers[key]
    local wall = solid[key] or (blocker ~= nil and blocker.closed)
    isometric.set_cell(map_id, SEEN_LAYER, c, r, wall and TILE_WALL or TILE_FLOOR, -1)
end

local function reveal(c, r)
    if c < 0 or r < 0 or c >= MAP_SIZE or r >= MAP_SIZE then
        return
    end
    local key = key_of(c, r)
    if not seen[key] then
        seen[key] = true
        bits[key + 1] = "1"
        seen_count = seen_count + 1
        dirty = true
        show(c, r)
    end
end

local function blocked(c, r)
    local key = key_of(c, r)
    if solid[key] then
        return true
    end
    local blocker = blockers[key]
    return blocker ~= nil and blocker.closed
end

-- One ray of the fog: walks the cells from the player along an angle (the player's yaw plus an offset), revealing each,
-- up to the first wall or FOG_RANGE.
local function cast(px, py, angle)
    local dx, dy = -math.sin(angle), -math.cos(angle)   -- the rows grow downward on the map: the world y is flipped
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
        if blocked(c, r) then
            return
        end
    end
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
    gamestate.set("minimap_seen", seen_count)
    save_clock = save_clock + dt
    if dirty and save_clock >= SAVE_PERIOD then
        dirty = false
        save_clock = 0
        gamestate.set("raycast_seen", table.concat(bits))
    end
end

-- Reads the walls of the hidden layer, clears the seen one (the tilemap asset may be shared with an earlier visit) and
-- restores the cells of the gamestate.
-- The fog is set up a few rows per frame: reading and clearing every cell at once overruns the script time quota on
-- slow machines (emulated arm64).
local FOG_ROWS_PER_FRAME = 8
local fog_row = 0

local function setup_fog_rows()
    local last = math.min(fog_row + FOG_ROWS_PER_FRAME, MAP_SIZE) - 1
    for r = fog_row, last do
        for c = 0, MAP_SIZE - 1 do
            local key = key_of(c, r)
            solid[key] = isometric.get_cell(map_id, WALL_LAYER, c, r) == TILE_WALL
            bits[key + 1] = "0"
            if isometric.get_cell(map_id, SEEN_LAYER, c, r) >= 0 then
                isometric.set_cell(map_id, SEEN_LAYER, c, r, -1, -1)
            end
        end
    end
    fog_row = last + 1
    return fog_row >= MAP_SIZE
end

local function restore_seen()
    local saved = gamestate.get("raycast_seen", "")
    for key = 0, MAP_SIZE * MAP_SIZE - 1 do
        if saved:byte(key + 1) == 49 then
            seen[key] = true
            bits[key + 1] = "1"
            seen_count = seen_count + 1
        end
    end
end

-- Draws the seen cells of the gamestate once the doors and secret walls are known (they change a cell's tile).
local function draw_restored()
    for key in pairs(seen) do
        show(key % MAP_SIZE, key // MAP_SIZE)
    end
end

function on_create()
    treasure_id = ui.find("TreasureText")
    player_id = scene.find_entity("MainCamera")
    map_id = scene.find_entity("Minimap")
    dot_id = scene.find_entity("MinimapPlayer")
    facing_id = scene.find_entity("MinimapFacing")
    for i = 1, DOORS_NORTH + DOORS_WEST do
        local name = i <= DOORS_NORTH and ("Door_N_" .. i) or ("Door_W_" .. (i - DOORS_NORTH))
        local id = scene.find_entity(name)
        if id ~= 0 then
            local x, y = transform.get_position(id)
            local c, r = cell_of(x, y)
            local blocker = { closed = door.get_state(id) == "idle" }
            blockers[key_of(c, r)] = blocker
            doors[#doors + 1] = { id = id, state = door.get_state(id), blocker = blocker, c = c, r = r }
        end
    end
    for i, name in ipairs(SECRETS) do
        walls[i] = scene.find_entity(name)
        found[i] = gamestate.get("secret_found_" .. name, 0) > 0
        if walls[i] ~= 0 then
            local x, y = transform.get_position(walls[i])
            local c, r = cell_of(x, y)
            local blocker = { closed = pushwall.get_state(walls[i]) == "idle" }
            blockers[key_of(c, r)] = blocker
            wall_blockers[i] = { blocker = blocker, c = c, r = r }
        end
    end
end

-- Plays a sound of the level at the position of an entity, softer the farther the player is.
local function play_near(path, id)
    if player_id == 0 then
        return
    end
    local px, py = transform.get_position(player_id)
    local x, y = transform.get_position(id)
    local near = 1 - math.sqrt((px - x) ^ 2 + (py - y) ^ 2) / HEARING
    if near > 0 then
        sound.set_volume(sound.play(path), near)
    end
end

function on_update(dt)
    for _, d in ipairs(doors) do
        local state = door.get_state(d.id)
        if state == "opening" and d.state ~= "opening" then
            play_near("sounds/raycast/door.wav", d.id)
            gamestate.set("raycast_doors_heard", gamestate.get("raycast_doors_heard", 0) + 1)
        end
        if state ~= d.state then
            d.blocker.closed = state == "idle"
            if seen[key_of(d.c, d.r)] then
                show(d.c, d.r)
            end
        end
        d.state = state
    end
    if map_id ~= 0 and fog_row < MAP_SIZE then
        if setup_fog_rows() then
            restore_seen()
            draw_restored()
        end
    elseif player_id ~= 0 and map_id ~= 0 then
        update_fog(dt)
    end
    if player_id ~= 0 and dot_id ~= 0 then
        local px, py = transform.get_position(player_id)
        local mx, my = transform.get_position(map_id)
        -- Clamped to the map so that a player pushed outside never leaves the minimap.
        local cx = math.max(-MAP_SIZE / 2, math.min(MAP_SIZE / 2, px))
        local cy = math.max(-MAP_SIZE / 2, math.min(MAP_SIZE / 2, py))
        transform.set_position(dot_id, mx + cx * MAP_SCALE, my + cy * MAP_SCALE, 0.1)
        gamestate.set("minimap_x", mx + cx * MAP_SCALE)
        gamestate.set("minimap_y", my + cy * MAP_SCALE)
        if facing_id ~= 0 then
            local _, _, yaw = transform.get_rotation(player_id)
            transform.set_position(facing_id, mx + cx * MAP_SCALE - math.sin(yaw) * FACING_OFFSET,
                    my + cy * MAP_SCALE + math.cos(yaw) * FACING_OFFSET, 0.2)
        end
    end

    local secrets = 0
    for i, name in ipairs(SECRETS) do
        if walls[i] ~= 0 then
            local state = pushwall.get_state(walls[i])
            if state == "moving" and wall_states[i] ~= "moving" then
                play_near("sounds/raycast/pushwall.wav", walls[i])
            end
            wall_states[i] = state
            local info = wall_blockers[i]
            if info ~= nil and info.blocker.closed ~= (state == "idle") then
                info.blocker.closed = state == "idle"
                if seen[key_of(info.c, info.r)] then
                    show(info.c, info.r)
                end
            end
        end
        if not found[i] and walls[i] ~= 0 and pushwall.get_state(walls[i]) ~= "idle" then
            found[i] = true
            gamestate.set("secret_found_" .. name, 1)
            gamestate.set("inv_toast", "Secret found!")
        end
        secrets = secrets + (found[i] and 1 or 0)
    end
    gamestate.set("raycast_secrets", secrets)

    local treasure = 0
    for _, id in ipairs(TREASURES) do
        treasure = treasure + gamestate.get("inv_" .. id, 0)
    end
    local text = "Treasure: " .. treasure .. " / " .. TREASURE_TOTAL .. "    Secrets: " .. secrets .. " / " .. #SECRETS
    if text ~= shown then
        shown = text
        ui.set_text(treasure_id, text)
    end
end
