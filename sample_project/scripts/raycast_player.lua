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
--   GLFW ESC (256) = exit raycast scene

properties = {
    {name = "speed",      type = "float", default = 3.0},
    {name = "turn_speed", type = "float", default = 1.8},  -- rad/sec (~103 deg/s)
}

function on_create()
    -- Top-down: gravity must not pull the player along Y.
    physics.set_gravity_scale(entity_id, 0)
    -- Restore world-position if returning from a transition. Route through
    -- `physics.set_transform` so the Box2D body picks up the new pose;
    -- `transform.set_position/set_rotation` would be silently overwritten on
    -- the next physics step (the body is the source of truth).
    local rx = gamestate.get("raycast_return_x", -1000)
    if rx ~= -1000 then
        local ry = gamestate.get("raycast_return_y", 0.0)
        local rz = gamestate.get("raycast_return_z", 0.0)
        physics.set_transform(entity_id, rx, ry, rz)
        gamestate.remove("raycast_return_x")
        gamestate.remove("raycast_return_y")
        gamestate.remove("raycast_return_z")
    end
    log.trace("RaycastPlayer ready")
end

function on_update(dt)
    local _, _, rz = transform.get_rotation(entity_id)

    -- A / D rotate the player in place (GLFW-A = AZERTY 'Q' = turn left;
    -- GLFW-D = AZERTY 'D' = turn right). CCW is positive rz.
    -- Must go through `physics.set_transform` (not `transform.set_rotation`) —
    -- the Box2D body is the source of truth for entities with a `PhysicBody`,
    -- and it overwrites `Transform.rotation` after every physics step. Going
    -- through `b2Body_SetTransform` rotates the body itself, so the rotation
    -- survives the next sync.
    local turning = 0
    if input.is_key_pressed(65) then turning = turning + 1 end  -- AZERTY Q (turn left, CCW)
    if input.is_key_pressed(68) then turning = turning - 1 end  -- AZERTY D (turn right, CW)
    if turning ~= 0 then
        rz = rz + turning * turn_speed * dt
        local px, py, _ = transform.get_position(entity_id)
        physics.set_transform(entity_id, px, py, rz)
    end

    -- Forward / back (no strafing — classic Wolfenstein controls).
    local fx = -math.sin(rz)
    local fy =  math.cos(rz)

    local mvx, mvy = 0, 0
    if input.is_key_pressed(87) then mvx = mvx + fx * speed; mvy = mvy + fy * speed end -- AZERTY Z
    if input.is_key_pressed(83) then mvx = mvx - fx * speed; mvy = mvy - fy * speed end -- AZERTY S
    physics.set_velocity(entity_id, mvx, mvy)

    -- ESC returns to the world map and stashes our pose so we land back here.
    if input.is_key_pressed(256) then
        local px, py, _ = transform.get_position(entity_id)
        gamestate.set("raycast_return_x", px)
        gamestate.set("raycast_return_y", py)
        gamestate.set("raycast_return_z", rz)
        scene.load_scene("scenes/world_map.owl")
    end
end
