-- Vertical moving platform — Kinematic body driven by physics.set_velocity so
-- it pushes Dynamic bodies that stand on top.

properties = {
    { name = "speed",    type = "float", default = 2.0 },
    { name = "distance", type = "float", default = 4.0 },
}

local startY = 0
local direction = 1

function on_create()
    local _, y, _ = transform.get_position(entity_id)
    startY = y
end

function on_update(dt)
    local _, y, _ = transform.get_position(entity_id)
    if y > startY + distance then direction = -1 end
    if y < startY then direction = 1 end
    physics.set_velocity(entity_id, 0, speed * direction)
end
