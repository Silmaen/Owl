-- Moving Platform
-- Demonstrates: transform get/set, time, properties, math library

properties = {
    { name = "speed",    type = "float", default = 2.0 },
    { name = "distance", type = "float", default = 4.0 },
}

local startY = 0
local direction = 1
local elapsed = 0

function on_create()
    local x, y, z = transform.get_position(entity_id)
    startY = y
    log.trace("Platform start Y=" .. startY .. " speed=" .. speed .. " distance=" .. distance)
end

function on_update(dt)
    local x, y, z = transform.get_position(entity_id)
    y = y + speed * direction * dt
    if y > startY + distance then
        direction = -1
    end
    if y < startY then
        direction = 1
    end
    transform.set_position(entity_id, x, y, z)

    -- Gentle rotation for visual effect
    elapsed = elapsed + dt
    local rx, ry, rz = transform.get_rotation(entity_id)
    transform.set_rotation(entity_id, rx, ry, math.sin(elapsed) * 0.05)
end
