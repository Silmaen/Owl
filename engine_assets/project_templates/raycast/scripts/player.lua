-- Raycast player: W / S move forward and back, A / D turn (GLFW key codes, physical positions).
-- The Box2D body is the source of truth: move and turn it through `physics`, never `transform`.

properties = {
    {name = "speed",      type = "float", default = 3.0},
    {name = "turn_speed", type = "float", default = 1.8},
}

function on_create()
    -- Top-down world: gravity must not pull the player along Y.
    physics.set_gravity_scale(entity_id, 0)
end

function on_update(dt)
    local _, _, rz = transform.get_rotation(entity_id)
    -- Built-in `move` action (WASD or left stick, y down): x turns, y walks.
    local steer_x, steer_y = input.get_action_vector("move")
    local turning = -steer_x -- A: turn left
    if turning ~= 0 then
        rz = rz + turning * turn_speed * dt
        local px, py, _ = transform.get_position(entity_id)
        physics.set_transform(entity_id, px, py, rz)
    end
    local fx, fy = -math.sin(rz), math.cos(rz)
    local walk = -steer_y -- W: forward, S: back
    local vx, vy = fx * walk * speed, fy * walk * speed
    physics.set_velocity(entity_id, vx, vy)
end
