-- World map player controller (top-down view).
-- Demonstrates: top-down 4-directional movement using physics.set_velocity
-- with gravity scale = 0 to opt out of the global Box2D gravity.
--
-- The world physic uses a downward gravity (-9.81 on Y). For top-down play we
-- set this body's gravity scale to 0 once at create time, so set_velocity is
-- the sole source of motion (no per-frame cancellation hack).

properties = {
    { name = "speed", type = "float", default = 5.0 },
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
-- town_atlas.png (the pack has none), played at k_step_time each while the player moves.
local k_frame_idle = 148
local k_frame_walk = 168
local k_walk_frames = 4
local k_step_time = 0.11
local walk_clock = 0
local shown_frame = -1

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

    -- Movement: the built-in `move` vector (WASD or left stick; y points down).
    local dirx, diry = input.get_action_vector("move")
    local mvx, mvy = dirx * speed, -diry * speed

    physics.set_velocity(entity_id, mvx, mvy)
    if sprite_id ~= 0 then
        if mvx ~= 0 then
            transform.set_scale(sprite_id, mvx < 0 and -sprite_scale or sprite_scale, sprite_scale, 1)
        end
        -- Walk cycle while moving, the idle frame when still.
        local frame = k_frame_idle
        if mvx ~= 0 or mvy ~= 0 then
            walk_clock = walk_clock + dt
            frame = k_frame_walk + math.floor(walk_clock / k_step_time) % k_walk_frames
        else
            walk_clock = 0
        end
        if frame ~= shown_frame then
            shown_frame = frame
            isometric.set_sprite_frame(sprite_id, frame)
        end
    end

    -- Camera always centred on the player. No clamping — if the player walks past the
    -- mountain border, we want to see the void rather than have the camera lag behind.
    -- The mountain colliders should keep the player inside the playable area regardless.
    if camera_id ~= 0 then
        local px, py, _ = transform.get_position(entity_id)
        local _, _, cz = transform.get_position(camera_id)
        transform.set_position(camera_id, px, py, cz)
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
