-- Torch and lamp flicker: tints the sprite of the entity with a warm, uneven pulse (two sines of unrelated rates).
-- `rate` is the base speed in rad/s, `phase` offsets the torches from each other.

properties = {
    { name = "rate",  type = "float", default = 7.0 },
    { name = "phase", type = "float", default = 0.0 },
}

local clock = 0.0

function on_update(dt)
    clock = clock + dt
    local t = clock * rate + phase
    local glow = 0.5 + 0.5 * math.sin(t) * math.sin(t * 1.73 + 1.3)
    entity.set_tint(entity_id, 1.0, 0.72 + 0.28 * glow, 0.38 + 0.5 * glow, 1.0)
end
