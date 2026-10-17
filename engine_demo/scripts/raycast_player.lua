-- Raycast player — Wolfenstein-like first-person controls (AZERTY-friendly).
-- Demonstrates: rotation in radians, forward/right vectors derived from yaw,
--               classic Wolfenstein layout (turn left/right + forward/back, no strafe),
--               ESC returns to the world map.
--
-- Convention (matches RendererRaycast extraction):
--   entity rotation [0,0,rz] in radians
--   forward = (-sin(rz),  cos(rz))     -- rz = 0 faces +Y
--
-- Key mapping (GLFW key codes — these refer to *physical* key positions, not
-- the printed layout, so on AZERTY the keys labelled Z/Q/S/D map to GLFW
-- W/A/S/D respectively — which is exactly what we want):
--
--   GLFW W (87)  = AZERTY 'Z' = forward
--   GLFW S (83)  = AZERTY 'S' = backward
--   GLFW A (65)  = AZERTY 'Q' = turn left  (rotate CCW)
--   GLFW D (68)  = AZERTY 'D' = turn right (rotate CW)
--   GLFW ESC (256) = back to the hub (pose kept); Tab / gamepad Y opens the inventory (inventory.lua)

properties = {
    {name = "speed",      type = "float", default = 3.0},
    {name = "turn_speed", type = "float", default = 1.8},  -- rad/sec (~103 deg/s)
}

-- The yaw lives here: the transform only follows the body on the fixed physics tick, so reading it back every frame
-- would drop or repeat a turn whenever frames and ticks do not line up.
local yaw = 0

function on_create()
    -- Top-down: gravity must not pull the player along Y.
    physics.set_gravity_scale(entity_id, 0)
    local _, _, start_yaw = transform.get_rotation(entity_id)
    yaw = start_yaw
    -- Restore world-position if returning from a transition. Route through
    -- `physics.set_transform` so the Box2D body picks up the new pose;
    -- `transform.set_position/set_rotation` would be silently overwritten on
    -- the next physics step (the body is the source of truth).
    local rx = gamestate.get("raycast_return_x", -1000)
    if rx ~= -1000 then
        local ry = gamestate.get("raycast_return_y", 0.0)
        local rz = gamestate.get("raycast_return_z", 0.0)
        physics.set_transform(entity_id, rx, ry, rz)
        yaw = rz
        gamestate.remove("raycast_return_x")
        gamestate.remove("raycast_return_y")
        gamestate.remove("raycast_return_z")
    end
    log.trace("RaycastPlayer ready")
end

function on_update(dt)
    -- Still while the inventory window is open, and while a scene transition runs.
    if gamestate.get("inv_window_open", 0) == 1 or ui.is_transition_active() then
        physics.set_velocity(entity_id, 0, 0)
        return
    end
    local rz = yaw

    -- A / D rotate the player in place (GLFW-A = AZERTY 'Q' = turn left;
    -- GLFW-D = AZERTY 'D' = turn right). CCW is positive rz.
    -- Must go through `physics.set_transform` (not `transform.set_rotation`) —
    -- the Box2D body is the source of truth for entities with a `PhysicBody`,
    -- and it overwrites `Transform.rotation` after every physics step. Going
    -- through `b2Body_SetTransform` rotates the body itself, so the rotation
    -- survives the next sync.
    local turning = input.get_action_value("turn")  -- A / right stick left: turn left (CCW)
    if turning ~= 0 then
        rz = (rz + turning * turn_speed * dt + math.pi) % (2 * math.pi) - math.pi
        yaw = rz
        local px, py, _ = transform.get_position(entity_id)
        physics.set_transform(entity_id, px, py, rz)
    end

    -- Forward / back (no strafing — classic Wolfenstein controls).
    local fx = -math.sin(rz)
    local fy =  math.cos(rz)

    local walk = input.get_action_value("walk")  -- W / left stick up: forward
    local mvx, mvy = fx * walk * speed, fy * walk * speed
    physics.set_velocity(entity_id, mvx, mvy)

    -- ESC returns to the world map and stashes our pose so we land back here.
    -- An Escape that has just closed the inventory window (ui_cancel) is not a pause.
    local closing = gamestate.get("inv_frame", 0) - gamestate.get("inv_closed_frame", -100) <= 1
    if input.is_action_just_pressed("pause") and not closing then
        local px, py, _ = transform.get_position(entity_id)
        gamestate.set("raycast_return_x", px)
        gamestate.set("raycast_return_y", py)
        gamestate.set("raycast_return_z", rz)
        scene.transition_to("scenes/world_map.owl", "wipe_right", 0.3)
    end
end
