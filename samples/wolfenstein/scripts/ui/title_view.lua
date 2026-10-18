-- The raycast view behind the title of Wolfenstein mini (script of the `TitleCamera` entity): the camera stands in the
-- courtyard of floor 3 and turns slowly, a sentry looking around. Nothing here reads the input.

properties = {
    { name = "turn_speed", type = "float", default = 0.12 }, -- radians per second
}

local yaw = 0

function on_create()
    local _, _, start_yaw = transform.get_rotation(entity_id)
    yaw = start_yaw
end

function on_update(dt)
    yaw = (yaw + (turn_speed or 0.12) * dt) % (2 * math.pi)
    transform.set_rotation(entity_id, 0, 0, yaw)
end
