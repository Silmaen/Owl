-- 2D player: the built-in `move` action (WASD or left stick, y down) slides the sprite across the screen.

properties = {
    {name = "speed", type = "float", default = 4.0},
}

function on_update(dt)
    local mx, my = input.get_action_vector("move")
    if mx == 0 and my == 0 then
        return
    end
    local x, y, z = transform.get_position(entity_id)
    transform.set_position(entity_id, x + mx * speed * dt, y - my * speed * dt, z)
end
