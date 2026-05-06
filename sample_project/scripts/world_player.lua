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
local escape_was_down = false
local teleporter_id = 0
local teleporter_visible = false
local health_bar_id = 0
local camera_id = 0

function on_create()
    log.info("WorldPlayer created with speed=" .. speed)
    -- Top-down: opt out of the world's downward gravity for this body.
    physics.set_gravity_scale(entity_id, 0.0)
    -- Initialise houses_total once per game (defaults to 1 in this sample).
    local total = gamestate.get("houses_total", 0)
    if total == 0 then
        gamestate.set("houses_total", 1)
        gamestate.set("houses_visited", 0)
    end

    -- Position restore priority: continue snapshot (ESC pause) wins over the
    -- normal "world_return" set by the door when coming back from a platformer.
    local cx = gamestate.get("continue_x", 0)
    local cy = gamestate.get("continue_y", 0)
    if cx ~= 0 or cy ~= 0 then
        local cz = gamestate.get("continue_z", 0)
        transform.set_position(entity_id, cx, cy, cz)
        log.info("Resumed world map at " .. cx .. ", " .. cy)
        gamestate.remove("continue_x")
        gamestate.remove("continue_y")
        gamestate.remove("continue_z")
        gamestate.remove("continue_scene")
    else
        local rx = gamestate.get("world_return_x", 0)
        local ry = gamestate.get("world_return_y", 0)
        if rx ~= 0 or ry ~= 0 then
            local _, _, pz = transform.get_position(entity_id)
            transform.set_position(entity_id, rx, ry, pz)
            log.info("Returned to world after platformer at " .. rx .. ", " .. ry)
            gamestate.remove("world_return_x")
            gamestate.remove("world_return_y")
        end
    end

    teleporter_id = scene.find_entity("Teleporter")
    health_bar_id = scene.find_entity("HealthBar")
    camera_id = scene.find_entity("MainCamera")
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

    -- Movement (WASD: W=87, A=65, S=83, D=68)
    local mvx, mvy = 0, 0
    if input.is_key_pressed(87) then mvy = mvy + speed end
    if input.is_key_pressed(83) then mvy = mvy - speed end
    if input.is_key_pressed(65) then mvx = mvx - speed end
    if input.is_key_pressed(68) then mvx = mvx + speed end

    physics.set_velocity(entity_id, mvx, mvy)

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

    -- Reveal the teleporter once every house has been explored.
    if teleporter_id ~= 0 and not teleporter_visible then
        local visited = gamestate.get("houses_visited", 0)
        local total = gamestate.get("houses_total", 1)
        if visited >= total then
            ui.set_visible(teleporter_id, true)
            teleporter_visible = true
            log.info("All houses explored — teleporter has appeared!")
        end
    end

    -- Escape returns to the main menu, preserving the world position so the
    -- "Continuer" button on the menu can resume exactly where we left off.
    local escape_down = input.is_key_pressed(256)
    if escape_down and not escape_was_down then
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
    escape_was_down = escape_down
end

function on_destroy()
    log.trace("WorldPlayer destroyed")
end
