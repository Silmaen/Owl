-- Logic of the raycast level (E1M1): the secrets and the HUD counters.
-- Demonstrates: the `pushwall` table (`pushwall.get_state` polled on every secret wall), `scene.find_entity`, a HUD
--               text refreshed when its value changes, the inventory counters of gamestate (`inv_<item>`).
--
-- The minimap in the lower left corner is a second rendering style composed with the raycast view: the `ui` layer of the
-- stack (a `Renderer2D` with `Space: Screen`, enabled with `raycast_world` in `EnabledRenderers`) draws a `Tilemap`
-- (`tilemaps/minimap.owltilemap`, the walls of the level reduced to two tiles, built by `textures/generate_minimap.py`)
-- and a dot this script moves to the player. The map is centred on its entity, three screen pixels per cell, and the
-- world row is the map row seen from the top: tile = (x + 32, 32 - y).
--
-- Sounds of the level: a door that starts to open and a push-wall that starts to slide make their noise (`door.get_state`
-- and `pushwall.get_state` polled on every one; a state that changes from `idle` is the start), louder when the player is
-- near; the exit elevator and the pick-ups sound in their own scripts.
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

local MAP_SIZE, MAP_SCALE = 64, 3
local player_id, map_id, dot_id = 0, 0, 0

function on_create()
    treasure_id = ui.find("TreasureText")
    player_id = scene.find_entity("MainCamera")
    map_id = scene.find_entity("Minimap")
    dot_id = scene.find_entity("MinimapPlayer")
    for i = 1, DOORS_NORTH + DOORS_WEST do
        local name = i <= DOORS_NORTH and ("Door_N_" .. i) or ("Door_W_" .. (i - DOORS_NORTH))
        local id = scene.find_entity(name)
        if id ~= 0 then
            doors[#doors + 1] = { id = id, state = door.get_state(id) }
        end
    end
    for i, name in ipairs(SECRETS) do
        walls[i] = scene.find_entity(name)
        found[i] = gamestate.get("secret_found_" .. name, 0) > 0
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
            play_near("sounds/raycast_door.wav", d.id)
            gamestate.set("raycast_doors_heard", gamestate.get("raycast_doors_heard", 0) + 1)
        end
        d.state = state
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
    end

    local secrets = 0
    for i, name in ipairs(SECRETS) do
        if walls[i] ~= 0 then
            local state = pushwall.get_state(walls[i])
            if state == "moving" and wall_states[i] ~= "moving" then
                play_near("sounds/raycast_pushwall.wav", walls[i])
            end
            wall_states[i] = state
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
