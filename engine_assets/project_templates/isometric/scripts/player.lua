-- Isometric player: the built-in `move` action (WASD or left stick) moves along the screen axes, kept on the island.
-- Map coordinates: x runs to the lower right of the screen, y to the lower left.

properties = {
    {name = "speed", type = "float", default = 3.0},
    {name = "margin", type = "float", default = 1.5},
    {name = "size", type = "float", default = 12.0},
}

local diagonal = math.sqrt(0.5)

function on_update(dt)
    local mx, my = input.get_action_vector("move")
    if mx == 0 and my == 0 then
        return
    end
    local x, y, z = transform.get_position(entity_id)
    x = x + (mx + my) * diagonal * speed * dt
    y = y + (my - mx) * diagonal * speed * dt
    x = math.max(margin, math.min(size - margin, x))
    y = math.max(margin, math.min(size - margin, y))
    transform.set_position(entity_id, x, y, z)
end
