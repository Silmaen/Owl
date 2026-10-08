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
    local turning = 0
    if input.is_key_pressed(65) then turning = turning + 1 end -- A: turn left
    if input.is_key_pressed(68) then turning = turning - 1 end -- D: turn right
    if turning ~= 0 then
        rz = rz + turning * turn_speed * dt
        local px, py, _ = transform.get_position(entity_id)
        physics.set_transform(entity_id, px, py, rz)
    end
    local fx, fy = -math.sin(rz), math.cos(rz)
    local vx, vy = 0, 0
    if input.is_key_pressed(87) then vx = vx + fx * speed; vy = vy + fy * speed end -- W: forward
    if input.is_key_pressed(83) then vx = vx - fx * speed; vy = vy - fy * speed end -- S: back
    physics.set_velocity(entity_id, vx, vy)
end
