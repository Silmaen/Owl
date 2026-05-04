-- Horizontal moving platform — drives the body via physics.set_velocity so a
-- Kinematic body in Box2D pushes Dynamic bodies sitting on top (the player).
-- Direct transform.set_position would NOT push the player, since the engine's
-- Lua binding only mutates the Transform component, not the b2Body.

properties = {
    { name = "speed",    type = "float", default = 2.0 },
    { name = "distance", type = "float", default = 4.0 },
}

local startX = 0
local direction = 1

function on_create()
    local x, _, _ = transform.get_position(entity_id)
    startX = x
end

function on_update(dt)
    local x, _, _ = transform.get_position(entity_id)
    if x > startX + distance then direction = -1 end
    if x < startX then direction = 1 end
    physics.set_velocity(entity_id, speed * direction, 0)
end
