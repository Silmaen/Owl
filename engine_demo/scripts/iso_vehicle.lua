-- Isometric vehicle: drives along the world X axis and wraps around at both ends.
-- Demonstrates: thousands-ready sprites of the isometric layer (compact instances), moved from a script.

properties = {
    { name = "speed", type = "float", default = 1.5 },
    { name = "min_x", type = "float", default = 0.0 },
    { name = "max_x", type = "float", default = 6.0 },
}

function on_update(dt)
    local x, y, z = transform.get_position(entity_id)
    x = x + speed * dt
    if speed >= 0 and x > max_x then
        x = min_x
    elseif speed < 0 and x < min_x then
        x = max_x
    end
    transform.set_position(entity_id, x, y, z)
end
