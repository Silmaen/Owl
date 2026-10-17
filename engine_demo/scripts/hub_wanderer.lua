-- Wanderer of the hub: a villager pacing between two points of a street, stopping at each end.
-- Demonstrates: a plain Lua loop on the transform (no pathfinding, no physics), script properties set per entity in
--               the scene (`ax`, `ay`, `bx`, `by`, `speed`), a sprite mirrored toward its direction, a hop while walking.

properties = {
    { name = "ax", type = "float", default = 0.0 },
    { name = "ay", type = "float", default = 0.0 },
    { name = "bx", type = "float", default = 1.0 },
    { name = "by", type = "float", default = 0.0 },
    { name = "speed", type = "float", default = 1.2 },
}

local WAIT = 1.5
local sprite_scale = 1.0
local x, y = 0, 0
local to_b = true
local wait_left = 0
local clock = 0

function on_create()
    sprite_scale = transform.get_scale(entity_id)
    x, y = ax, ay
    transform.set_position(entity_id, x, y, 0.5)
end

function on_update(dt)
    local tx, ty = to_b and bx or ax, to_b and by or ay
    local moving = false
    if wait_left > 0 then
        wait_left = wait_left - dt
    else
        local dx, dy = tx - x, ty - y
        local dist = math.sqrt(dx * dx + dy * dy)
        local step = speed * dt
        if dist <= step then
            x, y = tx, ty
            to_b = not to_b
            wait_left = WAIT
        else
            x, y = x + dx / dist * step, y + dy / dist * step
            moving = true
            if dx ~= 0 then
                transform.set_scale(entity_id, dx < 0 and -sprite_scale or sprite_scale, sprite_scale, 1)
            end
        end
    end
    if moving then
        clock = clock + dt
    end
    transform.set_position(entity_id, x, y + (moving and math.abs(math.sin(clock * 9)) * 0.06 or 0), 0.5)
end
