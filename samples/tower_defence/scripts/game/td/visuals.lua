-- Puffs, rings, fire and the bursts of the pooled particle emitters.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Cam, Vis = td.ui, td.camera, td.visuals

function Vis.spawn_puff(x, y, z, r, g, b, size, duration)
    local id = scene.spawn_prefab("prefabs/puff.owlprefab", Cam.billboard_at(x, y, z, 0.2))
    if id == 0 then return end
    entity.set_tint(id, r, g, b, 0.85)
    S.bursts[#S.bursts + 1] = { id = id, kind = "puff", t = 0.0, duration = duration, size = size, r = r, g = g, b = b }
end

function Vis.spawn_ring(x, y, z, radius, r, g, b)
    local id = scene.spawn_prefab("prefabs/shock_ring.owlprefab", x, y + 0.05, z)
    if id == 0 then return end
    transform.set_rotation(id, -math.pi / 2, 0.0, 0.0)
    S.bursts[#S.bursts + 1] = { id = id, kind = "ring", t = 0.0, duration = 0.4, size = radius * 2.0, r = r, g = g,
                                b = b }
end

function Vis.spawn_fire(x, y, z, radius)
    local id = scene.spawn_prefab("prefabs/fire.owlprefab", Cam.billboard_at(x, y, z, 0.28))
    if id == 0 then return end
    S.bursts[#S.bursts + 1] = { id = id, kind = "fire", t = 0.0, duration = fire_time, x = x, y = y, z = z,
                              radius = radius }
end

-- The pooled particle emitters of the level, `Fx<Kind>1`, `Fx<Kind>2`... (blood, blast, dust, embers, splinters, frost,
-- shield, rain), taken in turn: two bursts of a kind in a frame come from two emitters.
Vis.KINDS = { "Blood", "Blast", "Dust", "Embers", "Splinters", "Frost", "Shield", "Rain" }

Vis.pools, Vis.next = {}, {}

---@return nil
function Vis.find_pools()
    for _, kind in ipairs(Vis.KINDS) do
        local pool = {}
        for index = 1, 8 do
            local id = scene.find_entity("Fx" .. kind .. index)
            if id == 0 then break end
            pool[#pool + 1] = id
        end
        Vis.pools[kind], Vis.next[kind] = pool, 1
    end
end

-- A burst of `count` particles of a kind at a point.
function Vis.burst(kind, x, y, z, count)
    local pool = Vis.pools[kind]
    if pool == nil or #pool == 0 then return end
    local id = pool[Vis.next[kind]]
    Vis.next[kind] = Vis.next[kind] % #pool + 1
    transform.set_position(id, x, y, z)
    entity.particles_emit(id, count)
    S.bursts_fired = S.bursts_fired + 1
end

---@param dt any
function Vis.update_effects(dt)
    local index = 1
    while index <= #S.bursts do
        local effect = S.bursts[index]
        effect.t = effect.t + dt
        local t = math.min(1.0, effect.t / effect.duration)
        if effect.kind == "puff" then
            local size = effect.size * (0.5 + 0.7 * t)
            transform.set_scale(effect.id, size, size, 1.0)
            entity.set_tint(effect.id, effect.r, effect.g, effect.b, 0.85 * (1.0 - t))
        elseif effect.kind == "ring" then
            local size = effect.size * (0.3 + 0.9 * t)
            transform.set_scale(effect.id, size, size, 1.0)
            entity.set_tint(effect.id, effect.r, effect.g, effect.b, 0.9 * (1.0 - t))
        elseif effect.kind == "fire" then
            for _, foe in ipairs(S.enemies) do
                local dx, dz = foe.x - effect.x, foe.z - effect.z
                if dx * dx + dz * dz <= effect.radius * effect.radius then foe.burn = 0.5 end
            end
            local fade = t > 0.75 and (1.0 - t) * 4.0 or 1.0
            entity.set_tint(effect.id, 1.0, 1.0, 1.0, fade)
        end
        if t >= 1.0 then
            scene.destroy_entity(effect.id)
            table.remove(S.bursts, index)
        else
            index = index + 1
        end
    end
end

return Vis
