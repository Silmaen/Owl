-- World map player controller (top-down view).
-- Demonstrates: top-down 4-directional movement using physics.set_velocity
-- with gravity scale = 0 to opt out of the global Box2D gravity.
--
-- The world physic uses a downward gravity (-9.81 on Y). For top-down play we
-- set this body's gravity scale to 0 once at create time, so set_velocity is
-- the sole source of motion (no per-frame cancellation hack).

properties = {
    { name = "speed", type = "float", default = 5.0 },
    { name = "frame_idle", type = "int", default = 148, min = 0 },
    { name = "frame_walk", type = "int", default = 168, min = 0 },
    { name = "walk_frames", type = "int", default = 4, min = 1 },
    { name = "step_time", type = "float", default = 0.11, min = 0.01 },
    { name = "wade_factor", type = "float", default = 0.4, min = 0, max = 1 },
    { name = "wade_step_time", type = "float", default = 0.2, min = 0.01 },
}

local transitioning = false  -- gate input while the engine is in scene-load orchestration
local teleporter_id = 0
local teleporter_visible = false
local health_bar_id = 0
local camera_id = 0
local sprite_id = 0   -- child sprite (Tiny Dungeon character): mirrored to face where the player walks
local sprite_scale = 1.0
local levels_id = 0   -- HUD counter "Levels completed: n / 4"
local shown_levels = -1

-- Levels of the hub (`level_<name>_done` in gamestate, set by the victory of each level). The victory portal wakes up
-- when every one is done.
local LEVELS = { "platformer", "raycast", "iso", "voxel" }

-- Hero sprite: the idle frame of the Tiny Dungeon hero and the four walk frames drawn below the characters of
-- town_atlas.png (the pack has none), played at step_time each while the player moves.
local walk_clock = 0
local shown_frame = -1
local shown_step = -1
local STEP_VARIANTS = 4
-- Spelt out so the game export finds and packs every footstep.
local STEP_SOUNDS = { "sounds/hub/step_1.wav", "sounds/hub/step_2.wav", "sounds/hub/step_3.wav", "sounds/hub/step_4.wav" }
local step_sound = 0
-- In the river the hero wades: much slower, and a splash instead of a footstep. The water tiles of the ground layer
-- (four animation phases, hub_water.lua) tell where it is; the bridges are not water.
local WADE_SOUNDS = { "sounds/hub/wade_1.wav", "sounds/hub/wade_2.wav", "sounds/hub/wade_3.wav", "sounds/hub/wade_4.wav" }
local WATER_TILES = { [132] = true, [133] = true, [137] = true, [138] = true }
local map_id = 0
local wading = false

-- The camera follows the hero but stays inside the map: the view is as many cells high as the camera's orthographic
-- size (15, a pixel-exact 3x at 720p), so k_view_half_height = 7.5, and a 16:9 window shows k_view_half_width cells on each side.
local k_map_width, k_map_height = 64, 40
local k_view_half_height = 7.5
local k_view_half_width = k_view_half_height * 16 / 9

function on_create()
    log.info("WorldPlayer created with speed=" .. speed)
    -- Top-down: opt out of the world's downward gravity for this body.
    physics.set_gravity_scale(entity_id, 0.0)
    -- Position restore priority: continue snapshot (ESC pause) wins over the
    -- normal "world_return" set by the door when coming back from a platformer.
    local cx = gamestate.get("continue_x", 0)
    local cy = gamestate.get("continue_y", 0)
    if cx ~= 0 or cy ~= 0 then
        physics.set_transform(entity_id, cx, cy, 0)
        log.info("Resumed world map at " .. cx .. ", " .. cy)
        gamestate.remove("continue_x")
        gamestate.remove("continue_y")
        gamestate.remove("continue_z")
        gamestate.remove("continue_scene")
    else
        local rx = gamestate.get("world_return_x", 0)
        local ry = gamestate.get("world_return_y", 0)
        if rx ~= 0 or ry ~= 0 then
            physics.set_transform(entity_id, rx, ry, 0)
            log.info("Returned to world after platformer at " .. rx .. ", " .. ry)
            gamestate.remove("world_return_x")
            gamestate.remove("world_return_y")
        end
    end

    teleporter_id = scene.find_entity("Teleporter")
    health_bar_id = scene.find_entity("HealthBar")
    levels_id = ui.find("LevelsText")
    camera_id = scene.find_entity("MainCamera")
    map_id = scene.find_entity("WorldTilemap")
    gamestate.set("hub_wading", false)
    sprite_id = scene.find_entity("PlayerSprite")
    if sprite_id ~= 0 then sprite_scale = transform.get_scale(sprite_id) end
    -- Initialise health for a fresh run.
    if gamestate.get("health", 0) <= 0 then
        gamestate.set("health", 1.0)
    end
    if health_bar_id ~= 0 then
        ui.set_progress(health_bar_id, gamestate.get("health", 1.0))
    end
    ui.transition_fade_in(0.4)
end

function on_update(dt)
    -- Gate input while the engine drives a scene-to-scene transition.
    if transitioning or ui.is_transition_active() then
        return
    end

    -- Still while the information board is open: the move keys scroll it (info_board.lua).
    if gamestate.get("board_open", 0) == 1 then
        physics.set_velocity(entity_id, 0, 0)
        return
    end

    -- Movement: the built-in `move` vector (WASD or left stick; y points down), slowed down in the river.
    local px, py, _ = transform.get_position(entity_id)
    local in_water = map_id ~= 0 and WATER_TILES[isometric.get_cell(map_id, 0, math.floor(px), math.floor(-py))] == true
    if in_water ~= wading then
        wading = in_water
        gamestate.set("hub_wading", wading)
    end
    local pace = wading and speed * wade_factor or speed
    local dirx, diry = input.get_action_vector("move")
    local mvx, mvy = dirx * pace, -diry * pace

    physics.set_velocity(entity_id, mvx, mvy)
    if sprite_id ~= 0 then
        if mvx ~= 0 then
            transform.set_scale(sprite_id, mvx < 0 and -sprite_scale or sprite_scale, sprite_scale, 1)
        end
        -- Walk cycle while moving, the idle frame when still.
        local frame = frame_idle
        if mvx ~= 0 or mvy ~= 0 then
            walk_clock = walk_clock + dt
            frame = frame_walk + math.floor(walk_clock / (wading and wade_step_time or step_time)) % walk_frames
        else
            walk_clock = 0
        end
        if frame ~= shown_frame then
            shown_frame = frame
            isometric.set_sprite_frame(sprite_id, frame)
        end
        -- A footstep each time a foot lands: the contact frames of the cycle (the first and the third).
        local step = (frame - frame_walk) % walk_frames
        if frame >= frame_walk and step ~= shown_step and step % 2 == 0 then
            -- Four variants, never the same twice in a row, each a little louder or softer (no pitch control in Lua).
            step_sound = (step_sound + math.random(STEP_VARIANTS - 1) - 1) % STEP_VARIANTS + 1
            local sounds = wading and WADE_SOUNDS or STEP_SOUNDS
            sound.set_volume(sound.play(sounds[step_sound]), 0.55 + math.random() * 0.4)
        end
        shown_step = frame >= frame_walk and step or -1
    end

    -- Camera centred on the player, held inside the map (the forest colliders keep the player inside it too).
    if camera_id ~= 0 then
        local px, py, _ = transform.get_position(entity_id)
        local _, _, cz = transform.get_position(camera_id)
        local cx = math.max(k_view_half_width, math.min(k_map_width - k_view_half_width, px))
        local cy = math.max(-(k_map_height - k_view_half_height), math.min(-k_view_half_height, py))
        transform.set_position(camera_id, cx, cy, cz)
    end

    -- Push current health to the HUD bar.
    if health_bar_id ~= 0 then
        ui.set_progress(health_bar_id, gamestate.get("health", 1.0))
    end

    -- Count the levels done, show it, and reveal the victory portal once every one is.
    local done = 0
    for _, name in ipairs(LEVELS) do
        done = done + (gamestate.get("level_" .. name .. "_done", 0) > 0 and 1 or 0)
    end
    if done ~= shown_levels then
        shown_levels = done
        gamestate.set("levels_done", done)
        ui.set_text(levels_id, "Levels completed: " .. done .. " / " .. #LEVELS)
        local all_done = done >= #LEVELS
        if teleporter_id ~= 0 then
            ui.set_visible(teleporter_id, all_done)
        end
        if all_done and not teleporter_visible then
            log.info("All levels completed - the victory portal has appeared!")
            sound.play("sounds/hub/portal_wake.wav")
        end
        teleporter_visible = all_done
    end

    -- Escape returns to the main menu, preserving the world position so the
    -- "Continuer" button on the menu can resume exactly where we left off.
    -- (the Escape that closed the board is not a pause)
    local closing = gamestate.get("board_frame", 0) - gamestate.get("board_closed_frame", -100) <= 1
    if input.is_action_just_pressed("pause") and not closing then
        local px, py, pz = transform.get_position(entity_id)
        gamestate.set("continue_scene", "scenes/world_map.owl")
        gamestate.set("continue_x", px)
        gamestate.set("continue_y", py)
        gamestate.set("continue_z", pz)
        gamestate.set("has_continue", true)
        log.info("ESC: pausing — saved world position for Continue.")
        transitioning = true
        scene.transition_to("scenes/main_menu.owl", "fade_out", 0.3)
    end
end

function on_destroy()
    log.trace("WorldPlayer destroyed")
end
